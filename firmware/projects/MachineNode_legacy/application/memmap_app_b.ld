/* App Slot B — Pico 2 W (RP2350)
 * OTA-Zielpartition, gelinkt ab 0x10140000.
 * Maximale Größe: 1 MB (= 0x100000).
 *
 * Hinweis: Die App-Binary muss mit diesem Linker-Script neu gebaut werden,
 * bevor sie als OTA-Image ausgeliefert wird. Slot-A und Slot-B Images
 * sind NICHT austauschbar (unterschiedliche Link-Adressen).
 * TODO: Build-System so erweitern, dass beide Slots automatisch gebaut werden.
 */

MEMORY {
    FLASH(rx)      : ORIGIN = 0x10140000, LENGTH = 1024k
    RAM(rwx)       : ORIGIN = 0x20000000, LENGTH = 512k
    SCRATCH_X(rwx) : ORIGIN = 0x20080000, LENGTH = 4k
    SCRATCH_Y(rwx) : ORIGIN = 0x20081000, LENGTH = 4k
}

INCLUDE "pico_sdk_memory_map.ld"
