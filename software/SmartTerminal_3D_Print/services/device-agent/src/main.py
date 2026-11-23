from __future__ import annotations

import asyncio
import logging
import os
import time
from pathlib import Path
from typing import Any

from fastapi import FastAPI, WebSocket, WebSocketDisconnect
from pydantic import BaseModel, Field

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
current_led_progress: int | None = None
current_backlight_percent = 100


def _find_backlight_dir() -> Path | None:
    root = Path("/sys/class/backlight")
    if not root.exists():
        return None
    for entry in sorted(root.iterdir()):
        if (entry / "brightness").exists() and (entry / "max_brightness").exists():
            return entry
    return None


def _apply_hardware_backlight(percent: int) -> tuple[bool, int | None, int | None]:
    bl = _find_backlight_dir()
    if bl is None:
        return False, None, None
    try:
        max_val = max(1, int((bl / "max_brightness").read_text(encoding="utf-8").strip()))
        clamped = max(0, min(100, int(percent)))
        raw = int(round(max_val * clamped / 100.0))
        (bl / "brightness").write_text(f"{raw}\n", encoding="utf-8")
        return True, raw, max_val
    except Exception as exc:
        logger.warning("Failed to set hardware backlight (%s): %s", bl, exc)
        return False, None, None


class BeeperCommand(BaseModel):
    pattern: str = "ok"  # touch | attention | ok | warn | done | error


class LedCommand(BaseModel):
    mode: str = "idle"  # sleep | dim | idle | rfid | pay | warn | uploading | done | error
    progress: int | None = Field(default=None, ge=0, le=100)


class BacklightCommand(BaseModel):
    percent: int = Field(..., ge=0, le=100)


class MockRfidScan(BaseModel):
    uid: str


@app.get("/health")
def health() -> dict[str, Any]:
    bl = _find_backlight_dir()
    return {
        "status": "ok",
        "service": "device-agent",
        "hardwareMode": HARDWARE_MODE,
        "ledMode": current_led_mode,
        "ledProgress": current_led_progress,
        "backlightPercent": current_backlight_percent,
        "backlightDevice": bl.name if bl else None,
        "wsClients": len(hub._clients),
    }


@app.post("/commands/beeper")
def beeper_command(cmd: BeeperCommand) -> dict[str, Any]:
    if cmd.pattern != "touch":
        logger.info("Beeper command (mode=%s): %s", HARDWARE_MODE, cmd.pattern)
    return {"accepted": True, "pattern": cmd.pattern, "hardwareMode": HARDWARE_MODE}


@app.post("/commands/led")
def led_command(cmd: LedCommand) -> dict[str, Any]:
    global current_led_mode, current_led_progress
    current_led_mode = cmd.mode
    current_led_progress = cmd.progress
    logger.info("LED command (mode=%s): %s progress=%s", HARDWARE_MODE, cmd.mode, cmd.progress)
    return {
        "accepted": True,
        "mode": cmd.mode,
        "progress": cmd.progress,
        "hardwareMode": HARDWARE_MODE,
    }


@app.post("/commands/backlight")
def backlight_command(cmd: BacklightCommand) -> dict[str, Any]:
    global current_backlight_percent
    current_backlight_percent = cmd.percent
    applied, raw, max_val = _apply_hardware_backlight(cmd.percent)
    logger.info(
        "Backlight command: %d%% (hw_applied=%s, raw=%s/%s)",
        cmd.percent,
        applied,
        raw,
        max_val,
    )
    return {
        "accepted": True,
        "percent": cmd.percent,
        "hardwareApplied": applied,
        "raw": raw,
        "max": max_val,
    }


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
