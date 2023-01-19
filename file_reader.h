#ifndef FAT_FILE_READER_H
#define FAT_FILE_READER_H
#include "FatStructures.h"
#include "Fat12Table.h"
#include <stdio.h>
#define SECTOR_SIZE 512


struct disk_t{
    FILE *diskFD;
    uint32_t numberOfSectors;
};
struct disk_t* disk_open_from_file(const char* volume_file_name);
int disk_read(struct disk_t* pdisk, int32_t first_sector, void* buffer, int32_t sectors_to_read);
int disk_close(struct disk_t* pdisk);

struct volume_t{
    struct disk_t *disk;
    struct bootSectorFat fatInfo;
    void *FAT1;
    void *FAT2;
    void *rootDirectory;
};
struct volume_t* fat_open(struct disk_t* pdisk, uint32_t first_sector);
int fat_close(struct volume_t* pvolume);

struct clusters_chain_t *get_chain_fat12( void *  buffer, size_t size, uint16_t first_cluster);

struct file_t{
    struct SFN fileInfo;
    uint16_t pos;
    struct volume_t *fat;
    struct clusters_chain_t *fatChain;
};
struct file_t* file_open(struct volume_t* pvolume, const char* file_name);
int file_close(struct file_t* stream);
size_t file_read(void *ptr, size_t size, size_t nmemb, struct file_t *stream);
int32_t file_seek(struct file_t* stream, int32_t offset, int whence);

struct dir_t{
    void *dirData;
    int size;
    int pos;
    int readEmptyFiles;
};

struct dir_entry_t{
    char name[13];
    size_t size;
    int is_archived;
    int is_readonly;
    int is_system;
    int is_hidden;
    int is_directory;
};
struct dir_t* dir_open(struct volume_t* pvolume, const char* dir_path);
int dir_read(struct dir_t* pdir, struct dir_entry_t* pentry);
int dir_close(struct dir_t* pdir);


#endif
