from __future__ import annotations

import asyncio
import json
import time
from collections.abc import AsyncIterator
from typing import Any

from fastapi import Request


class EventBus:
    """In-memory pub/sub hub for Server-Sent Events (SSE) to the Kiosk UI."""

    def __init__(self) -> None:
        self._subscribers: set[asyncio.Queue[dict[str, Any] | None]] = set()
        self._closing = False

    def publish(self, event_type: str, data: dict[str, Any] | None = None) -> None:
        if self._closing:
            return
        payload = {"type": event_type, "ts": time.time(), "data": data or {}}
        for q in list(self._subscribers):
            try:
                q.put_nowait(payload)
            except asyncio.QueueFull:
                try:
                    q.get_nowait()
                    q.put_nowait(payload)
                except Exception:
                    pass

    def shutdown(self) -> None:
        """Unblock all active SSE streams immediately so reload/restart never hangs."""
        self._closing = True
        for q in list(self._subscribers):
            try:
                q.put_nowait(None)
            except asyncio.QueueFull:
                try:
                    q.get_nowait()
                    q.put_nowait(None)
                except Exception:
                    pass

    async def subscribe(self, request: Request | None = None) -> AsyncIterator[str]:
        q: asyncio.Queue[dict[str, Any] | None] = asyncio.Queue(maxsize=64)
        self._subscribers.add(q)
        try:
            yield f"event: connected\ndata: {json.dumps({'ok': True})}\n\n"
            while not self._closing:
                if request is not None and await request.is_disconnected():
                    break
                try:
                    msg = await asyncio.wait_for(q.get(), timeout=5.0)
                    if msg is None:
                        break
                    yield f"event: {msg['type']}\ndata: {json.dumps(msg, ensure_ascii=False)}\n\n"
                except asyncio.TimeoutError:
                    yield ": keepalive\n\n"
        except asyncio.CancelledError:
            pass
        finally:
            self._subscribers.discard(q)
