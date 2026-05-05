# 🛠️ easyVerwaltung Hardware

Central technical documentation and firmware for the **easyVerwaltung** hardware system.
The system manages machine access, usage tracking, and safety via RFID authentication and backend validation.


## 🏗️ System Architecture

The system follows a modular design:

* **Machine Nodes (ESP32 / Pico W)**
  Control individual machines (RFID, safety interlocks, power switching)

* **Smart Terminal (Raspberry Pi)**
  User interface for account management and system status

* **easyAPI**
  Backend communication layer for authentication and session handling


## 📁 Repository Structure

```text
firmware/
  projects/        # Buildable firmware targets
    RFIDBOX_PoC/   # Working proof of concept
    MachineNodes/  # Target structure for production nodes

  shared/          # Reusable libraries
    easyAPI/       # Backend communication (HTTP / JSON)
    drivers/       # Hardware-level drivers (RFID, GPIO, etc.)
    utils/         # General utilities (logging, helpers)
```

### Structure Principles

* `projects/` contains **complete firmware applications**
* `shared/` contains **reusable modules used across projects**
* Each module in `shared/` follows:

```
<module>/
  src/
  include/
```


## 📦 Modules

| Module           | Description                              | TechStack                  |
| ---------------- | ---------------------------------------- | -------------------------- |
| **RFIDBOX_PoC**  | Functional reference implementation      | ESP32 / Arduino / C++      |
| **MachineNodes** | Intended production firmware structure   | RP2040 (Pico W) / C/C++    |
| **easyAPI**      | Backend communication + session handling | C (embedded) / HTTP / JSON |


## 🚀 Setup (macOS & Linux)

### Prerequisites

* Git
* Python ≥ 3.8
* PlatformIO Core

Install PlatformIO:

```bash
pip install platformio
```


### Clone Repository

```bash
git clone <REPO_URL>
cd <REPO_NAME>
```


### Configuration (Secrets)

For the RFIDBOX project:

```bash
cd firmware/projects/RFIDBOX_PoC
cp include/secret.h.example include/secret.h
```

Edit `include/secret.h` and provide your credentials.

**Important:**
Do not commit this file.


## 🔧 Build

```bash
cd firmware/projects/RFIDBOX_PoC
pio run
```


## ⬆️ Upload (optional)

```bash
pio run --target upload
```


## 🔗 Using Shared Libraries

In each `platformio.ini`:

```ini
lib_extra_dirs =
  ../../shared
```

Example usage:

```cpp
#include <easy_api.h>
```

## 📚 Documentation

### Requirements

* Doxygen
* Graphviz

Install:

**macOS**

```bash
brew install doxygen graphviz
```

**Linux (Debian/Ubuntu)**

```bash
sudo apt install doxygen graphviz
```


### Generate Docs

```bash
doxygen Doxyfile
```

## ⚠️ Notes

* The repository reflects a **target architecture**
* `RFIDBOX_PoC` is the current working reference
* `MachineNodes` is the intended production structure


