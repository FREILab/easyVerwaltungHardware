# RFIDBOX PoC Firmware

## OTA Credentials Workflow

This firmware uses two sets of OTA credentials:

| Credential Set | Purpose | Defines |
|----------------|---------|---------|
| **Uploader** (`OTA_USERNAME`, `OTA_PASSWORD`) | Used by `platformio_upload.py` to authenticate against the *running* device | `secret.h` |
| **Firmware** (`OTA_BUILD_USERNAME`, `OTA_BUILD_PASSWORD`) | Baked into firmware, used by the device after reboot | `secret.h` |

### How It Works

1. **Upload sequence**: The uploader sends `OTA_USERNAME:OTA_PASSWORD` to authenticate against the device currently running
2. **New credentials**: After a successful upload, the firmware (baked into the new binary) uses `OTA_BUILD_USERNAME:OTA_BUILD_PASSWORD`
3. **Enforcing new credentials**: A **manual reboot via EN pin is required** after uploading new firmware to enforce the new credentials

### Setting New Firmware Credentials

In `include/secret.h`:

```cpp
// Uploader credentials (must match currently running firmware)
#define OTA_USERNAME "freilab-admin"
#define OTA_PASSWORD "changeme1"

// Firmware credentials (baked into firmware, used after reboot)
#define OTA_BUILD_USERNAME "freilab-admin-fw"
#define OTA_BUILD_PASSWORD "0u22qVq6W9upqyJPorui0hsfx9mljl8X"
```

### Changing Credentials Step-by-Step

1. Set `OTA_USERNAME`/`OTA_PASSWORD` to match what the *running* device expects
2. Set `OTA_BUILD_USERNAME`/`OTA_BUILD_PASSWORD` to the *new* credentials you want baked into firmware
3. Upload using the `pio` command (see below)
4. **Press the EN pin** on the ESP32 to trigger a hard reset and enforce new credentials
5. Subsequent uploads use the new uploader credentials (same as what the firmware now expects)

---

## Upload Methods

### Recommended: PlatformIO CLI

The **Upload button in PlatformIO IDE does NOT work** for OTA uploads. Use the command line:

```bash
pio run --target upload --environment esp32dev-ota
```

### Manual EN Reboot

After uploading new firmware, the device may not automatically reboot. To enforce the new credentials:

1. Locate the **EN** pin on the ESP32 module
2. Momentarily short **EN** to **GND** (or press the EN button if available)
3. The device will hard reset and run the new firmware with updated credentials

---

## Project Structure

```
include/
  secret.h     # Credentials and local configuration (git-ignored)
  ota.h        # OTA update module
  settings.h   # Default settings
src/
  main.cpp     # Main application
platformio.ini # Project configuration
platformio_upload.py # Custom OTA upload script
```
