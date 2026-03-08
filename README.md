/** @mainpage easyVerwaltung Hardware Documentation */

# 🛠️ easyVerwaltung Hardware

Welcome to the central technical documentation for the **easyVerwaltung** hardware ecosystem. This repository contains the firmware and hardware specifications for the access control units used in the workshop environment.

The system is designed to manage machine permissions, track usage, and ensure safety via RFID-based authentication and physical presence detection.

---

## 🏗️ System Architecture

The ecosystem is built on a modular "Stack-Design", separating logic from power electronics to ensure safety and maintainability.



* **Machine Nodes (ESP32/Pico W):** Distributed controllers connected to machines. They handle RFID input, safety interlocks, and power switching (230V/24V).
* **Smart Terminal (Raspberry Pi):** Central user interaction point with a touch interface for account management and machine status.
* **Backend API:** The central "source of truth" that validates permissions and logs activities.



---

## 📁 Project Modules

This documentation is split into several modules based on the device type. You can find detailed API references and state machine logic in the **Modules** tab.

| Module | Description | Tech Stack |
| :--- | :--- | :--- |
| **\ref RFIDBOX_POC** | Proof of Concept firmware for machine nodes. | ESP32 / Arduino / C++ |
| **\ref MachineNodes** | Production-ready firmware for the specialized Node-PCBs. | RP2040 (Pico W) / C++ |
| **\ref ProjectAPI** | Shared communication library for backend requests. | C++ / JSON API |

---

## 🚀 Getting Started (Development)

### 1. Prerequisites
To work with this repository on your MacBook, you need:
* **VS Code** with the **PlatformIO** extension.
* **Doxygen** & **Graphviz** (installed via `brew install doxygen graphviz`).

### 2. Configuration (Secrets)
The firmware requires a `secret.h` file in the `include/` directory. 
1. Copy `include/secret.example` to `include/secret.h`.
2. Fill in your WiFi credentials and the Backend API Token.
3. **Note:** Never commit your `secret.h` to version control.

### 3. Building Documentation
To update the local HTML documentation, run the following command in the root directory:
```bash
doxygen Doxyfile