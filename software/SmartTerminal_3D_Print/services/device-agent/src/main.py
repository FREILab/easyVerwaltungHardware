from __future__ import annotations

import asyncio
import logging
import os
import time
from contextlib import asynccontextmanager
from pathlib import Path
from typing import Any

from fastapi import FastAPI, WebSocket, WebSocketDisconnect
from pydantic import BaseModel, Field

from pn532_spi import Pn532Poller

logging.basicConfig(level=logging.INFO, format="%(asctime)s %(levelname)s [device-agent] %(message)s")
logger = logging.getLogger("st3d.device-agent")

HARDWARE_MODE = os.getenv("HARDWARE_MODE", "mock").strip().lower() or "mock"
NFC_SPI_BUS = int(os.getenv("NFC_SPI_BUS", "0"))
NFC_SPI_DEVICE = int(os.getenv("NFC_SPI_DEVICE", "0"))
NFC_CS_GPIO = int(os.getenv("NFC_CS_GPIO", "5"))  # Pin 29 (GPIO5) auf HW-147 SPI-Aufbau; -1 = Hardware CE0
NFC_GPIO_CHIP = os.getenv("NFC_GPIO_CHIP", "/dev/gpiochip0").strip() or "/dev/gpiochip0"
NFC_SPI_SPEED_HZ = int(os.getenv("NFC_SPI_SPEED_HZ", "250000"))

nfc_poller: Pn532Poller | None = None
_main_loop: asyncio.AbstractEventLoop | None = None


@asynccontextmanager
async def lifespan(_app: FastAPI):
    global nfc_poller, _main_loop
    _main_loop = asyncio.get_running_loop()
    if HARDWARE_MODE == "pi":
        def _on_nfc_scan(uid: str) -> None:
            if _main_loop is not None:
                asyncio.run_coroutine_threadsafe(
                    hub.broadcast({"type": "rfid_scan", "uid": uid, "ts": time.time(), "source": "pn532"}),
                    _main_loop,
                )

        nfc_poller = Pn532Poller(
            on_scan=_on_nfc_scan,
            spi_bus=NFC_SPI_BUS,
            spi_device=NFC_SPI_DEVICE,
            cs_gpio=NFC_CS_GPIO if NFC_CS_GPIO >= 0 else None,
            gpio_chip=NFC_GPIO_CHIP,
            speed_hz=NFC_SPI_SPEED_HZ,
        )
        nfc_poller.start()
    yield
    if nfc_poller is not None:
        nfc_poller.stop()


app = FastAPI(title="Smart Terminal Device Agent", version="0.3.0", lifespan=lifespan)


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
        "nfc": nfc_poller.status_dict() if nfc_poller is not None else {"connected": False, "mode": HARDWARE_MODE},
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
