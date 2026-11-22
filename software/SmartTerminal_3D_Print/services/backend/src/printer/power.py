from __future__ import annotations

import logging
from dataclasses import dataclass
from typing import Any

import httpx

from printer.registry import PrinterConfig

logger = logging.getLogger("st3d.power")


@dataclass
class PowerActionResult:
    ok: bool
    reason: str = ""


class PowerControlService:
    """Switches printer mains power via easyVerwaltung (mock or live)."""

    async def power_on(
        self,
        printer: PrinterConfig,
        *,
        rfid: str,
        user: dict[str, Any] | None = None,
    ) -> PowerActionResult:
        raise NotImplementedError

    async def power_off(
        self,
        printer: PrinterConfig,
        *,
        reason: str = "USER_REQUEST",
    ) -> PowerActionResult:
        raise NotImplementedError


class MockPowerControlService(PowerControlService):
    """Local simulation of easyVerwaltung printer power switching."""

    async def power_on(
        self,
        printer: PrinterConfig,
        *,
        rfid: str,
        user: dict[str, Any] | None = None,
    ) -> PowerActionResult:
        who = (user or {}).get("display_name", "Nutzer")
        logger.info("[MOCK] Power ON printer %s (%s) authorized by %s", printer.id, printer.name, who)
        return PowerActionResult(ok=True)

    async def power_off(
        self,
        printer: PrinterConfig,
        *,
        reason: str = "USER_REQUEST",
    ) -> PowerActionResult:
        logger.info("[MOCK] Power OFF printer %s (%s) reason=%s", printer.id, printer.name, reason)
        return PowerActionResult(ok=True)


class EasyVerwaltungPowerControlService(PowerControlService):
    """Live client forwarding printer power on/off requests to easyVerwaltung.

    Fail-closed on timeouts/errors so power state never silently drifts.
    """

    def __init__(self, base_url: str, service_id: str, service_token: str) -> None:
        self._base_url = base_url.rstrip("/")
        self._service_id = service_id
        self._service_token = service_token

    async def _post(self, path: str, payload: dict[str, Any]) -> PowerActionResult:
        if not self._base_url or not self._service_token:
            return PowerActionResult(ok=False, reason="EASYVERWALTUNG_NOT_CONFIGURED")
        url = f"{self._base_url}{path}"
        headers = {
            "X-Service-Token": self._service_token,
            "Content-Type": "application/json",
            "Accept": "application/json",
        }
        try:
            async with httpx.AsyncClient(timeout=8.0) as client:
                resp = await client.post(url, json=payload, headers=headers)
            if resp.status_code == 200:
                data = resp.json() if resp.content else {}
                if data.get("ok", True) and data.get("authorized", True):
                    return PowerActionResult(ok=True)
                return PowerActionResult(ok=False, reason=str(data.get("reason", "POWER_DENIED")))
            logger.warning("easyVerwaltung power call %s failed: HTTP %d", path, resp.status_code)
            return PowerActionResult(ok=False, reason=f"EASYVERWALTUNG_HTTP_{resp.status_code}")
        except httpx.TimeoutException:
            return PowerActionResult(ok=False, reason="EASYVERWALTUNG_TIMEOUT")
        except Exception as exc:
            logger.error("easyVerwaltung power call error: %s", exc)
            return PowerActionResult(ok=False, reason="EASYVERWALTUNG_UNREACHABLE")

    async def power_on(
        self,
        printer: PrinterConfig,
        *,
        rfid: str,
        user: dict[str, Any] | None = None,
    ) -> PowerActionResult:
        target_id = printer.power_control_id or printer.id
        return await self._post(
            "/api/service/machine/power",
            {
                "serviceId": self._service_id,
                "machineId": target_id,
                "action": "on",
                "rfid": rfid,
            },
        )

    async def power_off(
        self,
        printer: PrinterConfig,
        *,
        reason: str = "USER_REQUEST",
    ) -> PowerActionResult:
        target_id = printer.power_control_id or printer.id
        return await self._post(
            "/api/service/machine/power",
            {
                "serviceId": self._service_id,
                "machineId": target_id,
                "action": "off",
                "reason": reason,
            },
        )
