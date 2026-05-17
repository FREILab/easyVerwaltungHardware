#ifndef OTA_H
#define OTA_H

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* 
 * OTA Partition Layout for Pico 2 W (4MB Flash)
 * ======================================================
 * 0x10000000 - 0x101A0000: Active Partition (1.6MB)
 * 0x101A0000 - 0x10340000: Inactive Partition (1.6MB)
 * 0x10340000 - 0x10380000: Boot Metadata (256KB)
 * 0x10380000 - 0x10400000: Reserved (512KB)
 */

#define FLASH_BASE 0x10000000
#define PARTITION_SIZE (1024 * 1024 + 512 * 1024)  // 1.6MB
#define ACTIVE_PARTITION_START 0x10000000
#define INACTIVE_PARTITION_START 0x101A0000
#define BOOT_METADATA_START 0x10340000
#define BOOT_METADATA_SIZE 256 * 1024

/* Boot Metadata Structure */
typedef struct {
    uint32_t magic;           // 0x0OTA_BOOT (0x0TABOOT)
    uint32_t active_partition; // 0 = active is at 0x10000000, 1 = active is at 0x101A0000
    uint32_t firmware_crc32;  // CRC of active firmware
    uint32_t firmware_size;   // Size of active firmware
    char version[32];         // Version string
    uint32_t timestamp;       // Unix timestamp of last update
    uint32_t reserved[54];    // Padding to 256 bytes
} boot_metadata_t;

#define BOOT_MAGIC 0x5A5A0001
#define BOOT_METADATA_PAGE ((uint32_t *)(BOOT_METADATA_START))

/* OTA Functions */

/**
 * Initialize OTA system - read boot metadata, determine active partition
 */
bool ota_init(void);

/**
 * Get inactive partition address (where new firmware goes)
 */
uint32_t ota_get_inactive_partition_addr(void);

/**
 * Get active partition address (currently running firmware)
 */
uint32_t ota_get_active_partition_addr(void);

/**
 * Write firmware to inactive partition
 * Returns: true on success, false on error
 */
bool ota_write_firmware(const uint8_t *data, uint32_t size);

/**
 * Verify firmware CRC32 in inactive partition
 */
bool ota_verify_firmware(uint32_t size, uint32_t expected_crc32);

/**
 * Download firmware from HTTP URL.
 * Current implementation is a stub and returns false.
 */
bool ota_http_download(const char *url, uint32_t expected_size, uint32_t expected_crc32);

/**
 * Activate inactive partition (set boot flag and reboot)
 * This triggers a reset, does not return
 */
void ota_activate_and_reboot(uint32_t size, const char *version);

/**
 * Rollback to previous firmware (switch back to old active partition)
 */
bool ota_rollback(void);

/**
 * Get current boot metadata
 */
const boot_metadata_t *ota_get_metadata(void);

#ifdef __cplusplus
}
#endif

#endif // OTA_H
