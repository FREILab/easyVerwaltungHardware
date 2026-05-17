#pragma once
#include <stdint.h>
#include <stdbool.h>

/*
 * OTA State — wird an OTA_STATE_ADDR in Flash geschrieben.
 * Bootloader liest diese Struktur beim Start aus um zu entscheiden,
 * welcher App-Slot aktiv ist und ob ein Update ansteht.
 *
 * Schreibregel: Struct immer vollständig schreiben (flash_range_erase + program).
 * magic muss OTA_STATE_MAGIC sein, sonst ignoriert der Bootloader den Eintrag.
 */

#define OTA_STATE_MAGIC   0xDEADBEEFu
#define OTA_STATE_VERSION 1u

typedef enum {
    OTA_SLOT_A = 0,
    OTA_SLOT_B = 1,
} ota_slot_t;

typedef enum {
    OTA_STATUS_OK      = 0,  /* Kein Update ausstehend, boot normal */
    OTA_STATUS_PENDING = 1,  /* Neues Image in new_slot, bitte validieren + aktivieren */
    OTA_STATUS_FAILED  = 2,  /* Letztes OTA fehlgeschlagen, Bootloader ist auf active_slot zurückgefallen */
} ota_status_t;

typedef struct __attribute__((packed)) {
    uint32_t    magic;           /* Muss OTA_STATE_MAGIC sein */
    uint8_t     version;         /* Struct-Version, aktuell OTA_STATE_VERSION */
    ota_slot_t  active_slot;     /* Derzeit laufender App-Slot */
    ota_status_t status;         /* OTA-Zustand */
    ota_slot_t  new_slot;        /* Ziel-Slot des ausstehenden Updates */
    uint32_t    new_image_size;  /* Größe des neuen Images in Bytes */
    uint32_t    new_image_crc32; /* CRC32 des neuen Images zur Integritätsprüfung */
    char        new_version[32]; /* Versionsstring des neuen Images, null-terminiert */
    uint8_t     _reserved[7];    /* Padding auf 64 Bytes */
} ota_state_t;

_Static_assert(sizeof(ota_state_t) == 64, "ota_state_t size changed — update _reserved");
