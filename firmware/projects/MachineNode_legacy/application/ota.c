#include "ota.h"
#include "config.h"
#include "flash_layout.h"
#include "pico/stdlib.h"
#include "hardware/flash.h"
#include "hardware/sync.h"
#include "lwip/tcp.h"
#include <stdio.h>
#include <string.h>

/* CRC32 (IEEE 802.3) */
static uint32_t crc32(const uint8_t *data, size_t len) {
    uint32_t crc = 0xFFFFFFFFu;
    for (size_t i = 0; i < len; i++) {
        crc ^= data[i];
        for (int b = 0; b < 8; b++) {
            crc = (crc >> 1) ^ (0xEDB88320u & -(crc & 1));
        }
    }
    return ~crc;
}

static const ota_state_t *current_state(void) {
    return (const ota_state_t *)OTA_STATE_ADDR;
}

static void write_ota_state(const ota_state_t *state) {
    uint32_t save = save_and_disable_interrupts();
    flash_range_erase(OTA_STATE_OFFSET, FLASH_SECTOR_SIZE);
    flash_range_program(OTA_STATE_OFFSET, (const uint8_t *)state, sizeof(ota_state_t));
    restore_interrupts(save);
}

ota_slot_t ota_get_active_slot(void) {
    const ota_state_t *s = current_state();
    if (s->magic != OTA_STATE_MAGIC) return OTA_SLOT_A;
    return s->active_slot;
}

/* TODO: HTTP-Download via lwIP implementieren.
 *
 * Ablauf:
 *  1. GET OTA_PATH gegen OTA_HOST:OTA_PORT
 *  2. Header parsen: X-Firmware-Version, X-Firmware-Size, X-Firmware-CRC32
 *  3. Falls Version == aktuell: OTA_RESULT_UP_TO_DATE
 *  4. Body (Binary) in den inaktiven Slot schreiben:
 *       - flash_range_erase(target_offset, size_aligned)
 *       - flash_range_program(target_offset, buf, FLASH_PAGE_SIZE) pro Page
 *  5. CRC prüfen
 *  6. ota_state_t schreiben (status=PENDING, new_slot, crc, size, version)
 *  7. OTA_RESULT_UPDATE_STAGED zurückgeben
 *
 * Achtung: flash_range_erase/program dürfen nicht aus dem Flash selbst
 * aufgerufen werden, aus dem der Code läuft. Entweder __no_inline_not_in_flash_func
 * verwenden oder die Funktion explizit in RAM kopieren (RAM_FUNC-Attribut).
 */
ota_result_t ota_check_and_stage(void) {
    /* Bestimme Ziel-Slot (immer den inaktiven) */
    ota_slot_t active = ota_get_active_slot();
    ota_slot_t target_slot = (active == OTA_SLOT_A) ? OTA_SLOT_B : OTA_SLOT_A;
    uint32_t target_offset = (target_slot == OTA_SLOT_B) ? APP_SLOT_B_OFFSET : APP_SLOT_A_OFFSET;

    (void)target_offset; /* TODO: In HTTP-Download-Implementierung verwenden */

    printf("[OTA] active=%s, target=%s\n",
           active == OTA_SLOT_A ? "A" : "B",
           target_slot == OTA_SLOT_A ? "A" : "B");

    /* TODO: HTTP-Request durchführen und Binary schreiben */
    return OTA_RESULT_DOWNLOAD_FAILED;
}
