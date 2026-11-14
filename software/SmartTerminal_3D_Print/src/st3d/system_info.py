"""Device information shown on screen (hostname, addresses, version)."""

from __future__ import annotations

import socket
import subprocess
from pathlib import Path

from PySide6.QtCore import Property, QObject, QTimer, Signal

APP_DIR = Path(__file__).resolve().parents[2]


def _version() -> str:
    try:
        result = subprocess.run(
            ["git", "-C", str(APP_DIR), "describe", "--tags", "--match", "st3d-v*",
             "--always", "--dirty"],
            capture_output=True,
            text=True,
            timeout=2,
            check=True,
        )
        return result.stdout.strip()
    except (OSError, subprocess.SubprocessError):
        return "unbekannt"


def _addresses() -> list[str]:
    try:
        result = subprocess.run(
            ["hostname", "-I"], capture_output=True, text=True, timeout=2, check=True
        )
        return result.stdout.split()
    except (OSError, subprocess.SubprocessError):
        # macOS has no "hostname -I"; resolve the outgoing interface instead.
        try:
            with socket.socket(socket.AF_INET, socket.SOCK_DGRAM) as probe:
                probe.connect(("192.0.2.1", 9))
                return [probe.getsockname()[0]]
        except OSError:
            return []


class SystemInfo(QObject):
    changed = Signal()

    def __init__(self, parent: QObject | None = None) -> None:
        super().__init__(parent)
        self._hostname = socket.gethostname()
        self._version = _version()
        self._addresses: list[str] = []
        self._refresh()
        self._timer = QTimer(self)
        self._timer.timeout.connect(self._refresh)
        self._timer.start(5000)

    def _refresh(self) -> None:
        addresses = _addresses()
        if addresses != self._addresses:
            self._addresses = addresses
            self.changed.emit()

    @Property(str, notify=changed)
    def hostname(self) -> str:
        return self._hostname

    @Property(str, notify=changed)
    def version(self) -> str:
        return self._version

    @Property(str, notify=changed)
    def addresses(self) -> str:
        return ", ".join(self._addresses) if self._addresses else "keine Netzwerkverbindung"
