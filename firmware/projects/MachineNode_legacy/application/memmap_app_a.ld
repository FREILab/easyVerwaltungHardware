/* App Slot A — Pico 2 W (RP2350)
 * Gelinkt ab 0x10040000 (direkt nach dem 256 KB Bootloader).
 * Maximale Größe: 1 MB (= 0x100000).
 */

MEMORY {
    FLASH(rx)      : ORIGIN = 0x10040000, LENGTH = 1024k
    RAM(rwx)       : ORIGIN = 0x20000000, LENGTH = 512k
    SCRATCH_X(rwx) : ORIGIN = 0x20080000, LENGTH = 4k
    SCRATCH_Y(rwx) : ORIGIN = 0x20081000, LENGTH = 4k
}

INCLUDE "pico_sdk_memory_map.ld"
