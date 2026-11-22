from __future__ import annotations

import asyncio
import json
import logging
from collections.abc import Callable
from typing import Any

import httpx
import websockets

logger = logging.getLogger("st3d.device")


class DeviceAgentClient:
    """Resilient client for the local device-agent container (beeper, LED ring, RFID WS events)."""

    def __init__(self, base_url: str, on_rfid_scan: Callable[[str], None]) -> None:
        self.base_url = base_url.rstrip("/")
        self.on_rfid_scan = on_rfid_scan
        self.connected = False
        self._stop = asyncio.Event()

    async def send_beeper(self, pattern: str = "ok") -> None:
        try:
            async with httpx.AsyncClient(timeout=1.5) as client:
                await client.post(f"{self.base_url}/commands/beeper", json={"pattern": pattern})
        except Exception:
            pass

    async def send_led(self, mode: str = "idle") -> None:
        try:
            async with httpx.AsyncClient(timeout=1.5) as client:
                await client.post(f"{self.base_url}/commands/led", json={"mode": mode})
        except Exception:
            pass

    async def trigger_mock_rfid(self, uid: str) -> bool:
        try:
            async with httpx.AsyncClient(timeout=2.0) as client:
                resp = await client.post(f"{self.base_url}/mock/rfid", json={"uid": uid})
                return resp.status_code == 200
        except Exception:
            return False

    async def run_ws_listener(self) -> None:
        ws_url = self.base_url.replace("http://", "ws://").replace("https://", "wss://") + "/ws/events"
        backoff = 1.0
        while not self._stop.is_set():
            try:
                async with websockets.connect(ws_url, ping_interval=20, ping_timeout=10) as ws:
                    self.connected = True
                    backoff = 1.0
                    logger.info("Connected to device-agent WebSocket: %s", ws_url)
                    async for raw in ws:
                        if self._stop.is_set():
                            break
                        try:
                            msg: dict[str, Any] = json.loads(raw)
                        except json.JSONDecodeError:
                            continue
                        if msg.get("type") == "rfid_scan" and msg.get("uid"):
                            self.on_rfid_scan(str(msg["uid"]))
            except asyncio.CancelledError:
                break
            except Exception as exc:
                self.connected = False
                logger.debug("device-agent WS disconnected (%s), retrying in %.1fs", exc, backoff)
                try:
                    await asyncio.wait_for(self._stop.wait(), timeout=backoff)
                except asyncio.TimeoutError:
                    pass
                backoff = min(backoff * 2, 15.0)
        self.connected = False

    def stop(self) -> None:
        self._stop.set()
