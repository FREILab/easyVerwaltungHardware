#ifndef SETTINGS_H
#define SETTINGS_H

/**
 * TUER-KONFIGURATION
 * MACHINE_NAME und MACHINE_ID werden per platformio.ini Build-Flag gesetzt.
 * Fallbacks nur für lokale Entwicklung ohne platformio.secrets.ini.
 */

// --- Netzwerk ---
// Statische Fallback-IP für das Gerät, falls DHCP fehlschlägt.
// Format: vier Bytes kommagetrennt (direkt an IPAddress() übergeben).
#define ETHERNET_FALLBACK_IP 192, 168, 178, 177

// --- Stepper-Motor ---
#define STEPPER_SPEED  1200   ///< Schritte/Sekunde
#define STEPPER_ACCEL  2000   ///< Schritte/Sekunde²
#define STEPS_TO_OPEN   500   ///< Extra-Schritte nach dem Endschalter (Öffnen)
#define STEPS_TO_CLOSE  500   ///< Extra-Schritte über den Endschalter hinaus (Schließen)
#define STEPS_MAX      2000   ///< Maximale Schritte ohne Endschalter-Bestätigung

// --- Türlogik ---
// Wartezeit nach erfolgreichem Öffnen, bevor die Tür wieder gesperrt werden kann
#define DOOR_OPEN_MIN_HOLD_MS 2000

#endif /* SETTINGS_H */
