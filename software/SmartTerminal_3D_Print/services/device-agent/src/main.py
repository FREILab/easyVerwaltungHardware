from collections.abc import AsyncIterator
import os

from fastapi import FastAPI, WebSocket

app = FastAPI(title="Smart Terminal Device Agent", version="0.1.0-scaffold")


@app.get("/health")
def health() -> dict:
    return {"status": "ok", "service": "device-agent", "mode": "scaffold"}


@app.post("/commands/beeper")
def beeper_command() -> dict:
    return {
        "accepted": False,
        "note": "GPIO/I2C/SPI Hardwarezugriff wird spaeter implementiert.",
    }


async def fake_events() -> AsyncIterator[dict]:
    yield {"type": "boot", "source": "device-agent", "mode": "scaffold"}


@app.websocket("/ws/events")
async def events(ws: WebSocket) -> None:
    await ws.accept()
    async for event in fake_events():
        await ws.send_json(event)
    await ws.close()


if __name__ == "__main__":
    import uvicorn

    host = os.getenv("DEVICE_AGENT_HOST", "0.0.0.0")
    port = int(os.getenv("DEVICE_AGENT_PORT", "8081"))
    uvicorn.run(app, host=host, port=port)
