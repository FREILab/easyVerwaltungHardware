# Smart Terminal 3D-Print

Konzept: [KONZEPT_ESSENTIALS.md](KONZEPT_ESSENTIALS.md)

## Ersteinrichtung Pi

1. **Flashen** mit Raspberry Pi Imager:
   - Raspberry Pi OS Lite (64-bit), Debian 13 (trixie)
   - Hostname `3d-print-server`
   - Benutzer anlegen, SSH mit öffentlichem Schlüssel aktivieren
   - WLAN nur, falls kein LAN
2. **Verbinden** per VS Code Remote-SSH auf `<benutzer>@3d-print-server.local`.
3. **Repo holen** (nur dieser Ordner):
   ```bash
   sudo install -d -o "$USER" -g "$USER" /opt/easyVerwaltungHardware
   git clone --filter=blob:none --sparse https://github.com/FREILab/easyVerwaltungHardware.git /opt/easyVerwaltungHardware
   cd /opt/easyVerwaltungHardware
   git sparse-checkout set software/SmartTerminal_3D_Print
   git switch <branch oder tag>
   ```
4. **Installieren:**
   ```bash
   cd /opt/easyVerwaltungHardware/software/SmartTerminal_3D_Print
   ./install.sh
   ```
5. **Konfiguration** in `/etc/st3d/config.toml` anpassen, danach `sudo systemctl restart st3d`.

## Betrieb

| Aufgabe | Befehl |
|---|---|
| Status | `systemctl status st3d` |
| Logs live | `journalctl -u st3d -f` |
| Neustart | `sudo systemctl restart st3d` |

## Entwicklung am Mac

```bash
cd software/SmartTerminal_3D_Print
python3 -m venv .venv
.venv/bin/pip install -r requirements.txt
PYTHONPATH=src .venv/bin/python -m st3d
```

Konfiguration: `config.toml` im Projektordner oder `ST3D_CONFIG=<pfad>`.
