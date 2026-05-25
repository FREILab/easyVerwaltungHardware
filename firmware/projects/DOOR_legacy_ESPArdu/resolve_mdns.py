# PlatformIO extra_script: löst .local mDNS-Hostnamen vor dem OTA-Upload auf.
# macOS: Python's socket nutzt nicht Bonjour → dns-sd als Workaround.

import re
import subprocess
import threading

Import("env")  # noqa: F821  (PlatformIO-Kontext)


def resolve_via_dns_sd(hostname, timeout=6):
    """Nutzt dns-sd (macOS) um einen .local-Hostnamen in eine IPv4-Adresse aufzulösen."""
    result = [None]

    def lookup():
        try:
            proc = subprocess.Popen(
                ["dns-sd", "-G", "v4", hostname],
                stdout=subprocess.PIPE,
                stderr=subprocess.DEVNULL,
                text=True,
            )
            for line in proc.stdout:
                m = re.search(r"\b(\d{1,3}\.\d{1,3}\.\d{1,3}\.\d{1,3})\b", line)
                if m and not line.strip().endswith("(more to come)"):
                    result[0] = m.group(1)
                    proc.terminate()
                    return
        except Exception:
            pass

    t = threading.Thread(target=lookup, daemon=True)
    t.start()
    t.join(timeout)
    return result[0]


upload_port = env.GetProjectOption("upload_port", "")  # noqa: F821

if upload_port.endswith(".local"):
    print(f"mDNS: löse {upload_port} auf...")
    ip = resolve_via_dns_sd(upload_port)
    if ip:
        print(f"mDNS: {upload_port} → {ip}")
        env.Replace(UPLOAD_PORT=ip)
    else:
        print(f"mDNS: Auflösung fehlgeschlagen, versuche trotzdem mit {upload_port}")
