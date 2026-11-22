from __future__ import annotations

import asyncio
import base64
import json
import logging
import os
import socket
import time
import uuid
from pathlib import Path
from typing import Any

import httpx
from fastapi import APIRouter, HTTPException, Request
from fastapi.responses import FileResponse, StreamingResponse
from pydantic import BaseModel, Field

from jobs.parser import compute_job_price, parse_print_file
from payment.service import TerminalSession
from printer.client import MockPrinterClient

logger = logging.getLogger("st3d.api")
router = APIRouter(tags=["ui-api"])


class AuthorizeRequest(BaseModel):
    rfid: str = Field(..., min_length=2, max_length=64)
    pin: str | None = None


class StartJobRequest(BaseModel):
    sessionId: str
    mode: str = "personal"  # "personal" | "club" | "already_paid"
    pin: str | None = None
    sectionId: str | None = None
    allowOverdraft: bool = False


class TopUpRequest(BaseModel):
    sessionId: str
    amountCents: int = Field(..., gt=0, le=50000)


class ApproveRequest(BaseModel):
    sessionId: str
    instructorRfid: str = Field(..., min_length=2, max_length=64)


class DevSimulateRfidRequest(BaseModel):
    uid: str


class DevPrinterStateRequest(BaseModel):
    printerId: str
    state: str


class DevUploadSampleRequest(BaseModel):
    slot: int = 1
    filename: str = "3DBenchy_0.2mm_PETG_MK4S.bgcode"
    filamentGrams: float = 14.2
    filamentType: str = "PETG"
    estimatedTimeSeconds: int = 2820
    slicerCostEur: float = 0.57


class AdminConfigUpdate(BaseModel):
    terminalId: str = Field(..., min_length=1, max_length=64)
    easyverwaltungUrl: str = Field(default="", max_length=256)
    easyverwaltungServiceId: str = Field(default="", max_length=64)
    easyverwaltungServiceToken: str | None = Field(default=None, max_length=256)


class PrinterPowerOnRequest(BaseModel):
    sessionId: str


class DevPrinterTempRequest(BaseModel):
    printerId: str
    nozzleTemp: float | None = None  # None clears override


def _detect_local_ip() -> str:
    override = os.getenv("TERMINAL_IP", "").strip()
    if override:
        return override
    try:
        s = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
        s.connect(("8.8.8.8", 80))
        ip = s.getsockname()[0]
        s.close()
        return ip
    except OSError:
        return "localhost"


def _load_admin_overrides(data_dir: Path) -> dict[str, Any]:
    path = data_dir / "admin_config.json"
    if not path.exists():
        return {}
    try:
        return json.loads(path.read_text(encoding="utf-8"))
    except Exception:
        return {}


def _save_admin_overrides(data_dir: Path, payload: dict[str, Any]) -> None:
    path = data_dir / "admin_config.json"
    path.write_text(json.dumps(payload, indent=2, ensure_ascii=False), encoding="utf-8")


def _get_session(request: Request, session_id: str) -> TerminalSession:
    sessions: dict[str, TerminalSession] = request.app.state.sessions
    sess = sessions.get(session_id)
    if not sess or sess.is_expired:
        sessions.pop(session_id, None)
        raise HTTPException(status_code=401, detail={"code": "session_expired", "message": "Sitzung abgelaufen. Bitte Karte erneut vorhalten."})
    return sess


@router.get("/health")
async def health(request: Request) -> dict[str, Any]:
    st = request.app.state
    return {
        "status": "ok",
        "service": "backend",
        "terminalId": st.settings.terminal_id,
        "dbOk": st.store.health_ok(),
        "deviceAgentConnected": st.device_client.connected,
        "printersConfigured": len(st.registry.list_printers()),
        "modes": {
            "hardware": st.settings.hardware_mode,
            "payment": st.settings.payment_mode,
            "printer": st.settings.printer_mode,
            "devMock": st.settings.is_dev_mock,
        },
        "features": {
            "topup": st.settings.feature_topup or st.settings.payment_mode == "mock",
            "approval": st.settings.feature_approval or st.settings.payment_mode == "mock",
            "allowNegativeBalance": st.settings.allow_negative_balance,
            "maxNegativeBalanceCents": st.settings.max_negative_balance_cents,
        },
    }


@router.get("/state")
async def terminal_state(request: Request) -> dict[str, Any]:
    st = request.app.state
    meta = st.cached_metadata or {}
    uploads_by_printer: dict[str, dict[str, Any]] = st.active_uploads
    uploads_by_job = {u["jobId"]: u for u in uploads_by_printer.values()}

    printers = []
    lifetime_stats = st.store.printer_lifetime_stats()
    latest_jobs = st.store.latest_started_jobs_by_printer()
    now_ts = time.time()
    for p in st.registry.serialize(lifetime_stats):
        up = uploads_by_printer.get(p["id"])
        last = latest_jobs.get(p["id"])
        if up:
            p = {
                **p,
                "state": "UPLOADING",
                "canAcceptJob": False,
                "progress": up["percent"],
                "uploadProgress": up,
                "activeJob": up.get("displayName") or p.get("activeJob"),
                "activeJobOwner": up.get("userDisplayName") or None,
                "activeJobSection": up.get("sectionName") or None,
                "timeRemainingSeconds": up.get("estimatedTimeSeconds") or p.get("timeRemainingSeconds"),
            }
        elif p["state"] in {"PRINTING", "PAUSED"}:
            owner = last["userDisplayName"] if last and last.get("userDisplayName") else None
            sec_name = last.get("sectionName") if last else None
            rem_s = p.get("timeRemainingSeconds")
            if rem_s is None and last and last.get("estimatedTimeSeconds") and last.get("startedAt"):
                elapsed = max(0, int(now_ts - last["startedAt"]))
                rem_s = max(0, last["estimatedTimeSeconds"] - elapsed)
            p = {
                **p,
                "activeJob": p.get("activeJob") or (last["displayName"] if last else None),
                "activeJobOwner": owner,
                "activeJobSection": sec_name,
                "timeRemainingSeconds": rem_s,
            }
        elif p["state"] == "ATTENTION" and last and last.get("userDisplayName"):
            p = {
                **p,
                "activeJobOwner": last["userDisplayName"],
                "activeJobSection": last.get("sectionName"),
            }
        printers.append(p)

    jobs = []
    for j in st.store.list_queued_jobs():
        up = uploads_by_job.get(j["id"])
        if up:
            j = {**j, "uploadProgress": up}
        jobs.append(j)

    overrides = _load_admin_overrides(st.settings.data_dir)
    return {
        "terminalId": overrides.get("terminalId") or st.settings.terminal_id,
        "terminalIp": _detect_local_ip(),
        "version": "0.3.0",
        "serviceName": meta.get("service", {}).get("name", "SmartTerminal 3D-Print"),
        "printers": printers,
        "jobs": jobs,
        "sections": meta.get("sections", []),
        "modes": {
            "hardware": st.settings.hardware_mode,
            "payment": st.settings.payment_mode,
            "printer": st.settings.printer_mode,
            "devMock": st.settings.is_dev_mock,
        },
        "features": {
            "topup": st.settings.feature_topup or st.settings.payment_mode == "mock",
            "approval": st.settings.feature_approval or st.settings.payment_mode == "mock",
            "allowNegativeBalance": st.settings.allow_negative_balance,
            "maxNegativeBalanceCents": st.settings.max_negative_balance_cents,
        },
        "serverTime": time.time(),
    }


@router.get("/events")
async def event_stream(request: Request) -> StreamingResponse:
    return StreamingResponse(
        request.app.state.events.subscribe(request),
        media_type="text/event-stream",
        headers={"Cache-Control": "no-cache", "X-Accel-Buffering": "no"},
    )


@router.get("/jobs")
def list_jobs(request: Request) -> dict[str, Any]:
    return {"items": request.app.state.store.list_queued_jobs()}


@router.get("/jobs/{job_id}/thumbnail")
def job_thumbnail(job_id: str, request: Request) -> FileResponse:
    job = request.app.state.store.get_job(job_id)
    if not job or not job.get("thumbnailPath"):
        raise HTTPException(status_code=404, detail="No thumbnail")
    path = Path(job["thumbnailPath"])
    if not path.exists():
        raise HTTPException(status_code=404, detail="Thumbnail missing")
    media = "image/jpeg" if job.get("thumbnailExt") == "jpg" else "image/png"
    return FileResponse(path, media_type=media)


@router.delete("/jobs/{job_id}")
async def delete_job(job_id: str, request: Request) -> dict[str, Any]:
    st = request.app.state
    task = st.upload_tasks_by_job.pop(job_id, None)
    was_uploading = False
    if task and not task.done():
        was_uploading = True
        task.cancel()
        try:
            await asyncio.wait_for(asyncio.shield(task), timeout=2.0)
        except (asyncio.CancelledError, asyncio.TimeoutError, Exception):
            pass

    # Clear active upload entry for this job if still present
    for pid, info in list(st.active_uploads.items()):
        if info.get("jobId") == job_id:
            st.active_uploads.pop(pid, None)
            st.events.publish("printers_updated", {"printerId": pid})

    ok = st.store.cancel_job(job_id)
    if not ok and not was_uploading:
        raise HTTPException(status_code=404, detail="Job not found or already started")
    if was_uploading:
        await st.device_client.send_led("idle")
        st.store.log_event("upload_cancelled", job_id=job_id)
    st.events.publish("queue_updated", {"action": "cancelled", "jobId": job_id})
    return {"deleted": True, "jobId": job_id, "wasUploading": was_uploading}


@router.post("/session/authorize")
async def authorize_session(body: AuthorizeRequest, request: Request) -> dict[str, Any]:
    st = request.app.state
    mode = "rfid_pin" if body.pin else "rfid"
    res = await st.payment.authorize(mode=mode, rfid=body.rfid, pin=body.pin)
    if not res.get("authorized"):
        await st.device_client.send_beeper("error")
        await st.device_client.send_led("error")
        st.store.log_event("payment_auth_denied", details={"reason": res.get("reason", "DENIED")})
        return {
            "authorized": False,
            "reason": res.get("reason", "DENIED"),
        }

    session_id = f"sess_{uuid.uuid4().hex[:12]}"
    sess = TerminalSession(
        session_id=session_id,
        payment_authorization_id=res["paymentAuthorizationId"],
        rfid_uid=body.rfid.strip().upper(),
        user=res["user"],
        balance_cents=int(res["balance"]["amount"]),
        currency=res["balance"].get("currency", "EUR"),
        approved_for_3d_print=bool(res.get("approved", True)),
        is_instructor=bool(res.get("is_instructor", False)),
        expires_at=time.time() + int(res.get("expiresInSeconds", 300)),
        transactions=res.get("transactions", []),
    )
    st.sessions[session_id] = sess
    await st.device_client.send_beeper("ok")
    await st.device_client.send_led("success")
    st.store.log_event(
        "payment_authorized",
        user_id=sess.user.get("user_id"),
        user_name=sess.user.get("display_name"),
        details={"approved": sess.approved_for_3d_print},
    )
    return {"authorized": True, "session": sess.public_dict()}


@router.post("/session/cancel")
async def cancel_session(request: Request, sessionId: str | None = None) -> dict[str, Any]:
    st = request.app.state
    if sessionId and sessionId in st.sessions:
        sess = st.sessions.pop(sessionId)
        await st.payment.cancel_authorization(
            payment_authorization_id=sess.payment_authorization_id,
            reason="USER_CANCELLED",
        )
    await st.device_client.send_led("idle")
    return {"cancelled": True}


@router.post("/session/topup")
async def topup_session(body: TopUpRequest, request: Request) -> dict[str, Any]:
    st = request.app.state
    sess = _get_session(request, body.sessionId)
    if st.settings.payment_mode != "mock" and not st.settings.feature_topup:
        raise HTTPException(
            status_code=501,
            detail={"code": "topup_not_available", "message": "Guthaben aufladen ist in easyVerwaltung noch nicht freigeschaltet."},
        )
    res = await st.payment.topup_mock(rfid=sess.rfid_uid, amount_cents=body.amountCents)
    if not res.get("ok"):
        raise HTTPException(status_code=400, detail={"code": "topup_failed", "message": "Aufladung fehlgeschlagen."})
    sess.balance_cents = int(res["balanceCents"])
    sess.transactions = res.get("transactions", sess.transactions)
    await st.device_client.send_beeper("ok")
    st.store.log_event(
        "balance_topup",
        user_id=sess.user.get("user_id"),
        user_name=sess.user.get("display_name"),
        amount_cents=body.amountCents,
    )
    return {"ok": True, "session": sess.public_dict()}


@router.post("/session/approve")
async def approve_session(body: ApproveRequest, request: Request) -> dict[str, Any]:
    st = request.app.state
    sess = _get_session(request, body.sessionId)
    if st.settings.payment_mode != "mock" and not st.settings.feature_approval:
        raise HTTPException(
            status_code=501,
            detail={"code": "approval_not_available", "message": "Freigabe vor Ort ist in easyVerwaltung noch nicht freigeschaltet."},
        )
    res = await st.payment.approve_user_mock(target_rfid=sess.rfid_uid, instructor_rfid=body.instructorRfid)
    if not res.get("ok"):
        await st.device_client.send_beeper("error")
        return {"approved": False, "reason": res.get("reason", "NOT_AUTHORIZED_INSTRUCTOR")}
    sess.approved_for_3d_print = True
    await st.device_client.send_beeper("ok")
    st.store.log_event(
        "user_approved",
        user_id=sess.user.get("user_id"),
        user_name=sess.user.get("display_name"),
        details={"instructor": res.get("instructorName")},
    )
    return {"approved": True, "session": sess.public_dict(), "instructorName": res.get("instructorName")}


async def _run_printer_transfer(
    app_state: Any,
    *,
    job: dict[str, Any],
    printer: Any,
    mode: str,
    payment_ref: str | None,
    charged_cents: int,
    user_id: str | None,
    user_display_name: str,
    section_id: str | None,
    section_name: str | None,
) -> None:
    """Stream file to printer and start print asynchronously so slow uploads never time out HTTP."""
    job_id = job["id"]
    total_bytes = int(job.get("fileSize") or 0)
    started_mono = time.monotonic()

    def _emit_upload_progress(sent: int, total: int) -> None:
        pct = min(100, max(0, int(sent * 100 / total))) if total > 0 else 0
        elapsed = max(0.2, time.monotonic() - started_mono)
        bps = int(sent / elapsed)
        prog = {
            "jobId": job_id,
            "printerId": printer.id,
            "displayName": job.get("displayName", job["filename"]),
            "userDisplayName": user_display_name,
            "sectionName": section_name,
            "estimatedTimeSeconds": int(job.get("estimatedTimeSeconds") or 0),
            "stage": "uploading",
            "percent": pct,
            "sentBytes": sent,
            "totalBytes": total,
            "bytesPerSec": bps,
        }
        app_state.active_uploads[printer.id] = prog
        app_state.events.publish("job_progress", prog)

    try:
        should_start = bool(job.get("printAfterUpload", True))
        _emit_upload_progress(0, total_bytes)
        await app_state.device_client.send_led("sending")
        upload_res = await app_state.printer_client.upload_file(
            printer,
            Path(job["filePath"]),
            job["filename"],
            print_after_upload=should_start,
            on_progress=_emit_upload_progress,
        )
        if not upload_res.ok:
            reason = upload_res.reason or "UPLOAD_FAILED"
            logger.error("Background upload failed for %s on %s: %s", job_id, printer.id, reason)
            app_state.store.mark_job_failed(job_id)
            app_state.store.log_event(
                "print_transfer_failed",
                job_id=job_id,
                printer_id=printer.id,
                user_id=user_id,
                user_name=user_display_name,
                amount_cents=charged_cents,
                details={"reason": reason, "paymentRef": payment_ref, "mode": mode},
            )
            await app_state.device_client.send_beeper("error")
            await app_state.device_client.send_led("error")
            app_state.events.publish(
                "job_failed",
                {
                    "jobId": job_id,
                    "printerName": printer.name,
                    "displayName": job.get("displayName", job["filename"]),
                    "reason": reason,
                    "charged": charged_cents > 0,
                },
            )
            return

        app_state.store.mark_job_started(
            job_id,
            payment_mode=mode,
            payment_ref=payment_ref,
            user_display_name=user_display_name,
            section_id=section_id,
            section_name=section_name,
        )
        app_state.store.log_event(
            "print_started" if should_start else "file_uploaded_only",
            job_id=job_id,
            printer_id=printer.id,
            user_id=user_id,
            user_name=user_display_name,
            amount_cents=charged_cents,
            details={"mode": mode, "paymentRef": payment_ref, "sectionName": section_name, "printAfterUpload": should_start},
        )
        new_status = await app_state.printer_client.poll_status(printer)
        app_state.registry.set_status(printer.id, new_status)
        await app_state.device_client.send_beeper("ok")
        await app_state.device_client.send_led("idle")
        app_state.events.publish(
            "job_done",
            {
                "jobId": job_id,
                "printerName": printer.name,
                "printerSlot": printer.slot,
                "displayName": job.get("displayName", job["filename"]),
                "mode": mode,
                "chargedCents": charged_cents,
                "sectionName": section_name,
                "printAfterUpload": should_start,
            },
        )
    except asyncio.CancelledError:
        logger.info("Background transfer cancelled for %s", job_id)
        app_state.store.cancel_job(job_id)
        raise
    except Exception as exc:
        logger.exception("Unexpected error during background transfer for %s: %s", job_id, exc)
        app_state.store.mark_job_failed(job_id)
        app_state.events.publish(
            "job_failed",
            {"jobId": job_id, "printerName": printer.name, "reason": "TRANSFER_ERROR", "charged": charged_cents > 0},
        )
    finally:
        app_state.upload_tasks_by_job.pop(job_id, None)
        app_state.active_uploads.pop(printer.id, None)
        app_state.events.publish("queue_updated", {"action": "transfer_finished", "jobId": job_id})
        app_state.events.publish("printers_updated", {"printerId": printer.id})


@router.post("/jobs/{job_id}/start")
async def start_job(job_id: str, body: StartJobRequest, request: Request) -> dict[str, Any]:
    st = request.app.state
    sess = _get_session(request, body.sessionId)

    if not sess.approved_for_3d_print:
        return {"started": False, "reason": "NOT_APPROVED"}

    job = st.store.get_job(job_id)
    if not job or job["status"] != "queued" or job["remainingSeconds"] <= 0:
        return {"started": False, "reason": "JOB_EXPIRED"}

    printer = st.registry.get_by_slot(job["printerSlot"]) or st.registry.get_by_id(job["printerId"])
    if not printer:
        return {"started": False, "reason": "PRINTER_NOT_FOUND"}

    if printer.id in st.active_uploads:
        await st.device_client.send_beeper("error")
        return {"started": False, "reason": "PRINTER_BUSY"}

    mode = body.mode.strip().lower()
    if mode not in {"personal", "club", "already_paid"}:
        raise HTTPException(status_code=400, detail="Invalid start mode")

    # PIN verification for club print and already-paid reprint
    section_name: str | None = None
    if mode in {"club", "already_paid"}:
        if not body.pin:
            return {"started": False, "reason": "PIN_REQUIRED"}
        pin_check = await st.payment.authorize(mode="rfid_pin", rfid=sess.rfid_uid, pin=body.pin)
        if not pin_check.get("authorized"):
            await st.device_client.send_beeper("error")
            return {"started": False, "reason": pin_check.get("reason", "PIN_INVALID")}
        if pin_check.get("paymentAuthorizationId"):
            await st.payment.cancel_authorization(
                payment_authorization_id=pin_check["paymentAuthorizationId"],
                reason=f"PIN_VERIFY_{mode.upper()}",
            )
        if mode == "club":
            sections = (st.cached_metadata or {}).get("sections", [])
            matched = next((s for s in sections if s["sectionId"] == body.sectionId), None)
            if not matched:
                return {"started": False, "reason": "SECTION_REQUIRED"}
            section_name = matched["name"]

    # Step 1: Check live printer status BEFORE charging
    live_status = await st.printer_client.poll_status(printer)
    st.registry.set_status(printer.id, live_status)
    if live_status.state == "OFFLINE":
        await st.device_client.send_beeper("error")
        return {"started": False, "reason": "PRINTER_OFFLINE"}
    if not live_status.can_accept_job:
        await st.device_client.send_beeper("error")
        return {"started": False, "reason": "PRINTER_BUSY"}

    # Step 2: Balance check & immediate charge (before slow upload so 300s auth never expires!)
    allow_overdraft = bool(body.allowOverdraft and st.settings.allow_negative_balance)
    if mode == "personal" and sess.balance_cents < job["priceCents"]:
        projected = sess.balance_cents - job["priceCents"]
        if not allow_overdraft:
            return {
                "started": False,
                "reason": "INSUFFICIENT_BALANCE",
                "balanceCents": sess.balance_cents,
                "missingCents": job["priceCents"] - sess.balance_cents,
            }
        if st.settings.max_negative_balance_cents > 0 and projected < -st.settings.max_negative_balance_cents:
            return {
                "started": False,
                "reason": "OVERDRAFT_LIMIT_EXCEEDED",
                "balanceCents": sess.balance_cents,
                "maxNegativeBalanceCents": st.settings.max_negative_balance_cents,
            }

    payment_ref: str | None = None
    charged_cents = 0
    if mode == "personal":
        idem_suffix = "-ovd" if allow_overdraft else ""
        idem_key = f"st3d-{job_id}-{sess.payment_authorization_id}{idem_suffix}"
        corr_id = f"{st.settings.terminal_id}-{job_id}"
        charge_res = await st.payment.charge(
            payment_authorization_id=sess.payment_authorization_id,
            amount_cents=job["priceCents"],
            items=[
                {
                    "name": f"3D-Druck: {job['displayName'][:60]}",
                    "description": f"{printer.name} · {job['filamentGrams']:.1f} g {job['filamentType']}",
                    "quantity": 1,
                    "unitPrice": job["priceCents"],
                    "totalPrice": job["priceCents"],
                }
            ],
            idempotency_key=idem_key,
            correlation_id=corr_id,
            allow_overdraft=allow_overdraft,
            max_overdraft_cents=st.settings.max_negative_balance_cents,
        )
        if not charge_res.get("paid"):
            reason = charge_res.get("reason", "CHARGE_FAILED")
            if reason == "INSUFFICIENT_BALANCE":
                bal = int(charge_res.get("balance", {}).get("amount", sess.balance_cents))
                sess.balance_cents = bal
                return {
                    "started": False,
                    "reason": "INSUFFICIENT_BALANCE",
                    "balanceCents": bal,
                    "missingCents": max(0, job["priceCents"] - bal),
                }
            return {"started": False, "reason": reason}

        payment_ref = charge_res.get("paymentId")
        charged_cents = job["priceCents"]
        sess.balance_cents = int(charge_res.get("remainingBalance", {}).get("amount", sess.balance_cents - charged_cents))
        st.store.log_event(
            "payment_charged",
            job_id=job_id,
            printer_id=printer.id,
            user_id=sess.user.get("user_id"),
            user_name=sess.user.get("display_name"),
            amount_cents=charged_cents,
            details={"paymentRef": payment_ref, "overdraft": allow_overdraft},
        )
    else:
        await st.payment.cancel_authorization(
            payment_authorization_id=sess.payment_authorization_id,
            reason=f"USED_{mode.upper()}",
        )

    # Step 3: Hand off file upload + print start to background task and return immediately
    # Cancel any pending cooldown shutdown on this printer since a new job is starting
    st.store.set_power_state(printer.id, power_state="ON", pending_shutdown=False)
    cur_st = st.registry.get_status(printer.id)
    cur_st.pending_shutdown = False
    st.store.set_job_status(job_id, "sending")
    user_id = sess.user.get("user_id")
    user_display_name = sess.user.get("display_name", "")
    remaining_balance = sess.balance_cents
    st.sessions.pop(body.sessionId, None)

    st.active_uploads[printer.id] = {
        "jobId": job_id,
        "printerId": printer.id,
        "displayName": job.get("displayName", job["filename"]),
        "userDisplayName": user_display_name,
        "sectionName": section_name,
        "estimatedTimeSeconds": int(job.get("estimatedTimeSeconds") or 0),
        "stage": "uploading",
        "percent": 0,
        "sentBytes": 0,
        "totalBytes": int(job.get("fileSize") or 0),
        "bytesPerSec": 0,
    }
    st.events.publish("queue_updated", {"action": "sending", "jobId": job_id})
    st.events.publish("printers_updated", {"printerId": printer.id})

    task = asyncio.create_task(
        _run_printer_transfer(
            st,
            job=job,
            printer=printer,
            mode=mode,
            payment_ref=payment_ref,
            charged_cents=charged_cents,
            user_id=user_id,
            user_display_name=user_display_name,
            section_id=body.sectionId,
            section_name=section_name,
        ),
        name=f"transfer-{job_id}",
    )
    st.upload_tasks.add(task)
    st.upload_tasks_by_job[job_id] = task
    task.add_done_callback(st.upload_tasks.discard)

    return {
        "started": True,
        "uploading": True,
        "jobId": job_id,
        "mode": mode,
        "chargedCents": charged_cents,
        "remainingBalanceCents": remaining_balance,
        "sectionName": section_name,
        "printerName": printer.name,
    }


@router.post("/printers/{printer_id}/power/on")
async def printer_power_on(printer_id: str, body: PrinterPowerOnRequest, request: Request) -> dict[str, Any]:
    st = request.app.state
    sess = _get_session(request, body.sessionId)
    if not sess.approved_for_3d_print:
        return {"ok": False, "reason": "NOT_APPROVED"}

    printer = st.registry.get_by_id(printer_id)
    if not printer:
        raise HTTPException(status_code=404, detail="Drucker nicht gefunden")

    res = await st.power_control.power_on(printer, rfid=sess.rfid_uid, user=sess.user)
    if not res.ok:
        await st.device_client.send_beeper("error")
        return {"ok": False, "reason": res.reason or "POWER_ON_FAILED"}

    # Clean up payment authorization since it was only used to verify RFID/qualification
    await st.payment.cancel_authorization(
        payment_authorization_id=sess.payment_authorization_id,
        reason="PRINTER_POWER_ON",
    )
    st.sessions.pop(body.sessionId, None)

    st.store.set_power_state(printer.id, power_state="BOOTING", pending_shutdown=False)
    st.store.log_event(
        "printer_powered_on",
        printer_id=printer.id,
        user_id=sess.user.get("user_id"),
        user_name=sess.user.get("display_name"),
    )

    # Immediately re-poll so mock or fast-responding printers appear READY right away
    new_st = await st.printer_client.poll_status(printer)
    if printer.id in st.dev_nozzle_overrides:
        new_st.nozzle_temp = float(st.dev_nozzle_overrides[printer.id])
    new_st.shutdown_temp_threshold_c = st.settings.hotend_cooldown_temp_c
    new_st.pending_shutdown = False
    if new_st.state != "OFFLINE":
        new_st.power_state = "ON"
        st.store.set_power_state(printer.id, power_state="ON", pending_shutdown=False)
    else:
        new_st.power_state = "BOOTING"
        new_st.error = "Startet…"
    st.registry.set_status(printer.id, new_st)

    await st.device_client.send_beeper("ok")
    await st.device_client.send_led("idle")
    st.events.publish("printers_updated", {"printerId": printer.id, "state": new_st.state})
    return {"ok": True, "action": "powered_on", "printerId": printer.id, "state": new_st.state}


@router.post("/printers/{printer_id}/power/off")
async def printer_power_off(printer_id: str, request: Request) -> dict[str, Any]:
    st = request.app.state
    printer = st.registry.get_by_id(printer_id)
    if not printer:
        raise HTTPException(status_code=404, detail="Drucker nicht gefunden")

    if printer.id in st.active_uploads:
        await st.device_client.send_beeper("error")
        return {"ok": False, "reason": "PRINTER_UPLOADING"}

    live_st = await st.printer_client.poll_status(printer)
    if printer.id in st.dev_nozzle_overrides:
        live_st.nozzle_temp = float(st.dev_nozzle_overrides[printer.id])
    thresh = st.settings.hotend_cooldown_temp_c
    live_st.shutdown_temp_threshold_c = thresh

    if live_st.state == "PRINTING":
        st.registry.set_status(printer.id, live_st)
        await st.device_client.send_beeper("error")
        return {"ok": False, "reason": "PRINTER_PRINTING"}

    # Thermal protection: wait until hotend cooled down to <= threshold (default 50°C)
    if live_st.nozzle_temp is not None and live_st.nozzle_temp > thresh:
        st.store.set_power_state(printer.id, power_state="ON", pending_shutdown=True)
        live_st.power_state = "ON"
        live_st.pending_shutdown = True
        st.registry.set_status(printer.id, live_st)
        st.store.log_event(
            "printer_shutdown_scheduled",
            printer_id=printer.id,
            details={"nozzleTemp": live_st.nozzle_temp, "thresholdC": thresh},
        )
        await st.device_client.send_beeper("ok")
        st.events.publish("printers_updated", {"printerId": printer.id, "state": live_st.state})
        return {
            "ok": True,
            "action": "cooldown_scheduled",
            "printerId": printer.id,
            "nozzleTemp": live_st.nozzle_temp,
            "thresholdC": thresh,
        }

    res = await st.power_control.power_off(printer, reason="USER_REQUEST")
    if not res.ok:
        await st.device_client.send_beeper("error")
        return {"ok": False, "reason": res.reason or "POWER_OFF_FAILED"}

    st.store.set_power_state(printer.id, power_state="OFF", pending_shutdown=False)
    st.store.log_event(
        "printer_powered_off",
        printer_id=printer.id,
        details={"reason": "USER_REQUEST", "nozzleTemp": live_st.nozzle_temp},
    )
    live_st.power_state = "OFF"
    live_st.pending_shutdown = False
    if st.settings.power_mode == "mock":
        live_st.state = "OFFLINE"
        live_st.error = "Ausgeschaltet"
    st.registry.set_status(printer.id, live_st)

    await st.device_client.send_beeper("ok")
    st.events.publish("printers_updated", {"printerId": printer.id, "state": live_st.state})
    return {"ok": True, "action": "powered_off", "printerId": printer.id}


@router.post("/printers/{printer_id}/power/cancel-shutdown")
async def printer_cancel_shutdown(printer_id: str, request: Request) -> dict[str, Any]:
    st = request.app.state
    printer = st.registry.get_by_id(printer_id)
    if not printer:
        raise HTTPException(status_code=404, detail="Drucker nicht gefunden")

    st.store.set_power_state(printer.id, pending_shutdown=False)
    cur = st.registry.get_status(printer.id)
    cur.pending_shutdown = False
    st.store.log_event("printer_shutdown_cancelled", printer_id=printer.id)
    st.events.publish("printers_updated", {"printerId": printer.id, "state": cur.state})
    return {"ok": True, "printerId": printer.id}


@router.get("/audit")
def recent_audit(request: Request) -> dict[str, Any]:
    return {"items": request.app.state.store.recent_audit(30)}


@router.get("/admin/config")
def get_admin_config(request: Request) -> dict[str, Any]:
    st = request.app.state
    ov = _load_admin_overrides(st.settings.data_dir)
    token = ov.get("easyverwaltungServiceToken") or st.settings.easyverwaltung_service_token
    return {
        "terminalId": ov.get("terminalId") or st.settings.terminal_id,
        "easyverwaltungUrl": ov.get("easyverwaltungUrl", st.settings.easyverwaltung_url),
        "easyverwaltungServiceId": ov.get("easyverwaltungServiceId", st.settings.easyverwaltung_service_id),
        "hasToken": bool(token and token != "REPLACE_ME"),
        "terminalIp": _detect_local_ip(),
        "paymentMode": st.settings.payment_mode,
        "version": "0.3.0",
    }


@router.post("/admin/config")
def update_admin_config(body: AdminConfigUpdate, request: Request) -> dict[str, Any]:
    st = request.app.state
    ov = _load_admin_overrides(st.settings.data_dir)
    ov["terminalId"] = body.terminalId.strip()
    ov["easyverwaltungUrl"] = body.easyverwaltungUrl.strip().rstrip("/")
    ov["easyverwaltungServiceId"] = body.easyverwaltungServiceId.strip()
    if body.easyverwaltungServiceToken is not None and body.easyverwaltungServiceToken.strip():
        ov["easyverwaltungServiceToken"] = body.easyverwaltungServiceToken.strip()
    _save_admin_overrides(st.settings.data_dir, ov)
    st.store.log_event(
        "admin_config_updated",
        details={
            "terminalId": ov["terminalId"],
            "serviceId": ov["easyverwaltungServiceId"],
            "tokenUpdated": bool(body.easyverwaltungServiceToken and body.easyverwaltungServiceToken.strip()),
        },
    )
    return {"ok": True, "config": get_admin_config(request)}


@router.post("/admin/test-connection")
async def test_admin_connection(request: Request) -> dict[str, Any]:
    st = request.app.state
    ov = _load_admin_overrides(st.settings.data_dir)
    url = (ov.get("easyverwaltungUrl") or st.settings.easyverwaltung_url or "").strip().rstrip("/")
    service_id = (ov.get("easyverwaltungServiceId") or st.settings.easyverwaltung_service_id or "").strip()
    token = (ov.get("easyverwaltungServiceToken") or st.settings.easyverwaltung_service_token or "").strip()

    if st.settings.payment_mode == "mock" and (not url or "localhost:8001" in url):
        return {"ok": True, "mode": "mock", "message": "Mock-Modus aktiv (OK)"}
    if not url or not token or token == "REPLACE_ME":
        return {"ok": False, "message": "Server-URL oder Token fehlt."}

    try:
        async with httpx.AsyncClient(timeout=4.0) as client:
            params = {"serviceId": service_id} if service_id else {}
            resp = await client.post(
                f"{url}/api/service/health",
                params=params,
                headers={"X-Service-Token": token, "Accept": "application/json"},
            )
            data = resp.json() if resp.headers.get("content-type", "").startswith("application/json") else {}
            if resp.status_code == 200 and data.get("active"):
                return {"ok": True, "message": f"Verbunden: {data.get('serviceName') or service_id} (active)"}
            reason = data.get("reasonCode") or data.get("error", {}).get("code") or f"HTTP {resp.status_code}"
            return {"ok": False, "message": f"Abgelehnt: {reason}"}
    except Exception as exc:
        return {"ok": False, "message": f"Nicht erreichbar: {type(exc).__name__}"}


# --- Dev / Mock Simulator Endpoints (only active in mock modes) ---

@router.post("/dev/simulate-rfid")
async def dev_simulate_rfid(body: DevSimulateRfidRequest, request: Request) -> dict[str, Any]:
    st = request.app.state
    if not st.settings.is_dev_mock:
        raise HTTPException(status_code=403, detail="Dev endpoints disabled in live mode")
    uid = body.uid.strip().upper()
    # Try routing via device-agent first so full WS pipeline is exercised; fallback to direct publish
    ok = await st.device_client.trigger_mock_rfid(uid)
    if not ok:
        st.events.publish("rfid_scan", {"uid": uid})
    return {"ok": True, "uid": uid, "viaDeviceAgent": ok}


@router.post("/dev/printer-state")
async def dev_printer_state(body: DevPrinterStateRequest, request: Request) -> dict[str, Any]:
    st = request.app.state
    if st.settings.printer_mode != "mock" or not isinstance(st.printer_client, MockPrinterClient):
        raise HTTPException(status_code=403, detail="Printer mock disabled")
    new_st = st.printer_client.set_mock_state(body.printerId, body.state)
    st.registry.set_status(body.printerId, new_st)
    st.events.publish("printers_updated", {"printerId": body.printerId, "state": new_st.state})
    return {"ok": True, "printerId": body.printerId, "state": new_st.state}


@router.post("/dev/printer-temp")
async def dev_set_printer_temp(body: DevPrinterTempRequest, request: Request) -> dict[str, Any]:
    st = request.app.state
    if not st.settings.is_dev_mock:
        raise HTTPException(status_code=403, detail="Dev endpoints disabled in live mode")
    if body.nozzleTemp is None:
        st.dev_nozzle_overrides.pop(body.printerId, None)
    else:
        st.dev_nozzle_overrides[body.printerId] = float(body.nozzleTemp)
    cur = st.registry.get_status(body.printerId)
    if body.nozzleTemp is not None:
        cur.nozzle_temp = float(body.nozzleTemp)
    st.events.publish("printers_updated", {"printerId": body.printerId, "state": cur.state})
    return {"ok": True, "printerId": body.printerId, "nozzleTemp": body.nozzleTemp}


@router.post("/dev/upload-sample")
async def dev_upload_sample(body: DevUploadSampleRequest, request: Request) -> dict[str, Any]:
    st = request.app.state
    if not st.settings.is_dev_mock:
        raise HTTPException(status_code=403, detail="Dev endpoints disabled in live mode")

    # Tiny valid 1x1 orange PNG preview embedded as a G-Code thumbnail
    sample_png_b64 = (
        "iVBORw0KGgoAAAANSUhEUgAAAEAAAABACAIAAAAlC+aJAAAAQ0lEQVR4nO3PQQ0AAAwCoNm/"
        "9HI4hB5IYGZmZmZmZmZmZmZmZmZmZmZmZmZmZmZmZmZmZmZmZmZmZmZmZmZmZmZmZmY+Gw6m"
        "AAGRQ1xVAAAAAElFTkSuQmCC"
    )
    mins = max(1, body.estimatedTimeSeconds // 60)
    hours, rem_m = divmod(mins, 60)
    time_str = f"{hours}h {rem_m}m 0s" if hours else f"{rem_m}m 0s"
    gcode = (
        f"; generated by PrusaSlicer 2.9.4 (Dev Sample)\n"
        f"; thumbnail begin 64x64 {len(sample_png_b64)}\n"
        f"; {sample_png_b64}\n"
        f"; thumbnail end\n"
        f"G28\nG1 X10 Y10 Z0.2\n"
        f"; filament used [g] = {body.filamentGrams:.2f}\n"
        f"; total filament cost = {body.slicerCostEur:.2f}\n"
        f"; estimated printing time (normal mode) = {time_str}\n"
        f"; filament_type = {body.filamentType}\n"
    ).encode("utf-8")

    job_id = st.store.new_job_id()
    dest = st.store.uploads_dir / f"{job_id}.gcode"
    dest.write_bytes(gcode)
    meta = parse_print_file(dest)
    pricing = compute_job_price(
        meta,
        default_price_per_gram_cents=st.settings.default_price_per_gram_cents,
        min_price_per_gram_cents=st.settings.min_price_per_gram_cents,
        min_job_price_cents=st.settings.min_job_price_cents,
    )
    thumb_path = st.store.thumbs_dir / f"{job_id}.png"
    thumb_path.write_bytes(base64.b64decode(sample_png_b64))

    printer = st.registry.get_by_slot(body.slot)
    job = st.store.add_job(
        job_id=job_id,
        printer_slot=body.slot,
        printer_id=printer.id if printer else f"prusa-{body.slot:02d}",
        printer_name=printer.name if printer else f"Drucker {body.slot}",
        filename=body.filename,
        file_path=dest,
        file_size=len(gcode),
        thumbnail_path=thumb_path,
        thumbnail_ext="png",
        filament_g=meta.filament_g,
        filament_type=meta.filament_type,
        estimated_time_s=meta.estimated_time_s,
        price_cents=pricing.price_cents,
        slicer_price_cents=pricing.slicer_price_cents,
        price_source=pricing.price_source,
        ttl_seconds=st.settings.job_ttl_seconds,
    )
    st.events.publish("queue_updated", {"action": "uploaded", "jobId": job["id"], "slot": body.slot})
    return {"ok": True, "job": job}
