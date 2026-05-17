#pragma once

/* Kopiere diese Datei nach config.h und fülle die Werte aus.
 * config.h ist in .gitignore und wird nie committed. */

/* WiFi-Zugangsdaten */
#define WIFI_SSID     "mein-netzwerk"
#define WIFI_PASSWORD "mein-passwort"

/* Backend-Adresse und Authentifizierung */
#define SERVER_HOST   "192.168.1.100"
#define SERVER_PORT   8080
#define AUTH_TOKEN    "mein-token"

/* Geräte-Identifikation */
#define MACHINE_ID    "machinenode-01"
#define MACHINE_NAME  "example-machine"

/* OTA-Server: HTTP-Endpunkt, der das neueste Firmware-Binary liefert */
#define OTA_HOST      SERVER_HOST
#define OTA_PORT      SERVER_PORT
#define OTA_PATH      "/api/firmware/machine-node/latest.bin"
