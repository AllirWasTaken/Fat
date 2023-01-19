#ifndef FAT_FAT12TABLE_H
#define FAT_FAT12TABLE_H
#include "FatStructures.h"

uint16_t TableValue(uint16_t index,void *Table);
void AssignTableValue(uint16_t index, uint16_t value);
void AssignTable(void *table);


#endif
