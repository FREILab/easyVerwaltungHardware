from __future__ import annotations

import asyncio
import logging
import time
from collections.abc import Callable
from dataclasses import dataclass
from pathlib import Path
from urllib.parse import quote

import httpx

from printer.registry import PrinterConfig, PrinterStatus

logger = logging.getLogger("st3d.prusalink")


@dataclass
class PrinterActionResult:
    ok: bool
    reason: str = ""
    remote_path: str = ""


class PrinterClient:
    """Interface for talking to real or simulated PrusaLink printers."""

    async def poll_status(self, printer: PrinterConfig) -> PrinterStatus:
        raise NotImplementedError

    async def upload_file(
        self,
        printer: PrinterConfig,
        file_path: Path,
        filename: str,
        print_after_upload: bool = True,
        on_progress: Callable[[int, int], None] | None = None,
    ) -> PrinterActionResult:
        """Upload file to printer, forwarding Print-After-Upload (?1 / ?0) as requested by Slicer."""
        raise NotImplementedError

    async def start_print(self, printer: PrinterConfig, remote_path: str) -> PrinterActionResult:
        """Start previously uploaded file on printer."""
        raise NotImplementedError


def map_prusalink_state(raw_state: str) -> tuple[str, str | None]:
    """Map PrusaLink printer state to terminal state.

    Safety rule: ONLY IDLE/READY can accept a new job.
    FINISHED is mapped to ATTENTION so a new print never starts on an uncleared print bed.
    """
    s = (raw_state or "").strip().upper()
    if s in {"IDLE", "READY"}:
        return "READY", None
    if s in {"PRINTING", "BUSY"}:
        return "PRINTING", None
    if s == "PAUSED":
        return "PAUSED", None
    if s == "FINISHED":
        return "ATTENTION", "Fertig (am Drucker bestätigen)"
    if s in {"ATTENTION", "ERROR", "STOPPED"}:
        return "ATTENTION", s
    return "OFFLINE", f"Unbekannter Status: {s or 'leer'}"


class PrusaLinkClient(PrinterClient):
    """Real PrusaLink v1 HTTP client (supports MINI, MK3S/Pi, MK3.5, MK4, XL)."""

    def __init__(self, status_timeout: float = 3.5, upload_timeout: float = 120.0) -> None:
        self.status_timeout = status_timeout
        self.upload_timeout = upload_timeout
        self._storage_cache: dict[str, str] = {}
        self._nozzle_cache: dict[str, float] = {}

    @staticmethod
    def _base_url(printer: PrinterConfig) -> str:
        ip = printer.ip.strip()
        if ip.startswith(("http://", "https://")):
            return ip.rstrip("/")
        return f"http://{ip}"

    @staticmethod
    def _auth_kwargs(printer: PrinterConfig) -> dict:
        headers: dict[str, str] = {}
        auth: httpx.Auth | None = None
        if printer.auth_type == "api_key" or (printer.api_key and not printer.password):
            if printer.api_key:
                headers["X-Api-Key"] = printer.api_key
        elif printer.username or printer.password:
            auth = httpx.DigestAuth(printer.username or "maker", printer.password)
        return {"headers": headers, "auth": auth}

    async def _resolve_storage(self, client: httpx.AsyncClient, printer: PrinterConfig, auth_kw: dict) -> str:
        cached = self._storage_cache.get(printer.id)
        if cached:
            return cached
        try:
            resp = await client.get(f"{self._base_url(printer)}/api/v1/storage", **auth_kw)
            if resp.status_code == 200:
                data = resp.json()
                for entry in data.get("storage_list", []):
                    if entry.get("available") and not entry.get("read_only"):
                        name = str(entry.get("name") or entry.get("path", "usb")).strip("/")
                        if name:
                            self._storage_cache[printer.id] = name
                            return name
        except Exception as exc:
            logger.debug("Storage probe failed for %s: %s", printer.id, exc)
        return "usb"

    async def poll_status(self, printer: PrinterConfig) -> PrinterStatus:
        if not printer.ip:
            return PrinterStatus(state="OFFLINE", error="Keine IP konfiguriert", updated_at=time.time())
        auth_kw = self._auth_kwargs(printer)
        try:
            async with httpx.AsyncClient(timeout=self.status_timeout) as client:
                resp = await client.get(f"{self._base_url(printer)}/api/v1/status", **auth_kw)
                if resp.status_code == 401:
                    return PrinterStatus(state="OFFLINE", error="PrusaLink Auth fehlgeschlagen", updated_at=time.time())
                resp.raise_for_status()
                data = resp.json()

            p = data.get("printer") or {}
            job = data.get("job") or {}
            storage = data.get("storage") or {}
            if storage.get("name") and not storage.get("read_only"):
                self._storage_cache[printer.id] = str(storage["name"]).strip("/")

            # Fetch nozzle_diameter once via /api/v1/info and cache it
            nozzle_d = self._nozzle_cache.get(printer.id)
            if nozzle_d is None:
                try:
                    async with httpx.AsyncClient(timeout=self.status_timeout) as c2:
                        ir = await c2.get(f"{self._base_url(printer)}/api/v1/info", **auth_kw)
                        if ir.status_code == 200:
                            nd = ir.json().get("nozzle_diameter")
                            if nd is not None:
                                nozzle_d = float(nd)
                                self._nozzle_cache[printer.id] = nozzle_d
                except Exception:
                    pass

            mapped_state, note = map_prusalink_state(str(p.get("state", "")))
            progress = job.get("progress")
            time_rem = job.get("time_remaining")
            active_job = None
            if isinstance(job.get("file"), dict):
                active_job = job["file"].get("display_name") or job["file"].get("name")

            return PrinterStatus(
                state=mapped_state,
                progress=float(progress) if progress is not None else None,
                active_job=active_job,
                nozzle_temp=float(p["temp_nozzle"]) if p.get("temp_nozzle") is not None else None,
                target_nozzle=float(p["target_nozzle"]) if p.get("target_nozzle") is not None else None,
                bed_temp=float(p["temp_bed"]) if p.get("temp_bed") is not None else None,
                target_bed=float(p["target_bed"]) if p.get("target_bed") is not None else None,
                nozzle_diameter=nozzle_d,
                time_remaining_s=int(time_rem) if time_rem is not None else None,
                updated_at=time.time(),
                error=note,
            )
        except Exception as exc:
            logger.debug("Status poll failed for %s (%s): %s", printer.id, printer.ip, exc)
            return PrinterStatus(state="OFFLINE", error="Nicht erreichbar", updated_at=time.time())

    async def upload_file(
        self,
        printer: PrinterConfig,
        file_path: Path,
        filename: str,
        print_after_upload: bool = True,
        on_progress: Callable[[int, int], None] | None = None,
    ) -> PrinterActionResult:
        if not file_path.exists():
            return PrinterActionResult(ok=False, reason="FILE_MISSING")

        auth_kw = self._auth_kwargs(printer)
        size = file_path.stat().st_size
        # Keep filename FAT32-friendly for Prusa USB storage
        stem = Path(filename).stem[:48]
        ext = Path(filename).suffix.lower() or ".gcode"
        remote_name = f"{stem}{ext}"

        async def _file_chunks():
            sent = 0
            last_emit_pct = -1
            last_emit_ts = 0.0
            if on_progress:
                on_progress(0, size)
            with file_path.open("rb") as f:
                while True:
                    chunk = f.read(64 * 1024)
                    if not chunk:
                        break
                    sent += len(chunk)
                    yield chunk
                    if on_progress and size > 0:
                        pct = int(sent * 100 / size)
                        now = time.monotonic()
                        if pct >= last_emit_pct + 2 or now - last_emit_ts >= 0.25 or sent >= size:
                            last_emit_pct = pct
                            last_emit_ts = now
                            on_progress(sent, size)

        # Per-operation timeouts: large files over slow USB can take minutes,
        # while a dead socket still aborts within 30s.
        timeout = httpx.Timeout(connect=5.0, write=30.0, read=60.0, pool=5.0)
        try:
            async with httpx.AsyncClient(timeout=timeout) as client:
                storage = await self._resolve_storage(client, printer, auth_kw)
                remote_path = f"{storage}/{remote_name}"
                encoded_name = quote(remote_name, safe="._-")
                url = f"{self._base_url(printer)}/api/v1/files/{storage}/{encoded_name}"

                headers = {
                    **auth_kw["headers"],
                    "Content-Type": "application/octet-stream",
                    "Content-Length": str(size),
                    "Print-After-Upload": "?1" if print_after_upload else "?0",
                    "Overwrite": "?1",
                }
                resp = await client.put(url, content=_file_chunks(), headers=headers, auth=auth_kw["auth"])
                if resp.status_code in {200, 201, 204}:
                    if on_progress:
                        on_progress(size, size)
                    logger.info(
                        "Uploaded %s (%d bytes, print_after_upload=%s) to %s:/%s",
                        filename,
                        size,
                        print_after_upload,
                        printer.id,
                        remote_path,
                    )
                    return PrinterActionResult(ok=True, remote_path=remote_path)
                if resp.status_code == 409:
                    return PrinterActionResult(ok=False, reason="PRINTER_BUSY")
                if resp.status_code == 401:
                    return PrinterActionResult(ok=False, reason="PRINTER_AUTH_FAILED")
                logger.warning("PrusaLink upload failed on %s: HTTP %d %s", printer.id, resp.status_code, resp.text[:200])
                return PrinterActionResult(ok=False, reason=f"UPLOAD_HTTP_{resp.status_code}")
        except httpx.TimeoutException:
            return PrinterActionResult(ok=False, reason="UPLOAD_TIMEOUT")
        except Exception as exc:
            logger.error("PrusaLink upload exception on %s: %s", printer.id, exc)
            return PrinterActionResult(ok=False, reason="PRINTER_OFFLINE")

    async def start_print(self, printer: PrinterConfig, remote_path: str) -> PrinterActionResult:
        auth_kw = self._auth_kwargs(printer)
        parts = remote_path.strip("/").split("/", 1)
        if len(parts) != 2:
            return PrinterActionResult(ok=False, reason="INVALID_REMOTE_PATH")
        storage, remote_name = parts
        encoded_name = quote(remote_name, safe="._-/")
        url = f"{self._base_url(printer)}/api/v1/files/{storage}/{encoded_name}"

        try:
            async with httpx.AsyncClient(timeout=10.0) as client:
                resp = await client.post(url, headers=auth_kw["headers"], auth=auth_kw["auth"])
                if resp.status_code in {200, 201, 202, 204}:
                    logger.info("Started print %s on %s", remote_path, printer.id)
                    return PrinterActionResult(ok=True, remote_path=remote_path)
                if resp.status_code == 409:
                    return PrinterActionResult(ok=False, reason="PRINTER_BUSY")
                logger.warning("PrusaLink start failed on %s: HTTP %d %s", printer.id, resp.status_code, resp.text[:200])
                return PrinterActionResult(ok=False, reason=f"START_HTTP_{resp.status_code}")
        except httpx.TimeoutException:
            return PrinterActionResult(ok=False, reason="START_TIMEOUT")
        except Exception as exc:
            logger.error("PrusaLink start exception on %s: %s", printer.id, exc)
            return PrinterActionResult(ok=False, reason="PRINTER_OFFLINE")


class MockPrinterClient(PrinterClient):
    """In-memory simulator for local Mac & Pi UI testing before connecting real Prusa printers."""

    def __init__(self) -> None:
        self._states: dict[str, PrinterStatus] = {}

    def set_mock_state(self, printer_id: str, state: str, active_job: str | None = None) -> PrinterStatus:
        normalized = state.upper()
        if normalized not in {"READY", "PRINTING", "PAUSED", "ATTENTION", "OFFLINE"}:
            normalized = "READY"
        st = PrinterStatus(
            state=normalized,
            progress=42.0 if normalized == "PRINTING" else None,
            active_job=active_job or ("mock_job.bgcode" if normalized == "PRINTING" else None),
            nozzle_temp=215.0 if normalized == "PRINTING" else 24.0,
            bed_temp=60.0 if normalized == "PRINTING" else 22.0,
            updated_at=time.time(),
        )
        self._states[printer_id] = st
        return st

    async def poll_status(self, printer: PrinterConfig) -> PrinterStatus:
        st = self._states.get(printer.id)
        if st is None:
            st = PrinterStatus(
                state="READY",
                nozzle_temp=24.0,
                bed_temp=22.0,
                updated_at=time.time(),
            )
            self._states[printer.id] = st
        else:
            st.updated_at = time.time()
        return st

    async def upload_file(
        self,
        printer: PrinterConfig,
        file_path: Path,
        filename: str,
        print_after_upload: bool = True,
        on_progress: Callable[[int, int], None] | None = None,
    ) -> PrinterActionResult:
        st = await self.poll_status(printer)
        if st.state == "OFFLINE":
            return PrinterActionResult(ok=False, reason="PRINTER_OFFLINE")
        if not st.can_accept_job:
            return PrinterActionResult(ok=False, reason="PRINTER_BUSY")
        total = file_path.stat().st_size if file_path.exists() else 1000
        for step in (0, 25, 50, 75, 100):
            if on_progress:
                on_progress(int(total * step / 100), total)
            if step < 100:
                await asyncio.sleep(0.08)
        if print_after_upload:
            self.set_mock_state(printer.id, "PRINTING", active_job=filename)
        return PrinterActionResult(ok=True, remote_path=f"usb/{filename}")

    async def start_print(self, printer: PrinterConfig, remote_path: str) -> PrinterActionResult:
        st = await self.poll_status(printer)
        if not st.can_accept_job:
            return PrinterActionResult(ok=False, reason="PRINTER_BUSY")
        self.set_mock_state(printer.id, "PRINTING", active_job=remote_path.split("/")[-1])
        return PrinterActionResult(ok=True, remote_path=remote_path)
