#pragma once
#include <stdbool.h>
#include "ota_state.h"

typedef enum {
    OTA_RESULT_UP_TO_DATE = 0,
    OTA_RESULT_UPDATE_STAGED,
    OTA_RESULT_NO_NETWORK,
    OTA_RESULT_DOWNLOAD_FAILED,
    OTA_RESULT_VERIFY_FAILED,
    OTA_RESULT_FLASH_ERROR,
} ota_result_t;

/*
 * Prüft beim OTA-Server ob eine neue Version verfügbar ist.
 * Falls ja: lädt das Image herunter, schreibt es in den inaktiven Slot,
 * setzt den OTA-State und gibt OTA_RESULT_UPDATE_STAGED zurück.
 * Anschließend kann der Aufrufer einen Reboot auslösen.
 */
ota_result_t ota_check_and_stage(void);

/* Gibt den aktuellen aktiven Slot zurück (liest aus OTA-State). */
ota_slot_t ota_get_active_slot(void);
