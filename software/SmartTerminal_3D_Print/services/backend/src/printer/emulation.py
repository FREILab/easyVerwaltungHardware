from __future__ import annotations

import logging
import re
from pathlib import Path
from typing import Any

from fastapi import APIRouter, File, Form, HTTPException, Request, UploadFile
from fastapi.responses import JSONResponse

from jobs.parser import compute_job_price, parse_print_file

logger = logging.getLogger("st3d.emulation")
router = APIRouter(tags=["prusalink-emulation"])

_SAFE_NAME_RE = re.compile(r"[^A-Za-z0-9._-]+")


def _sanitize_filename(raw: str) -> str:
    base = Path(raw).name.strip() or "upload.gcode"
    cleaned = _SAFE_NAME_RE.sub("_", base)
    if not cleaned.lower().endswith((".gcode", ".gco", ".bgcode", ".g")):
        cleaned += ".gcode"
    return cleaned


def _resolve_slot(request: Request) -> int:
    raw = request.headers.get("X-Printer-Slot", "1").strip()
    try:
        return max(1, int(raw))
    except ValueError:
        return 1


def _check_slicer_auth(request: Request) -> None:
    expected = request.app.state.settings.slicer_api_key
    if not expected:
        return
    api_key = request.headers.get("X-Api-Key", "").strip()
    if api_key == expected:
        return
    raise HTTPException(status_code=401, detail="Invalid or missing X-Api-Key")


def _parse_print_after_upload(raw: str | None, default: bool = False) -> bool:
    if raw is None:
        return default
    v = raw.strip().lower()
    if v in {"?1", "1", "true", "yes", "on"}:
        return True
    if v in {"?0", "0", "false", "no", "off"}:
        return False
    return default


async def _ingest_print_file(
    request: Request, filename: str, temp_path: Path, size: int, *, print_after_upload: bool = True
) -> dict[str, Any]:
    app_state = request.app.state
    settings = app_state.settings
    store = app_state.store
    registry = app_state.registry

    slot = _resolve_slot(request)
    printer = registry.get_by_slot(slot)
    if printer is None:
        printer_id = f"prusa-{slot:02d}"
        printer_name = f"Drucker {slot}"
    else:
        printer_id = printer.id
        printer_name = printer.name

    safe_name = _sanitize_filename(filename)
    meta = parse_print_file(temp_path)
    pricing = compute_job_price(
        meta,
        default_price_per_gram_cents=settings.default_price_per_gram_cents,
        min_price_per_gram_cents=settings.min_price_per_gram_cents,
        min_job_price_cents=settings.min_job_price_cents,
    )

    job_id = temp_path.stem
    thumb_path: Path | None = None
    thumb_ext: str | None = None
    if meta.thumbnail_bytes:
        thumb_ext = meta.thumbnail_ext
        thumb_path = store.thumbs_dir / f"{job_id}.{thumb_ext}"
        thumb_path.write_bytes(meta.thumbnail_bytes)

    job = store.add_job(
        job_id=job_id,
        printer_slot=slot,
        printer_id=printer_id,
        printer_name=printer_name,
        filename=safe_name,
        file_path=temp_path,
        file_size=size,
        thumbnail_path=thumb_path,
        thumbnail_ext=thumb_ext,
        filament_g=meta.filament_g,
        filament_type=meta.filament_type,
        estimated_time_s=meta.estimated_time_s,
        price_cents=pricing.price_cents,
        slicer_price_cents=pricing.slicer_price_cents,
        price_source=pricing.price_source,
        ttl_seconds=settings.job_ttl_seconds,
        print_after_upload=print_after_upload,
    )
    app_state.events.publish(
        "queue_updated",
        {"action": "uploaded", "jobId": job["id"], "slot": slot, "autoRfid": True},
    )
    # Wake backlight & trigger friendly soft attention ping + RFID LED ring
    import asyncio
    try:
        loop = asyncio.get_running_loop()
        loop.create_task(app_state.device_client.send_beeper("attention"))
        loop.create_task(app_state.device_client.send_led("rfid"))
    except RuntimeError:
        pass
    logger.info(
        "Queued upload %s for slot %d (%s): %.1fg, %ds, %d ct (%s)",
        safe_name,
        slot,
        printer_id,
        meta.filament_g,
        meta.estimated_time_s,
        pricing.price_cents,
        pricing.price_source,
    )
    return job


@router.get("/api/version")
def prusalink_version(request: Request) -> dict[str, Any]:
    _check_slicer_auth(request)
    slot = _resolve_slot(request)
    printer = request.app.state.registry.get_by_slot(slot)
    name = printer.name if printer else f"SmartTerminal Slot {slot}"
    return {
        "api": "2.0.0",
        "server": "2.1.2",
        "original": "0.7.0",
        "text": f"PrusaLink SmartTerminal ({name})",
        "hostname": f"{request.app.state.settings.terminal_id}-slot{slot}",
        "capabilities": {
            "upload-by-put": True,
        },
    }


@router.get("/api/v1/health")
def printer_emulation_health(request: Request) -> dict[str, Any]:
    return {
        "status": "ok",
        "service": "printer-emulation",
        "slot": _resolve_slot(request),
        "mode": request.app.state.settings.printer_mode,
    }


@router.get("/api/v1/info")
def prusalink_info(request: Request) -> dict[str, Any]:
    _check_slicer_auth(request)
    slot = _resolve_slot(request)
    printer = request.app.state.registry.get_by_slot(slot)
    return {
        "name": printer.name if printer else f"SmartTerminal Slot {slot}",
        "hostname": f"{request.app.state.settings.terminal_id}-slot{slot}",
        "serial": f"ST3D-{request.app.state.settings.terminal_id}-{slot:02d}",
        "nozzle_diameter": 0.4,
        "mmu": False,
        "min_extrusion_temp": 170,
    }


@router.get("/api/v1/status")
def prusalink_status(request: Request) -> dict[str, Any]:
    _check_slicer_auth(request)
    # Always report IDLE to the slicer so uploads land cleanly in the staging queue
    return {
        "printer": {
            "state": "IDLE",
            "temp_bed": 22.0,
            "target_bed": 0.0,
            "temp_nozzle": 24.0,
            "target_nozzle": 0.0,
            "axis_z": 0.0,
            "flow": 100,
            "speed": 100,
        },
        "storage": {
            "path": "/usb/",
            "name": "usb",
            "read_only": False,
        },
    }


@router.get("/api/v1/storage")
def prusalink_storage(request: Request) -> dict[str, Any]:
    _check_slicer_auth(request)
    return {
        "storage_list": [
            {
                "path": "/usb/",
                "name": "usb",
                "type": "USB",
                "read_only": False,
                "available": True,
                "free_space": 4 * 1024 * 1024 * 1024,
            }
        ]
    }


@router.head("/api/v1/files/{storage}/{filepath:path}")
def prusalink_v1_file_exists(storage: str, filepath: str, request: Request) -> JSONResponse:
    """PrusaSlicer checks via HEAD whether target file already exists before PUT."""
    _check_slicer_auth(request)
    return JSONResponse(status_code=404, content={"detail": "Not Found"})


@router.put("/api/v1/files/{storage}/{filepath:path}")
async def prusalink_v1_upload(storage: str, filepath: str, request: Request) -> JSONResponse:
    """PrusaLink API v1 binary/text file upload used by PrusaSlicer (MINI, MK3.5, MK4, XL)."""
    _check_slicer_auth(request)
    settings = request.app.state.settings
    store = request.app.state.store

    safe_name = _sanitize_filename(filepath)
    ext = Path(safe_name).suffix.lower() or ".gcode"
    job_id = store.new_job_id()
    dest = store.uploads_dir / f"{job_id}{ext}"

    total = 0
    try:
        with dest.open("wb") as out:
            async for chunk in request.stream():
                total += len(chunk)
                if total > settings.max_upload_bytes:
                    raise HTTPException(status_code=413, detail="Upload exceeds MAX_UPLOAD_MB")
                out.write(chunk)
        if total == 0:
            raise HTTPException(status_code=400, detail="Empty upload body")
    except Exception:
        dest.unlink(missing_ok=True)
        raise

    print_after = _parse_print_after_upload(request.headers.get("Print-After-Upload"), default=False)
    job = await _ingest_print_file(request, safe_name, dest, total, print_after_upload=print_after)
    return JSONResponse(
        status_code=201,
        content={
            "name": job["filename"],
            "path": f"/{storage}/{job['filename']}",
            "size": total,
            "queued": True,
            "jobId": job["id"],
        },
    )


@router.post("/api/files/{storage}")
@router.post("/api/files/{storage}/")
async def prusalink_multipart_upload(
    storage: str,
    request: Request,
    file: UploadFile = File(...),
    print_flag: str | None = Form(default=None, alias="print"),
) -> JSONResponse:
    """Multipart upload used by PrusaSlicer (/api/files/local, /api/files/usb/, /api/files/sdcard/)."""
    _check_slicer_auth(request)
    settings = request.app.state.settings
    store = request.app.state.store

    safe_name = _sanitize_filename(file.filename or "upload.gcode")
    ext = Path(safe_name).suffix.lower() or ".gcode"
    job_id = store.new_job_id()
    dest = store.uploads_dir / f"{job_id}{ext}"

    total = 0
    try:
        with dest.open("wb") as out:
            while True:
                chunk = await file.read(1024 * 1024)
                if not chunk:
                    break
                total += len(chunk)
                if total > settings.max_upload_bytes:
                    raise HTTPException(status_code=413, detail="Upload exceeds MAX_UPLOAD_MB")
                out.write(chunk)
        if total == 0:
            raise HTTPException(status_code=400, detail="Empty file")
    except Exception:
        dest.unlink(missing_ok=True)
        raise
    finally:
        await file.close()

    raw_flag = print_flag if print_flag is not None else request.headers.get("Print-After-Upload")
    print_after = _parse_print_after_upload(raw_flag, default=False)
    job = await _ingest_print_file(request, safe_name, dest, total, print_after_upload=print_after)
    return JSONResponse(
        status_code=201,
        content={
            "files": {storage: {"name": job["filename"], "origin": storage}},
            "done": True,
            "queued": True,
            "jobId": job["id"],
        },
    )
