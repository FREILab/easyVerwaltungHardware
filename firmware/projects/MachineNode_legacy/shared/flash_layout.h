#pragma once

/*
 * Flash-Layout für Pico 2 W (RP2350, 4 MB Flash)
 *
 * Adresse           Größe    Inhalt
 * 0x10000000        256 KB   Bootloader
 * 0x10040000        1 MB     App Slot A  (Standard-Partition nach Erstflash)
 * 0x10140000        1 MB     App Slot B  (OTA-Zielpartition)
 * 0x10240000        64 KB    OTA State   (ota_state_t, in Flash geschrieben)
 * 0x10250000        ~1.7 MB  Frei / NVS
 */

#define FLASH_BASE          0x10000000u

#define BOOTLOADER_ADDR     (FLASH_BASE + 0x00000000u)
#define BOOTLOADER_SIZE     (256u * 1024u)

#define APP_SLOT_A_ADDR     (FLASH_BASE + 0x00040000u)
#define APP_SLOT_B_ADDR     (FLASH_BASE + 0x00140000u)
#define APP_SLOT_SIZE       (1024u * 1024u)

#define OTA_STATE_ADDR      (FLASH_BASE + 0x00240000u)
#define OTA_STATE_SIZE      (64u * 1024u)

/* XIP-Offset (für flash_range_erase / flash_range_program):
 * Der Pico SDK erwartet Offsets relativ zum Flash-Anfang, nicht absolute Adressen. */
#define ADDR_TO_FLASH_OFFSET(addr) ((addr) - FLASH_BASE)

#define APP_SLOT_A_OFFSET   ADDR_TO_FLASH_OFFSET(APP_SLOT_A_ADDR)
#define APP_SLOT_B_OFFSET   ADDR_TO_FLASH_OFFSET(APP_SLOT_B_ADDR)
#define OTA_STATE_OFFSET    ADDR_TO_FLASH_OFFSET(OTA_STATE_ADDR)
