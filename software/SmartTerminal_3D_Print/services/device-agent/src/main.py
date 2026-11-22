from __future__ import annotations

import asyncio
import logging
import os
import time
from typing import Any

from fastapi import FastAPI, WebSocket, WebSocketDisconnect
from pydantic import BaseModel

logging.basicConfig(level=logging.INFO, format="%(asctime)s %(levelname)s [device-agent] %(message)s")
logger = logging.getLogger("st3d.device-agent")

HARDWARE_MODE = os.getenv("HARDWARE_MODE", "mock").strip().lower() or "mock"

app = FastAPI(title="Smart Terminal Device Agent", version="0.2.0")


class EventHub:
    def __init__(self) -> None:
        self._clients: set[WebSocket] = set()

    async def connect(self, ws: WebSocket) -> None:
        await ws.accept()
        self._clients.add(ws)

    def disconnect(self, ws: WebSocket) -> None:
        self._clients.discard(ws)

    async def broadcast(self, event: dict[str, Any]) -> None:
        dead: list[WebSocket] = []
        for ws in list(self._clients):
            try:
                await ws.send_json(event)
            except Exception:
                dead.append(ws)
        for ws in dead:
            self._clients.discard(ws)


hub = EventHub()
current_led_mode = "idle"


class BeeperCommand(BaseModel):
    pattern: str = "ok"


class LedCommand(BaseModel):
    mode: str = "idle"


class MockRfidScan(BaseModel):
    uid: str


@app.get("/health")
def health() -> dict[str, Any]:
    return {
        "status": "ok",
        "service": "device-agent",
        "hardwareMode": HARDWARE_MODE,
        "ledMode": current_led_mode,
        "wsClients": len(hub._clients),
    }


@app.post("/commands/beeper")
def beeper_command(cmd: BeeperCommand) -> dict[str, Any]:
    logger.info("Beeper command (mode=%s): %s", HARDWARE_MODE, cmd.pattern)
    return {"accepted": True, "pattern": cmd.pattern, "hardwareMode": HARDWARE_MODE}


@app.post("/commands/led")
def led_command(cmd: LedCommand) -> dict[str, Any]:
    global current_led_mode
    current_led_mode = cmd.mode
    logger.info("LED command (mode=%s): %s", HARDWARE_MODE, cmd.mode)
    return {"accepted": True, "mode": cmd.mode, "hardwareMode": HARDWARE_MODE}


@app.post("/mock/rfid")
async def mock_rfid(scan: MockRfidScan) -> dict[str, Any]:
    uid = scan.uid.strip().upper()
    await hub.broadcast({"type": "rfid_scan", "uid": uid, "ts": time.time(), "source": "mock"})
    return {"ok": True, "broadcasted": True}


@app.websocket("/ws/events")
async def events(ws: WebSocket) -> None:
    await hub.connect(ws)
    try:
        await ws.send_json({"type": "connected", "hardwareMode": HARDWARE_MODE, "ts": time.time()})
        while True:
            try:
                await asyncio.wait_for(ws.receive_text(), timeout=30.0)
            except asyncio.TimeoutError:
                await ws.send_json({"type": "ping", "ts": time.time()})
    except WebSocketDisconnect:
        pass
    finally:
        hub.disconnect(ws)


if __name__ == "__main__":
    import uvicorn

    host = os.getenv("DEVICE_AGENT_HOST", "0.0.0.0")
    port = int(os.getenv("DEVICE_AGENT_PORT", "8081"))
    uvicorn.run(app, host=host, port=port)
