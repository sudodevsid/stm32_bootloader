/*
 * Flash memory map + shared "OTA control block" used to hand off state
 * between the app and the bootloader across a reset. Must be identical
 * in both projects (bootloader + app) - keep any edits in sync.
 *
 * Sector    Range                        Size    Use
 *   0-2     0x08000000-0x0800BFFF        48K     Bootloader code
 *   3       0x0800C000-0x0800FFFF        16K     OTA control block (this file)
 *   4-5     0x08010000-0x0803FFFF       192K     App ACTIVE slot (what runs)
 *   6-7     0x08040000-0x0807FFFF       256K     App STAGING slot (OTA download target)
 */
#ifndef __OTA_SHARED_H
#define __OTA_SHARED_H

#include "main.h"

#define OTA_BOOTLOADER_ADDR           0x08000000u

#define OTA_CTRL_ADDR                 0x0800C000u
#define OTA_CTRL_SECTOR                3u
#define OTA_CTRL_SECTOR_COUNT          1u

#define OTA_APP_ACTIVE_ADDR            0x08010000u
#define OTA_APP_ACTIVE_SECTOR          4u
#define OTA_APP_ACTIVE_SECTOR_COUNT    2u
#define OTA_APP_ACTIVE_SIZE            (192u * 1024u)

#define OTA_APP_STAGING_ADDR           0x08040000u
#define OTA_APP_STAGING_SECTOR         6u
#define OTA_APP_STAGING_SECTOR_COUNT   2u
#define OTA_APP_STAGING_SIZE           (256u * 1024u)

#define OTA_MAGIC                      0x4F544131u /* "OTA1" */

typedef struct {
  uint32_t magic;
  uint8_t  update_pending;   /* set by the app, cleared by the bootloader once handled */
  uint8_t  _pad[3];
  uint32_t staged_length;    /* bytes actually written to staging, set by the bootloader */
  uint32_t staged_crc32;     /* CRC32 computed while streaming into staging */
  char     installed_etag[64];
  char     pending_etag[64]; /* ETag the app saw when it requested this update */
} ota_ctrl_t;

/* Reads the control block from flash. If the magic doesn't match (first
 * boot ever, or a corrupt/erased sector), fills in a safe empty default
 * instead (no update pending, no installed_etag) rather than failing. */
void OTA_Ctrl_Load(ota_ctrl_t *ctrl);

/* Erases sector 3 and writes the given control block. Returns 0/-1. */
int OTA_Ctrl_Save(const ota_ctrl_t *ctrl);

/* Standard CRC32 (IEEE 802.3 / zlib), streaming-friendly: seed the first
 * call with 0xFFFFFFFF, feed successive chunks, then XOR the final result
 * with 0xFFFFFFFF to get the conventional CRC32 value. */
uint32_t OTA_Crc32_Update(uint32_t crc, const uint8_t *data, uint32_t len);

#endif /* __OTA_SHARED_H */
