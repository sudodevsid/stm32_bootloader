/* Thin wrappers around HAL_FLASH for sector erase + buffered programming. */
#ifndef __FLASH_IF_H
#define __FLASH_IF_H

#include "main.h"

/* Erases num_sectors starting at first_sector (STM32 FLASH_SECTOR_x index,
 * 0-7 on this part). Returns 0 on success, -1 on failure. */
int FlashIf_EraseSectors(uint8_t first_sector, uint8_t num_sectors);

/* Programs len bytes at addr. Handles any address/length alignment safely
 * (falls back to byte programming at unaligned edges), so callers may
 * write arbitrarily-sized chunks back to back without tracking alignment
 * themselves. Returns 0 on success, -1 on failure. */
int FlashIf_Write(uint32_t addr, const uint8_t *data, uint32_t len);

#endif /* __FLASH_IF_H */
