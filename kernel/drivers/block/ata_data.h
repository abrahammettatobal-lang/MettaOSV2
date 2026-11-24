#ifndef METTA_ATA_DATA_H
#define METTA_ATA_DATA_H
#include "block.h"
metta_block_dev_t *ata_data_device(void);
int ata_data_mounted(void);
void ata_set_mounted(void);
#endif
