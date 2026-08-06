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

// --- RFID-Recovery ---
// Unbedingtes volles RC522-Re-Init (Hard-Reset via RST-Pin) im Idle. Heilt einen
// still weggekippten Frontend, den der alte VersionReg-Check NICHT sah (Ausfall
// 2026-06-30: SPI/VersionReg gültig, aber Karten wurden nicht mehr gelesen).
// Max. Tür-Totzeit nach einem Glitch = dieses Intervall.
#define RFID_REINIT_INTERVAL_MS  10000UL

// --- ATmega-Heartbeat ---
// Liveness + RC522-Registerdiagnose auf den Bus (via ESP ins SD-Log gespiegelt).
// Bewusst != RFID_REINIT_INTERVAL_MS, damit der HB die Register auch im "gealterten"
// Zustand (vor dem nächsten Re-Init) samplet und einen Fault sichtbar macht.
#define ATMEGA_HB_INTERVAL_MS     5000UL
