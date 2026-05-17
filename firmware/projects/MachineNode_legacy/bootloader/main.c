#include <stdint.h>
#include <string.h>
#include "pico/stdlib.h"
#include "hardware/flash.h"
#include "hardware/sync.h"
#include "flash_layout.h"
#include "ota_state.h"

/* CRC32 (IEEE 802.3) — gleiche Implementierung wie in ota.c */
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

/* Liest die OTA-State-Struct aus Flash (read-only, kein Kopieren nötig). */
static const ota_state_t *read_ota_state(void) {
    return (const ota_state_t *)OTA_STATE_ADDR;
}

/* Schreibt eine neue OTA-State-Struct in Flash. */
static void write_ota_state(const ota_state_t *state) {
    uint32_t save = save_and_disable_interrupts();
    flash_range_erase(OTA_STATE_OFFSET, FLASH_SECTOR_SIZE);
    flash_range_program(OTA_STATE_OFFSET, (const uint8_t *)state, sizeof(ota_state_t));
    restore_interrupts(save);
}

/* Springt in die App am angegebenen Flash-Slot. Kehrt nie zurück. */
static void __attribute__((noreturn)) jump_to_app(uint32_t slot_addr) {
    const uint32_t *vector_table = (const uint32_t *)slot_addr;
    uint32_t sp = vector_table[0];
    uint32_t pc = vector_table[1];

    /* Interrupts deaktivieren, Vektor-Tabelle umbiegen */
    __disable_irq();
    SCB->VTOR = slot_addr;
    __dsb();
    __isb();

    __asm volatile (
        "msr msp, %0 \n"
        "bx  %1      \n"
        : : "r"(sp), "r"(pc) : "memory"
    );
    __builtin_unreachable();
}

/* Prüft ob ein App-Image im Slot valide ist (Mindestgröße + Vektor-Tabelle). */
static bool is_slot_valid(uint32_t slot_addr) {
    const uint32_t *vt = (const uint32_t *)slot_addr;
    /* Stack-Pointer muss in den RAM-Bereich zeigen */
    if (vt[0] < 0x20000000u || vt[0] > 0x20100000u) return false;
    /* Reset-Handler muss im gleichen Slot liegen (Thumb-Bit gesetzt) */
    if ((vt[1] & ~1u) < slot_addr || (vt[1] & ~1u) >= slot_addr + APP_SLOT_SIZE) return false;
    return true;
}

int main(void) {
    const ota_state_t *state = read_ota_state();

    uint32_t active_addr = (state->active_slot == OTA_SLOT_B) ? APP_SLOT_B_ADDR : APP_SLOT_A_ADDR;

    if (state->magic == OTA_STATE_MAGIC && state->status == OTA_STATUS_PENDING) {
        uint32_t new_addr = (state->new_slot == OTA_SLOT_B) ? APP_SLOT_B_ADDR : APP_SLOT_A_ADDR;
        const uint8_t *new_image = (const uint8_t *)new_addr;

        uint32_t actual_crc = crc32(new_image, state->new_image_size);
        bool ok = (actual_crc == state->new_image_crc32) && is_slot_valid(new_addr);

        ota_state_t next = *state;
        if (ok) {
            next.active_slot = state->new_slot;
            next.status = OTA_STATUS_OK;
            active_addr = new_addr;
        } else {
            next.status = OTA_STATUS_FAILED;
        }
        write_ota_state(&next);
    }

    if (!is_slot_valid(active_addr)) {
        /* Fallback: versuche den anderen Slot */
        active_addr = (active_addr == APP_SLOT_A_ADDR) ? APP_SLOT_B_ADDR : APP_SLOT_A_ADDR;
    }

    jump_to_app(active_addr);
}
