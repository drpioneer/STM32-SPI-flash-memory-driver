#ifndef _45DBXX_H
#define _45DBXX_H

#include <stdint.h>
#include <stdbool.h>
#include "lfs.h"

typedef struct
{
    uint8_t     FlashSize_MBit;	
    uint16_t    PageSize;
    uint16_t    Pages;
    uint8_t	    Shift;
} AT45_t;

bool    AT45_Init       (void);
void    AT45_EraseChip  (void);
void    AT45_ErasePage  (uint16_t page);
void    AT45_WritePage  (uint16_t page, uint16_t offset, uint16_t size, uint8_t *buf);
void    AT45_ReadPage   (uint16_t page, uint16_t offset, uint16_t size, uint8_t *buf);

int block_device_read   (const struct lfs_config *c, lfs_block_t block, lfs_off_t off, void *buffer, lfs_size_t size);
int block_device_prog   (const struct lfs_config *c, lfs_block_t block, lfs_off_t off, const void *buffer, lfs_size_t size);
int block_device_erase  (const struct lfs_config *c, lfs_block_t block);
int block_device_sync   (const struct lfs_config *c);

#endif
