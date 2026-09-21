#ifndef ATA_H
#define ATA_H

#include "../common.h"

int ata_is_ready();
void read_sector(uint32_t lba, uint8_t* buffer);
void write_sector(uint32_t lba, uint8_t* buffer);

#endif
