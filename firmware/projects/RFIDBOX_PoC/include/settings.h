#ifndef SETTINGS_H
#define SETTINGS_H

/**
 * MASCHINEN-KONFIGURATION
 * Name (Typ):  tuer, lasercutter, LaserXTool, 3dprinter, metal-mill, lathe, embroiderymachine,
 * cncmill-wood, cncmill-metal, 3dprinter-xl, panel-saw, wood-planer, 
 * wood-bandsaw, miter-saw, wood-routertable
 */
#define MACHINE_NAME "3dprinter"   // Typ aus der Liste oben wählen
#define MACHINE_ID   "Drucker3"     // Eindeutige Kennung dieser Hardware-Box
#define RFIDCARD_AUTH_CONST true    // true = Card-Auth aktiv
#define CONTINUOUS_SERVER_CHECK true // true = alle 2s Server-Auth-Check im RUNNING (nur mit RFIDCARD_AUTH_CONST)

#endif