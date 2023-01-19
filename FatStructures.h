#ifndef FAT_STRUCTURES_H
#define FAT_STRUCTURES_H

#define FAT16_END 65528
#define FAT12_END 4090

#include <stdint.h>
#include <stddef.h>


struct date_t
{
    uint16_t day : 5;
    uint16_t month : 4;
    uint16_t year : 7;
};

struct my_time_t
{
    uint16_t seconds : 5;
    uint16_t minutes : 6;
    uint16_t hours : 5;
};

struct __attribute__((__packed__)) SFN
{
    char filename[11];
    uint8_t file_attributes;
    uint8_t reserved;
    uint8_t file_creation_time;
    struct my_time_t creation_time;
    struct date_t creation_date;
    uint16_t access_date;
    uint16_t high_order_address_of_first_cluster;
    struct my_time_t modified_time;
    struct date_t modified_date;
    uint16_t low_order_address_of_first_cluster;
    uint32_t size;
};


struct __attribute__((__packed__)) bootSectorFat {
    char unused[3]; //Assembly code instructions to jump to boot code (mandatory in bootable partition)
    char name[8]; //OEM name in ASCII
    uint16_t bytes_per_sector; //Bytes per sector (512, 1024, 2048, or 4096)
    uint8_t sectors_per_clusters; //Sectors per cluster (Must be a power of 2 and cluster size must be <=32 KB)
    uint16_t size_of_reserved_area; //Size of reserved area, in sectors
    uint8_t number_of_fats; //Number of FATs (usually 2)
    uint16_t maximum_number_of_files; //Maximum number of files in the root directory (FAT12/16; 0 for FAT32)
    uint16_t number_of_sectors; //Number of sectors in the file system; if 2 B is not large enough, set to 0 and use 4 B value in bytes 32-35 below
    uint8_t media_type; //Media type (0xf0=removable disk, 0xf8=fixed disk)
    uint16_t size_of_fat; //Size of each FAT, in sectors, for FAT12/16; 0 for FAT32
    uint16_t sectors_per_track; //Sectors per track in storage device
    uint16_t number_of_heads; //Number of heads in storage device
    uint32_t number_of_sectors_before_partition; //Number of sectors before the start partition
    uint32_t number_of_sectors_in_filesystem; //Number of sectors in the file system; this field will be 0 if the 2B field above(bytes 19 - 20) is non - zero
    uint8_t drive_number; //BIOS INT 13h(low level disk services) drive number
    uint8_t unused_1; //Not used
    uint8_t boot_signature; //Extended boot signature to validate next three fields (0x29)
    uint32_t serial_number; //Volume serial number
    char label[11];  //Volume label, in ASCII
    char type[8];  //File system type level, in ASCII. (Generally "FAT", "FAT12", or "FAT16")
    uint8_t unused_2[448]; //Not used
    uint16_t signature; //Signature value (0xaa55)
};

struct clusters_chain_t {
    uint16_t *clusters;
    size_t size;
};

#endif