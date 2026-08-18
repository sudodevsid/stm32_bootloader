#include "ota_shared.h"
#include "flash_if.h"
#include <string.h>

void OTA_Ctrl_Load(ota_ctrl_t *ctrl)
{
  memcpy(ctrl, (const void *)OTA_CTRL_ADDR, sizeof(*ctrl));
  if (ctrl->magic != OTA_MAGIC) {
    memset(ctrl, 0, sizeof(*ctrl));
    ctrl->magic = OTA_MAGIC;
  }
}

int OTA_Ctrl_Save(const ota_ctrl_t *ctrl)
{
  if (FlashIf_EraseSectors(OTA_CTRL_SECTOR, OTA_CTRL_SECTOR_COUNT) != 0) return -1;
  return FlashIf_Write(OTA_CTRL_ADDR, (const uint8_t *)ctrl, sizeof(*ctrl));
}

/* Table-based CRC32 (IEEE 802.3 / zlib), polynomial 0xEDB88320 reflected. */
static uint32_t crc32_table[256];
static int table_ready = 0;

static void build_table(void)
{
  for (uint32_t i = 0; i < 256; i++) {
    uint32_t c = i;
    for (int k = 0; k < 8; k++) {
      c = (c & 1) ? (0xEDB88320u ^ (c >> 1)) : (c >> 1);
    }
    crc32_table[i] = c;
  }
  table_ready = 1;
}

uint32_t OTA_Crc32_Update(uint32_t crc, const uint8_t *data, uint32_t len)
{
  if (!table_ready) build_table();
  for (uint32_t i = 0; i < len; i++) {
    crc = crc32_table[(crc ^ data[i]) & 0xFFu] ^ (crc >> 8);
  }
  return crc;
}
