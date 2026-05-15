Import("env")
import socket, base64, sys

OTA_PORT = 65280
AUTH = "Basic " + base64.b64encode(b"arduino:").decode()  # empty password

def resolve_host(host):
    try:
        ip = socket.gethostbyname(host)
        print(f"  Hostname '{host}' -> {ip}")
        return ip
    except socket.gaierror:
        return host  # IP direkt verwenden

def do_ota_upload(source, target, env):
    firmware_path = str(source[0])
    host = resolve_host(env.get("UPLOAD_PORT", ""))

    with open(firmware_path, "rb") as f:
        firmware = f.read()

    size = len(firmware)
    print(f"OTA -> {host}:{OTA_PORT}  ({size} bytes)")

    try:
        sock = socket.create_connection((host, OTA_PORT), timeout=10)
    except Exception as e:
        print(f"Verbindung fehlgeschlagen: {e}", file=sys.stderr)
        env.Exit(1)
        return

    request = (
        f"POST /sketch HTTP/1.1\r\n"
        f"Host: {host}\r\n"
        f"Authorization: {AUTH}\r\n"
        f"Content-Length: {size}\r\n"
        f"Content-Type: application/octet-stream\r\n"
        f"\r\n"
    ).encode()

    sock.sendall(request)

    chunk = 4096
    sent = 0
    while sent < size:
        end = min(sent + chunk, size)
        sock.sendall(firmware[sent:end])
        sent = end
        pct = int(sent * 100 / size)
        print(f"\r  {pct:3d}%  {sent}/{size} bytes", end="", flush=True)
    print()

    sock.settimeout(15)
    try:
        response = b""
        while b"\r\n\r\n" not in response:
            response += sock.recv(256)
        status_line = response.split(b"\r\n")[0].decode()
        print(f"Antwort: {status_line}")
        if "200" not in status_line:
            print("Upload fehlgeschlagen.", file=sys.stderr)
            env.Exit(1)
    except Exception as e:
        print(f"Keine Antwort: {e}", file=sys.stderr)
        env.Exit(1)
    finally:
        sock.close()

env.Replace(UPLOADCMD=do_ota_upload)
