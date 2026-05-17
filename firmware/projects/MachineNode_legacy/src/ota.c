#include "ota.h"
#include <string.h>
#include <stdio.h>
#include "pico/stdlib.h"
#include "pico/time.h"
#include "hardware/flash.h"
#include "hardware/sync.h"
#include "hardware/watchdog.h"

/* CRC32 implementation */
static uint32_t crc32_table[256];
static bool crc32_table_initialized = false;

static void crc32_init(void) {
    if (crc32_table_initialized) return;
    
    for (uint32_t i = 0; i < 256; i++) {
        uint32_t crc = i;
        for (int j = 0; j < 8; j++) {
            if (crc & 1) {
                crc = (crc >> 1) ^ 0xEDB88320;
            } else {
                crc >>= 1;
            }
        }
        crc32_table[i] = crc;
    }
    crc32_table_initialized = true;
}

static uint32_t crc32_update(uint32_t crc, const uint8_t *data, uint32_t size) {
    crc32_init();
    crc ^= 0xFFFFFFFF;
    for (uint32_t i = 0; i < size; i++) {
        crc = crc32_table[(crc ^ data[i]) & 0xFF] ^ (crc >> 8);
    }
    return crc ^ 0xFFFFFFFF;
}

static boot_metadata_t boot_metadata = {0};
static bool metadata_initialized = false;

/* Read boot metadata from flash */
static void ota_read_metadata(void) {
    if (metadata_initialized) return;
    
    memcpy(&boot_metadata, (void *)BOOT_METADATA_START, sizeof(boot_metadata_t));
    
    if (boot_metadata.magic != BOOT_MAGIC) {
        printf("[OTA] No valid boot metadata found, initializing defaults\n");
        memset(&boot_metadata, 0, sizeof(boot_metadata_t));
        boot_metadata.magic = BOOT_MAGIC;
        boot_metadata.active_partition = 0;  // Active is at 0x10000000
        strcpy(boot_metadata.version, "1.0.0");
    }
    
    metadata_initialized = true;
}

bool ota_init(void) {
    ota_read_metadata();
    
    printf("[OTA] Initialized\n");
    printf("[OTA] Active Partition: 0x%08X (%s)\n",
           boot_metadata.active_partition == 0 ? ACTIVE_PARTITION_START : INACTIVE_PARTITION_START,
           boot_metadata.active_partition == 0 ? "ACTIVE_SLOT" : "INACTIVE_SLOT");
    printf("[OTA] Firmware Size: %u bytes\n", boot_metadata.firmware_size);
    printf("[OTA] Version: %s\n", boot_metadata.version);
    
    return true;
}

uint32_t ota_get_inactive_partition_addr(void) {
    ota_read_metadata();
    // If active is 0, inactive is 1 (and vice versa)
    return boot_metadata.active_partition == 0 ? INACTIVE_PARTITION_START : ACTIVE_PARTITION_START;
}

uint32_t ota_get_active_partition_addr(void) {
    ota_read_metadata();
    return boot_metadata.active_partition == 0 ? ACTIVE_PARTITION_START : INACTIVE_PARTITION_START;
}

const boot_metadata_t *ota_get_metadata(void) {
    ota_read_metadata();
    return &boot_metadata;
}

bool ota_write_firmware(const uint8_t *data, uint32_t size) {
    if (size > PARTITION_SIZE) {
        printf("[OTA] ERROR: Firmware too large (%u > %u)\n", size, PARTITION_SIZE);
        return false;
    }
    
    uint32_t inactive_addr = ota_get_inactive_partition_addr();
    uint32_t flash_offset = inactive_addr - FLASH_BASE;
    
    printf("[OTA] Writing %u bytes to partition at 0x%08X (offset 0x%X)\n", 
           size, inactive_addr, flash_offset);
    
    /* Erase flash sector by sector (4KB sectors) */
    uint32_t sectors_needed = (size + FLASH_SECTOR_SIZE - 1) / FLASH_SECTOR_SIZE;
    printf("[OTA] Erasing %u sectors...\n", sectors_needed);
    
    uint32_t saved_irq = save_and_disable_interrupts();
    
    for (uint32_t i = 0; i < sectors_needed; i++) {
        flash_range_erase(flash_offset + i * FLASH_SECTOR_SIZE, FLASH_SECTOR_SIZE);
    }
    
    /* Write firmware in 256-byte pages */
    uint32_t pages_written = 0;
    for (uint32_t offset = 0; offset < size; offset += FLASH_PAGE_SIZE) {
        uint32_t chunk_size = (size - offset < FLASH_PAGE_SIZE) ? (size - offset) : FLASH_PAGE_SIZE;
        flash_range_program(flash_offset + offset, &data[offset], chunk_size);
        pages_written++;
        
        if (pages_written % 64 == 0) {
            printf("[OTA] Progress: %u/%u bytes (%.1f%%)\n", 
                   offset, size, (float)offset / size * 100.0);
        }
    }
    
    restore_interrupts(saved_irq);
    
    printf("[OTA] Firmware write complete\n");
    return true;
}

bool ota_verify_firmware(uint32_t size, uint32_t expected_crc32) {
    uint32_t inactive_addr = ota_get_inactive_partition_addr();
    
    printf("[OTA] Verifying firmware CRC32...\n");
    
    uint32_t calculated_crc = crc32_update(0, (uint8_t *)inactive_addr, size);
    
    printf("[OTA] Expected: 0x%08X, Calculated: 0x%08X\n", expected_crc32, calculated_crc);
    
    if (calculated_crc != expected_crc32) {
        printf("[OTA] ERROR: CRC32 mismatch!\n");
        return false;
    }
    
    printf("[OTA] CRC32 verification passed\n");
    return true;
}

bool ota_http_download(const char *url, uint32_t expected_size, uint32_t expected_crc32) {
    (void)expected_size;
    (void)expected_crc32;
    printf("[OTA] ota_http_download not implemented yet. URL=%s\n", url ? url : "(null)");
    return false;
}

void ota_activate_and_reboot(uint32_t size, const char *version) {
    printf("[OTA] Activating new firmware and rebooting...\n");
    
    /* Update metadata */
    boot_metadata.active_partition = boot_metadata.active_partition == 0 ? 1 : 0;
    boot_metadata.firmware_size = size;
    boot_metadata.timestamp = time_us_64() / 1000000;  // Unix-like timestamp in seconds
    strncpy(boot_metadata.version, version, sizeof(boot_metadata.version) - 1);
    
    /* Write metadata back to flash */
    uint32_t flash_offset = BOOT_METADATA_START - FLASH_BASE;
    uint32_t saved_irq = save_and_disable_interrupts();
    
    flash_range_erase(flash_offset, FLASH_SECTOR_SIZE);
    flash_range_program(flash_offset, (uint8_t *)&boot_metadata, sizeof(boot_metadata_t));
    
    restore_interrupts(saved_irq);
    
    printf("[OTA] Metadata updated, rebooting in 2 seconds...\n");
    sleep_ms(2000);
    
    /* Trigger watchdog reset */
    watchdog_enable(1, 1);
    while (1) {
        tight_loop_contents();
    }
}

bool ota_rollback(void) {
    printf("[OTA] Rolling back to previous firmware...\n");
    
    boot_metadata.active_partition = boot_metadata.active_partition == 0 ? 1 : 0;
    
    /* Write metadata */
    uint32_t flash_offset = BOOT_METADATA_START - FLASH_BASE;
    uint32_t saved_irq = save_and_disable_interrupts();
    
    flash_range_erase(flash_offset, FLASH_SECTOR_SIZE);
    flash_range_program(flash_offset, (uint8_t *)&boot_metadata, sizeof(boot_metadata_t));
    
    restore_interrupts(saved_irq);
    
    printf("[OTA] Rollback complete, rebooting...\n");
    sleep_ms(1000);
    watchdog_enable(1, 1);
    while (1) {
        tight_loop_contents();
    }
}
