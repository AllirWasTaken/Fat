#include "file_reader.h"
#include "FatStructures.h"

#include <stdlib.h>
#include <errno.h>
#include <memory.h>
#include <string.h>

//#include "SmartPointers.h"



struct disk_t *disk_open_from_file(const char *volume_file_name) {
    if (!volume_file_name) {
        errno = EFAULT;
        return NULL;
    }
    struct disk_t *result = malloc(sizeof(struct disk_t));
    if (!result) {
        errno = ENOMEM;
        return NULL;
    }

    result->diskFD = fopen(volume_file_name, "rb");
    if (!result->diskFD) {
        free(result);
        errno = ENOENT;
        return NULL;
    }

    result->numberOfSectors = 0;
    char temp[SECTOR_SIZE];
    while (fread(temp, SECTOR_SIZE, 1, result->diskFD))result->numberOfSectors++;


    return result;
}

int disk_read(struct disk_t *pdisk, int32_t first_sector, void *buffer, int32_t sectors_to_read) {
    if (!pdisk || !buffer) {
        errno = EFAULT;
        return -1;
    }

    if ((uint32_t) (first_sector + sectors_to_read) > pdisk->numberOfSectors) {
        errno = ERANGE;
        return -1;
    }

    fseek(pdisk->diskFD, first_sector * SECTOR_SIZE, SEEK_SET);

    fread(buffer, SECTOR_SIZE, sectors_to_read, pdisk->diskFD);


    return sectors_to_read;
}

int disk_close(struct disk_t *pdisk) {
    if (!pdisk) {
        errno = EFAULT;
        return -1;
    }
    fclose(pdisk->diskFD);
    free(pdisk);
    return 0;
}


struct volume_t *fat_open(struct disk_t *pdisk, uint32_t first_sector) {
    if (!pdisk) {
        errno = EFAULT;
        return NULL;
    }


    struct volume_t *result = malloc(sizeof(struct volume_t));
    if (!result) {
        free(result);
        errno = ENOMEM;
        return NULL;
    }

    if (disk_read(pdisk, (int) first_sector, &result->fatInfo, 1) == -1) {
        free(result);
        return NULL;
    }

    if (result->fatInfo.signature != 0xaa55) {
        free(result);
        errno = EINVAL;
        return NULL;
    }

    result->FAT1 = malloc(result->fatInfo.bytes_per_sector * result->fatInfo.size_of_fat);
    if (!result->FAT1) {
        free(result);
        errno = ENOMEM;
        return NULL;
    }
    result->FAT2 = malloc(result->fatInfo.bytes_per_sector * result->fatInfo.size_of_fat);
    if (!result->FAT2) {
        free(result->FAT1);
        free(result);
        errno = ENOMEM;
        return NULL;
    }
    result->rootDirectory = malloc(sizeof(struct SFN) * result->fatInfo.maximum_number_of_files);
    if (!result->rootDirectory) {
        free(result->FAT1);
        free(result->FAT2);
        free(result);
        errno = ENOMEM;
        return NULL;
    }

    result->disk = pdisk;

    disk_read(pdisk, result->fatInfo.size_of_reserved_area, result->FAT1, result->fatInfo.size_of_fat);
    disk_read(pdisk, result->fatInfo.size_of_reserved_area + result->fatInfo.size_of_fat, result->FAT2,
              result->fatInfo.size_of_fat);


    if (memcmp(result->FAT1, result->FAT2, result->fatInfo.bytes_per_sector * result->fatInfo.size_of_fat)) {
        free(result->FAT1);
        free(result->FAT2);
        free(result->rootDirectory);
        free(result);
        errno = EINVAL;
        return NULL;
    }

    disk_read(pdisk, result->fatInfo.size_of_reserved_area + result->fatInfo.size_of_fat * 2, result->rootDirectory,
              (int) sizeof(struct SFN) * result->fatInfo.maximum_number_of_files / result->fatInfo.bytes_per_sector);


    return result;
}

int fat_close(struct volume_t *pvolume) {
    if (!pvolume) {
        errno = EFAULT;
        return -1;
    }
    free(pvolume->FAT1);
    free(pvolume->FAT2);
    free(pvolume->rootDirectory);
    free(pvolume);
    return 0;
}

int CompareFatWords(const char *a, const char *b) {

    for (int i = 0; i < 11; i++) {
        if (a[i] != b[i])return 1;
    }

    return 0;
}

struct clusters_chain_t *get_chain_fat12(void *buffer, size_t size, uint16_t first_cluster) {
    if (!buffer)return NULL;
    unsigned numberOfCluster = size / 3 * 2;
    if (first_cluster > numberOfCluster || !numberOfCluster)return NULL;
    struct clusters_chain_t *result = malloc(sizeof(struct clusters_chain_t));
    if (!result)return NULL;
    result->size = 1;
    result->clusters = NULL;

    int err = 0;


    for (uint16_t next = first_cluster; result->size < numberOfCluster + 2;) {
        next = TableValue(next, buffer);
        if (next >= FAT12_END_BEG&&next<=FAT12_END_END){
            break;
        }
        if (next >= numberOfCluster || result->size > numberOfCluster) {
            err = 1;
            break;
        }
        result->size++;
    }
    if (err) {
        free(result);
        return NULL;
    }

    result->clusters = malloc(result->size * sizeof(uint16_t));
    if (!result->clusters) {
        free(result);
        return NULL;
    }

    for (size_t i = 0, next = first_cluster; i < result->size; i++, next = TableValue(next, buffer)) {
        result->clusters[i] = next;
    }

    return result;
}

struct file_t *file_open(struct volume_t *pvolume, const char *file_name) {

    if (!pvolume||!file_name) {
        errno = EFAULT;
        return NULL;
    }

    struct SFN *rootDirectory = pvolume->rootDirectory;

    struct file_t *result = malloc(sizeof(struct file_t));
    if (!result) {
        errno = ENOMEM;
        return NULL;
    }

    int found = 0;


    char fixedName[11]="           ";

    {

        for(int i=0;i<11;i++){


            if(file_name[i]=='.'){
                i++;
                for(int z=8;z<11;z++,i++){
                    if(file_name[i]=='\0'){
                        break;
                    }
                    fixedName[z]=file_name[i];
                }
                break;
            }
            else if(file_name[i]=='\0'){
                break;
            }
            fixedName[i]=file_name[i];

        }

    }

    for (unsigned i = 0; i < pvolume->fatInfo.maximum_number_of_files; i++) {
        if (CompareFatWords(rootDirectory->filename, fixedName) == 0) {
            if ((rootDirectory->file_attributes & ( 1 << 4 )) >> 4 == 1) {
                errno = EISDIR;
                free(result);
                return NULL;
            }
            found = 1;
            result->fileInfo = *rootDirectory;
            break;
        }
        rootDirectory++;
    }


    if (found == 0) {
        errno = ENOENT;
        free(result);
        return NULL;
    }

    result->pos = 0;
    result->fat = pvolume;

    result->fatChain = get_chain_fat12(pvolume->FAT1, pvolume->fatInfo.size_of_fat * pvolume->fatInfo.bytes_per_sector,
                                       result->fileInfo.low_order_address_of_first_cluster);

    if (!result->fatChain) {
        free(result);
        errno = ENOMEM;
        return NULL;
    }

    return result;
}

int file_close(struct file_t *stream) {
    if (!stream) {
        errno = EFAULT;
        return -1;
    }


    free(stream->fatChain->clusters);
    free(stream->fatChain);
    free(stream);
    return 0;
}


int GetFileCluster(struct file_t *stream, void *buffer, int clusterNumber) {
    int sectorNumber=0;
    sectorNumber+=stream->fat->fatInfo.size_of_reserved_area;
    sectorNumber+=stream->fat->fatInfo.size_of_fat*stream->fat->fatInfo.number_of_fats;
    sectorNumber+=stream->fat->fatInfo.maximum_number_of_files*(int)sizeof(struct SFN)/stream->fat->fatInfo.bytes_per_sector;
    sectorNumber+=(stream->fatChain->clusters[clusterNumber]-2)*stream->fat->fatInfo.sectors_per_clusters;
    if (disk_read(stream->fat->disk, sectorNumber, buffer,
                  stream->fat->fatInfo.sectors_per_clusters) == -1) {
        errno = ERANGE;
        return 1;
    }
    return 0;
}


size_t file_read(void *ptr, size_t size, size_t nmemb, struct file_t *stream) {

    if (!ptr || !stream) {
        errno = EFAULT;
        return -1;
    }

    int sizeOfCluster = stream->fat->fatInfo.bytes_per_sector * stream->fat->fatInfo.sectors_per_clusters;

    char *tempCluster = malloc(sizeOfCluster);
    if (!tempCluster) {
        errno = ENOMEM;
        return -1;
    }

    int clusterNumber;
    int clusterPos = stream->pos % sizeOfCluster;
    int elementCounter = 0;
    int leftToReadInElement=0;
    char *ptrTemp = ptr;
    int ptrPos = 0;


    while (1) {
        if (stream->pos + (unsigned)1 > stream->fileInfo.size||elementCounter==(int)nmemb) {
            break;
        }

        //optimize clusters later
        clusterNumber = stream->pos / sizeOfCluster;
        if (GetFileCluster(stream, tempCluster, clusterNumber)) {
            errno = ERANGE;
            free(tempCluster);
            return -1;
        }


        if (leftToReadInElement) {

            clusterPos = stream->pos % sizeOfCluster;
            ptrTemp[ptrPos] = tempCluster[clusterPos];

            ptrPos++;
            stream->pos++;
            leftToReadInElement--;

            if (!leftToReadInElement)elementCounter++;
        }
        else {
            leftToReadInElement = (int) size;
        }
    }


    free(tempCluster);


    return elementCounter;
}

int32_t file_seek(struct file_t *stream, int32_t offset, int whence) {
    if (whence == SEEK_SET) {
        stream->pos = 0 + offset;
    }
    if (whence == SEEK_CUR) {
        stream->pos += offset;
    }
    if (whence == SEEK_END) {
        stream->pos = stream->fileInfo.size + offset;
    }
    return 0;
}


struct dir_t *dir_open(struct volume_t *pvolume, const char *dir_path) {

    if(!pvolume||!dir_path){
        errno=EFAULT;
        return NULL;
    }

    struct dir_t *result=malloc(sizeof(struct dir_t));
    if(!result){
        errno=ENOMEM;
        return NULL;
    }

    if(strcmp("\\",dir_path)){
        free(result);
        errno=ENOENT;
        return NULL;
    }

    result->pos=0;
    result->size=pvolume->fatInfo.maximum_number_of_files;
    result->dirData=pvolume->rootDirectory;
    result->readEmptyFiles=0;

    return result;
}

int dir_read(struct dir_t *pdir, struct dir_entry_t *pentry) {

    if(!pdir||!pentry){
        errno=EFAULT;
        return -1;
    }




    int found=0;
    struct SFN *directory=pdir->dirData;
    for(;pdir->pos<pdir->size;){

        if(directory[pdir->pos].filename[0]!=0x0&&directory[pdir->pos].filename[0]!=(char)0xe5){
            if((directory[pdir->pos].size!=0&&pdir->readEmptyFiles==0)||(pdir->readEmptyFiles&&directory[pdir->pos].size==0)) {
                if (((directory[pdir->pos].file_attributes & (1 << 3)) >> 3) != 1) {
                    found = 1;
                }
            }
        }
        pdir->pos++;
        if(pdir->pos==pdir->size){
            if(pdir->readEmptyFiles==0){
                pdir->readEmptyFiles=1;
                pdir->pos=0;
            }
        }
        if(found){
            break;
        }
    }
    if(!found){
        return 1;
    }


    directory+=pdir->pos-1;

    pentry->size=directory->size;

    {
        int fi=0;
        for(int i=0;i<11;i++,fi++){
            if(directory->filename[i]==' '||(i==8&&directory->filename[i+1]!=' ')){
                if((i==8&&directory->filename[i+1]!=' '))i--;
                for(int z=i+1;z<11;z++){
                    if(directory->filename[z]!=' '){
                        pentry->name[fi]='.';
                        fi++;
                        for(int j=z;j<11;j++){
                            if(directory->filename[j]==' ')break;
                            pentry->name[fi]=directory->filename[j];
                            fi++;
                        }
                        break;
                    }
                }
                break;
            }
            pentry->name[fi]=directory->filename[i];
        }
        pentry->name[fi]='\0';
    }


    pentry->is_archived= ((directory->file_attributes & ( 1 << 5 )) >> 5)==1;
    pentry->is_directory= ((directory->file_attributes & ( 1 << 4 )) >> 4)==1;
    pentry->is_hidden= ((directory->file_attributes & ( 1 << 1 )) >> 1)==1;
    pentry->is_readonly= ((directory->file_attributes & ( 1 << 0 )) >> 0)==1;
    pentry->is_system= ((directory->file_attributes & ( 1 << 2 )) >> 2)==1;






    return 0;
}

int dir_close(struct dir_t *pdir) {
    if(!pdir){
        errno=EFAULT;
        return -1;
    }
    free(pdir);

    return 0;
}