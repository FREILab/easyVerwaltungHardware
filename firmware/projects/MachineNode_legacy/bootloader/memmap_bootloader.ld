/* Bootloader Linker Script — Pico 2 W (RP2350)
 *
 * Begrenzt den Bootloader auf die ersten 256 KB ab Flash-Anfang.
 * Die App-Slots beginnen erst ab 0x10040000.
 *
 * Basiert auf dem Standard-Pico-SDK memmap_default.ld.
 * INCLUDE am Ende zieht alle anderen Sektionen (.data, .bss, .stack, etc.)
 * aus dem Pico-SDK-Linker-Script nach.
 */

MEMORY {
    FLASH(rx)    : ORIGIN = 0x10000000, LENGTH = 256k
    RAM(rwx)     : ORIGIN = 0x20000000, LENGTH = 512k
    SCRATCH_X(rwx) : ORIGIN = 0x20080000, LENGTH = 4k
    SCRATCH_Y(rwx) : ORIGIN = 0x20081000, LENGTH = 4k
}

INCLUDE "pico_sdk_memory_map.ld"
