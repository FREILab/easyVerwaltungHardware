#ifndef SETTINGS_H
#define SETTINGS_H

/**
 * MASCHINEN-KONFIGURATION
 * Name (Typ):  tuer, lasercutter, LaserXTool, 3dprinter, metal-mill, lathe, embroiderymachine,
 * cncmill-wood, cncmill-metal, 3dprinter-xl, panel-saw, wood-planer, 
 * wood-bandsaw, miter-saw, wood-routertable
 */
#define MACHINE_NAME "cncmill-wood"              // Typ aus der Liste oben wählen
#define MACHINE_ID   "HolzCNC"                   // Eindeutige Kennung dieser Hardware-Box
#define DEVICE_NAME  "rfid-holz"           // mDNS Hostname für OTA Updates (z.B. "DEVICE_NAME.local")
#define RFIDCARD_AUTH_CONST true                 // true = Card-Auth aktiv
#define CONTINUOUS_SERVER_CHECK false            // true = alle 2s Server-Auth-Check im RUNNING (nur mit RFIDCARD_AUTH_CONST)

#endif