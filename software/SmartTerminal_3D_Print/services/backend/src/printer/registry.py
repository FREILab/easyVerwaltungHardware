from __future__ import annotations

import json
import logging
import time
from dataclasses import dataclass, field
from pathlib import Path
from typing import Any

import httpx

logger = logging.getLogger("st3d.printers")


_DEFAULT_VOLUMES = {
    "MINI": "180×180×180",
    "MK3S": "250×210×210",
    "MK3.5": "250×210×210",
    "MK4": "250×210×220",
    "MK4S": "250×210×220",
    "XL": "360×360×360",
}


@dataclass
class PrinterConfig:
    slot: int
    id: str
    name: str
    model: str
    ip: str
    auth_type: str = "digest"  # "digest" | "api_key"
    username: str = "maker"
    password: str = ""
    api_key: str = ""
    build_volume: str = ""
    base_print_hours: float = 0.0
    base_filament_km: float = 0.0
    power_control_id: str | None = None


@dataclass
class PrinterStatus:
    state: str = "READY"  # READY | PRINTING | PAUSED | ATTENTION | OFFLINE
    progress: float | None = None
    active_job: str | None = None
    nozzle_temp: float | None = None
    target_nozzle: float | None = None
    bed_temp: float | None = None
    target_bed: float | None = None
    nozzle_diameter: float | None = None
    time_remaining_s: int | None = None
    online_since: float | None = None
    power_state: str = "ON"  # ON | OFF | BOOTING
    pending_shutdown: bool = False
    shutdown_temp_threshold_c: float = 50.0
    updated_at: float = field(default_factory=time.time)
    error: str | None = None

    @property
    def can_accept_job(self) -> bool:
        return self.state == "READY"


class PrinterRegistry:
    """Loads printer definitions from file/URL with last-known-good fallback and tracks live status."""

    def __init__(
        self,
        config_file: Path,
        config_url: str = "",
        override_file: Path | None = None,
    ) -> None:
        self.config_file = config_file
        self.config_url = config_url
        self.override_file = override_file
        self._printers: dict[int, PrinterConfig] = {}
        self._statuses: dict[str, PrinterStatus] = {}
        self.reload_from_file()

    def reload_from_file(self) -> bool:
        target = self.override_file if (self.override_file and self.override_file.exists()) else self.config_file
        if not target.exists():
            logger.warning("Printers config file not found: %s", target)
            return False
        try:
            raw = json.loads(target.read_text(encoding="utf-8"))
            return self._apply_raw(raw)
        except Exception as exc:
            logger.error("Failed to load printers config %s: %s", target, exc)
            return False

    def export_for_admin(self) -> list[dict[str, Any]]:
        """Export printer configs for the BV Admin UI without leaking plaintext secrets."""
        out: list[dict[str, Any]] = []
        for p in self.list_printers():
            has_secret = bool(
                (p.auth_type == "api_key" and p.api_key and p.api_key != "REPLACE_ME")
                or (p.auth_type == "digest" and p.password and p.password != "REPLACE_ME")
            )
            out.append(
                {
                    "slot": p.slot,
                    "id": p.id,
                    "name": p.name,
                    "model": p.model,
                    "ip": p.ip,
                    "authType": p.auth_type,
                    "username": p.username,
                    "hasSecret": has_secret,
                    "buildVolume": p.build_volume,
                    "powerControlId": p.power_control_id or "",
                }
            )
        return out

    def save_override(self, items: list[dict[str, Any]]) -> bool:
        """Persist updated printer list to /data/printers.json preserving unchanged secrets."""
        existing_by_slot = dict(self._printers)
        existing_by_id = {p.id: p for p in self._printers.values()}
        cleaned: list[dict[str, Any]] = []
        for idx, item in enumerate(items, start=1):
            slot = int(item.get("slot") or idx)
            if slot < 1 or slot > 4:
                continue
            pid = str(item.get("id") or f"prusa-{slot:02d}").strip() or f"prusa-{slot:02d}"
            prev = existing_by_id.get(pid) or existing_by_slot.get(slot)
            auth_type = str(item.get("authType") or item.get("auth_type") or "digest").strip().lower()
            if auth_type not in {"digest", "api_key"}:
                auth_type = "digest"
            secret_in = str(item.get("secret") or "").strip()
            password = prev.password if prev else ""
            api_key = prev.api_key if prev else ""
            if secret_in:
                if auth_type == "api_key":
                    api_key = secret_in
                else:
                    password = secret_in
            model = str(item.get("model") or "MK4").strip() or "MK4"
            cleaned.append(
                {
                    "slot": slot,
                    "id": pid,
                    "name": str(item.get("name") or f"Drucker {slot} ({model})").strip(),
                    "model": model,
                    "ip": str(item.get("ip") or "").strip(),
                    "auth_type": auth_type,
                    "username": str(item.get("username") or "maker").strip() or "maker",
                    "password": password,
                    "api_key": api_key,
                    "build_volume": str(item.get("buildVolume") or item.get("build_volume") or _DEFAULT_VOLUMES.get(model.upper(), "")).strip(),
                    "base_print_hours": prev.base_print_hours if prev else 0.0,
                    "base_filament_km": prev.base_filament_km if prev else 0.0,
                    "power_control_id": str(item.get("powerControlId") or item.get("power_control_id") or "").strip(),
                }
            )
        if not cleaned:
            return False
        if self.override_file:
            self.override_file.parent.mkdir(parents=True, exist_ok=True)
            tmp = self.override_file.with_suffix(".json.tmp")
            tmp.write_text(json.dumps(cleaned, indent=2, ensure_ascii=False), encoding="utf-8")
            tmp.replace(self.override_file)
        return self._apply_raw(cleaned)

    async def reload_from_url(self) -> bool:
        if not self.config_url:
            return self.reload_from_file()
        try:
            async with httpx.AsyncClient(timeout=5.0) as client:
                resp = await client.get(self.config_url)
                resp.raise_for_status()
                return self._apply_raw(resp.json())
        except Exception as exc:
            logger.warning("Failed to fetch remote printers.json (%s), keeping current config: %s", self.config_url, exc)
            return False

    def _apply_raw(self, raw: Any) -> bool:
        if not isinstance(raw, list):
            return False
        parsed: dict[int, PrinterConfig] = {}
        for idx, item in enumerate(raw, start=1):
            if not isinstance(item, dict):
                continue
            slot = int(item.get("slot") or idx)
            pid = str(item.get("id") or f"prusa-{slot:02d}")
            # Support legacy `token` field as api_key fallback
            api_key = str(item.get("api_key") or item.get("token") or "")
            auth_type = str(item.get("auth_type") or ("api_key" if api_key and not item.get("password") else "digest"))
            model = str(item.get("model") or "Prusa")
            vol = str(item.get("build_volume") or _DEFAULT_VOLUMES.get(model.upper(), "250×210×210"))
            cfg = PrinterConfig(
                slot=slot,
                id=pid,
                name=str(item.get("name") or f"Drucker {slot}"),
                model=model,
                ip=str(item.get("ip") or ""),
                auth_type=auth_type,
                username=str(item.get("username") or "maker"),
                password=str(item.get("password") or ""),
                api_key=api_key,
                build_volume=vol,
                base_print_hours=float(item.get("base_print_hours") or 0.0),
                base_filament_km=float(item.get("base_filament_km") or 0.0),
                power_control_id=str(item.get("power_control_id") or "").strip() or None,
            )
            parsed[slot] = cfg
            self._statuses.setdefault(pid, PrinterStatus(state="READY"))
        if parsed:
            self._printers = parsed
            return True
        return False

    def list_printers(self) -> list[PrinterConfig]:
        return [self._printers[s] for s in sorted(self._printers)]

    def get_by_slot(self, slot: int) -> PrinterConfig | None:
        return self._printers.get(slot)

    def get_by_id(self, printer_id: str) -> PrinterConfig | None:
        for p in self._printers.values():
            if p.id == printer_id:
                return p
        return None

    def get_status(self, printer_id: str) -> PrinterStatus:
        return self._statuses.setdefault(printer_id, PrinterStatus(state="OFFLINE"))

    def set_status(self, printer_id: str, status: PrinterStatus) -> None:
        prev = self._statuses.get(printer_id)
        if status.state != "OFFLINE":
            if status.online_since is None:
                status.online_since = (
                    prev.online_since if (prev and prev.state != "OFFLINE" and prev.online_since) else time.time()
                )
            if status.nozzle_diameter is None and prev and prev.nozzle_diameter is not None:
                status.nozzle_diameter = prev.nozzle_diameter
        else:
            status.online_since = None
        self._statuses[printer_id] = status

    def serialize(self, stats_by_printer: dict[str, dict[str, float]] | None = None) -> list[dict[str, Any]]:
        out: list[dict[str, Any]] = []
        now = time.time()
        stats = stats_by_printer or {}
        for p in self.list_printers():
            st = self.get_status(p.id)
            p_stats = stats.get(p.id, {})
            total_hours = round(p.base_print_hours + float(p_stats.get("hours", 0.0)), 1)
            # ~330 m per kg of 1.75mm PLA/PETG -> 0.00033 km per gram
            total_km = round(p.base_filament_km + float(p_stats.get("grams", 0.0)) * 0.00033, 2)
            on_time_s = int(max(0, now - st.online_since)) if (st.online_since and st.state != "OFFLINE") else None
            out.append(
                {
                    "slot": p.slot,
                    "id": p.id,
                    "name": p.name,
                    "model": p.model,
                    "ip": p.ip,
                    "buildVolume": p.build_volume,
                    "state": st.state,
                    "canAcceptJob": st.can_accept_job,
                    "progress": st.progress,
                    "activeJob": st.active_job,
                    "nozzleTemp": st.nozzle_temp,
                    "targetNozzle": st.target_nozzle,
                    "bedTemp": st.bed_temp,
                    "targetBed": st.target_bed,
                    "nozzleDiameter": st.nozzle_diameter,
                    "timeRemainingSeconds": st.time_remaining_s,
                    "onTimeSeconds": on_time_s,
                    "totalPrintHours": total_hours if total_hours > 0 else None,
                    "totalFilamentKm": total_km if total_km > 0 else None,
                    "powerState": st.power_state,
                    "pendingShutdown": st.pending_shutdown,
                    "shutdownTempThresholdC": st.shutdown_temp_threshold_c,
                    "error": st.error,
                    "updatedAt": st.updated_at,
                }
            )
        return out
