#include "flash_if.h"
#include <string.h>

int FlashIf_EraseSectors(uint8_t first_sector, uint8_t num_sectors)
{
  HAL_FLASH_Unlock();

  FLASH_EraseInitTypeDef erase = {0};
  uint32_t sector_error = 0;
  erase.TypeErase = FLASH_TYPEERASE_SECTORS;
  erase.Sector = first_sector;
  erase.NbSectors = num_sectors;
  erase.VoltageRange = FLASH_VOLTAGE_RANGE_3;

  HAL_StatusTypeDef st = HAL_FLASHEx_Erase(&erase, &sector_error);

  HAL_FLASH_Lock();
  return (st == HAL_OK && sector_error == 0xFFFFFFFFu) ? 0 : -1;
}

int FlashIf_Write(uint32_t addr, const uint8_t *data, uint32_t len)
{
  HAL_FLASH_Unlock();

  int ok = 1;
  uint32_t i = 0;

  /* Byte-program until the address is word-aligned. */
  while (ok && i < len && ((addr + i) & 0x3u)) {
    if (HAL_FLASH_Program(FLASH_TYPEPROGRAM_BYTE, addr + i, data[i]) != HAL_OK) { ok = 0; break; }
    i++;
  }
  /* Word-program the aligned middle section. */
  while (ok && (i + 4u) <= len) {
    uint32_t word;
    memcpy(&word, &data[i], 4);
    if (HAL_FLASH_Program(FLASH_TYPEPROGRAM_WORD, addr + i, word) != HAL_OK) { ok = 0; break; }
    i += 4u;
  }
  /* Byte-program whatever's left (< 4 bytes). */
  while (ok && i < len) {
    if (HAL_FLASH_Program(FLASH_TYPEPROGRAM_BYTE, addr + i, data[i]) != HAL_OK) { ok = 0; break; }
    i++;
  }

  HAL_FLASH_Lock();
  return ok ? 0 : -1;
}
