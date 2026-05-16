#!/usr/bin/env python3
"""
Dev-Mode Boot Screen.
Zeigt Verbindungsinfos (Hostname, IP, SSH-Befehl) auf dem DSI-Display.
Läuft als Systemd-Service vor dem Kiosk-Browser.
Chromium öffnet diese Seite und leitet nach 30s automatisch zu localhost weiter —
genug Zeit damit der Docker-Stack hochfährt.
"""
import socket
import subprocess
from pathlib import Path


def get_ip() -> str:
    try:
        s = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
        s.connect(("8.8.8.8", 80))
        ip = s.getsockname()[0]
        s.close()
        return ip
    except OSError:
        return "–"


def get_ssh_user() -> str:
    try:
        result = subprocess.run(["logname"], capture_output=True, text=True)
        return result.stdout.strip() or "pi"
    except Exception:
        return "pi"


hostname = socket.gethostname()
ip       = get_ip()
user     = get_ssh_user()
ssh_cmd  = f"ssh {user}@{hostname}.local"

html = f"""<!DOCTYPE html>
<html lang="de">
<head>
  <meta charset="utf-8">
  <meta http-equiv="refresh" content="30;url=http://localhost">
  <title>Dev Kit</title>
  <style>
    *, *::before, *::after {{ box-sizing: border-box; margin: 0; padding: 0; }}
    body {{
      background: #0f172a;
      color: #f1f5f9;
      font-family: ui-monospace, monospace;
      height: 100dvh;
      display: flex;
      align-items: center;
      justify-content: center;
    }}
    .card {{
      background: #1e293b;
      border: 1px solid #334155;
      border-radius: 16px;
      padding: 48px 64px;
      min-width: 420px;
      text-align: center;
    }}
    .badge {{
      display: inline-block;
      background: #1d4ed8;
      color: #bfdbfe;
      font-size: 0.7em;
      padding: 3px 10px;
      border-radius: 999px;
      letter-spacing: 0.08em;
      text-transform: uppercase;
      margin-bottom: 20px;
    }}
    h1 {{
      font-size: 1.25em;
      color: #e2e8f0;
      margin-bottom: 32px;
    }}
    .row {{ margin-top: 18px; }}
    .label {{
      font-size: 0.72em;
      color: #64748b;
      text-transform: uppercase;
      letter-spacing: 0.06em;
    }}
    .value {{
      font-size: 1.05em;
      color: #cbd5e1;
      margin-top: 3px;
    }}
    .cmd {{
      display: inline-block;
      background: #0f172a;
      color: #4ade80;
      padding: 8px 18px;
      border-radius: 8px;
      margin-top: 6px;
      font-size: 1em;
      letter-spacing: 0.03em;
    }}
    .footer {{
      margin-top: 36px;
      font-size: 0.72em;
      color: #475569;
    }}
  </style>
</head>
<body>
  <div class="card">
    <div class="badge">Dev Kit</div>
    <h1>Smart Terminal 3D-Print</h1>

    <div class="row">
      <div class="label">Hostname</div>
      <div class="value">{hostname}.local</div>
    </div>

    <div class="row">
      <div class="label">IP-Adresse</div>
      <div class="value">{ip}</div>
    </div>

    <div class="row">
      <div class="label">VS Code → Remote SSH</div>
      <div class="cmd">{ssh_cmd}</div>
    </div>

    <div class="footer">
      Weiter zu localhost in 30 Sekunden &mdash; Docker-Stack startet&nbsp;&hellip;
    </div>
  </div>
</body>
</html>"""

out = Path("/tmp/devinfo.html")
out.write_text(html)

# Chromium im Kiosk-Modus mit der Info-Seite öffnen
subprocess.Popen([
    "chromium-browser",
    "--kiosk",
    "--noerrdialogs",
    "--disable-infobars",
    "--no-first-run",
    f"file://{out}",
])
