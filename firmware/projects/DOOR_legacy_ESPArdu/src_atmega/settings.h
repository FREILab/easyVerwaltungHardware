#pragma once

// --- Stepper-Motor ---
#define STEPPER_SPEED     1200   // Schritte/Sekunde
#define STEPPER_ACCEL     2000   // Schritte/Sekunde²
#define STEPS_TO_OPEN      500   // Extra-Schritte nach Endschalter (Öffnen)
#define STEPS_TO_CLOSE     500   // Extra-Schritte über Endschalter hinaus (Schließen)
#define STEPS_MAX         2000   // Maximale Schritte ohne Endschalter-Bestätigung

// --- Timeouts ---
#define MOTOR_TIMEOUT_MS      5000   // Max. Zeit für Motorbewegung bis Endschalter
#define DOOR_REED_TIMEOUT_MS 30000   // Max. Wartezeit auf Reed-Freigabe beim Schließen

// --- WDT ---
// avr/wdt.h: WDTO_4S entspricht ~4 Sekunden
