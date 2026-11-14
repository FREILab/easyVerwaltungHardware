"""Entry point: python -m st3d"""

from __future__ import annotations

import logging
import signal
import sys
from pathlib import Path

from PySide6.QtCore import QUrl
from PySide6.QtGui import QGuiApplication
from PySide6.QtQml import QQmlApplicationEngine

from . import config
from .system_info import SystemInfo

QML_DIR = Path(__file__).resolve().parent / "qml"


def main() -> int:
    logging.basicConfig(level=logging.INFO, format="%(levelname)s %(name)s: %(message)s")
    log = logging.getLogger("st3d")

    cfg = config.load()
    log.info("Konfiguration: %s, Drehung %d°", cfg.source, cfg.rotation)

    app = QGuiApplication(sys.argv)
    app.setApplicationName("st3d")
    # Let Ctrl+C / SIGTERM end the Qt event loop cleanly.
    signal.signal(signal.SIGINT, lambda *_: app.quit())
    signal.signal(signal.SIGTERM, lambda *_: app.quit())

    system_info = SystemInfo()

    engine = QQmlApplicationEngine()
    engine.setInitialProperties({"rotation_": cfg.rotation, "systemInfo": system_info})
    engine.load(QUrl.fromLocalFile(str(QML_DIR / "Main.qml")))
    if not engine.rootObjects():
        log.error("QML konnte nicht geladen werden")
        return 1

    return app.exec()


if __name__ == "__main__":
    sys.exit(main())
