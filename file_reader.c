#include "file_reader.h"
#include "FatStructures.h"

#include <stdlib.h>
#include <errno.h>
#include <memory.h>
#include <string.h>


struct disk_t* disk_open_from_file(const char* volume_file_name){
    if(!volume_file_name){
        errno=EFAULT;
        return NULL;
    }
    struct disk_t *result=malloc(sizeof(struct disk_t));
    if(!result){
        errno=ENOMEM;
        return NULL;
    }

    result->diskFD= fopen(volume_file_name,"rb");
    if(!result->diskFD){
        free(result);
        errno=ENOENT;
        return NULL;
    }

    result->numberOfSectors=0;
    char temp[SECTOR_SIZE];
    while(fread(temp,SECTOR_SIZE,1,result->diskFD))result->numberOfSectors++;


    return result;
}
int disk_read(struct disk_t* pdisk, int32_t first_sector, void* buffer, int32_t sectors_to_read){
    if(!pdisk||!buffer){
        errno=EFAULT;
        return -1;
    }

    if((uint32_t)(first_sector+sectors_to_read)>pdisk->numberOfSectors){
        errno=ERANGE;
        return -1;
    }

    fseek(pdisk->diskFD,first_sector*SECTOR_SIZE,SEEK_SET);

    fread(buffer,SECTOR_SIZE,sectors_to_read,pdisk->diskFD);


    return sectors_to_read;
}
int disk_close(struct disk_t* pdisk){
    if(!pdisk){
        errno=EFAULT;
        return -1;
    }
    fclose(pdisk->diskFD);
    free(pdisk);
    return 0;
}


struct volume_t* fat_open(struct disk_t* pdisk, uint32_t first_sector){
    if(!pdisk){
        errno=EFAULT;
        return NULL;
    }


    struct volume_t *result=malloc(sizeof(struct volume_t));
    if(!result){
        errno=ENOMEM;
        return NULL;
    }

    if(disk_read(pdisk,(int)first_sector,&result->fatInfo,1)==-1){
        return NULL;
    }

    if(result->fatInfo.signature!=0xaa55){
        errno=EINVAL;
        return NULL;
    }

    result->FAT1=malloc(result->fatInfo.bytes_per_sector*result->fatInfo.size_of_fat);
    if(!result->FAT1){
        free(result);
        errno=ENOMEM;
        return NULL;
    }
    result->FAT2=malloc(result->fatInfo.bytes_per_sector*result->fatInfo.size_of_fat);
    if(!result->FAT2){
        free(result->FAT1);
        free(result);
        errno=ENOMEM;
        return NULL;
    }
    result->rootDirectory=malloc(sizeof(struct SFN)*result->fatInfo.maximum_number_of_files);
    if(!result->rootDirectory){
        free(result->FAT1);
        free(result->FAT2);
        free(result);
        errno=ENOMEM;
        return NULL;
    }

    result->disk=pdisk;

    disk_read(pdisk,result->fatInfo.size_of_reserved_area,result->FAT1,result->fatInfo.size_of_fat);
    disk_read(pdisk,result->fatInfo.size_of_reserved_area+result->fatInfo.size_of_fat,result->FAT2,result->fatInfo.size_of_fat);


    if(memcmp(result->FAT1,result->FAT2,result->fatInfo.bytes_per_sector*result->fatInfo.size_of_fat)){
        free(result->FAT1);
        free(result->FAT2);
        free(result->rootDirectory);
        free(result);
        errno=EINVAL;
        return NULL;
    }

    disk_read(pdisk,result->fatInfo.size_of_reserved_area+result->fatInfo.size_of_fat*2,result->rootDirectory,
              (int)sizeof(struct SFN)*result->fatInfo.maximum_number_of_files/result->fatInfo.bytes_per_sector);


    return result;
}
int fat_close(struct volume_t* pvolume){
    if(!pvolume){
        errno=EFAULT;
        return -1;
    }
    free(pvolume->FAT1);
    free(pvolume->FAT2);
    free(pvolume->rootDirectory);
    free(pvolume);
    return 0;
}

int CompareFatWords(const char* a,const char *b){

    for(int i=0;i<11;i++){
        if(a[i]!=b[i])return 1;
    }

    return 0;
}

struct clusters_chain_t *get_chain_fat12(void * buffer, size_t size, uint16_t first_cluster) {
    if (!buffer)return NULL;
    unsigned numberOfCluster = size / 3 * 2;
    if (first_cluster > numberOfCluster || !numberOfCluster)return NULL;
    struct clusters_chain_t *result = malloc(sizeof(struct clusters_chain_t));
    if (!result)return NULL;
    result->size = 1;
    result->clusters = NULL;

    int err = 0;


    for (uint16_t next = first_cluster; result->size < numberOfCluster + 2;) {
        next = TableValue(next,buffer);
        if (next == FAT12_END) {
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

    result->clusters=malloc(result->size*sizeof(uint16_t));
    if(!result->clusters){
        free(result);
        return NULL;
    }

    for (size_t i=0,next=first_cluster;i<result->size;i++,next= TableValue(next,buffer)){
        result->clusters[i]=next;
    }

    return result;
}

struct file_t* file_open(struct volume_t* pvolume, const char* file_name){

    if(!pvolume){
        errno=EFAULT;
        return NULL;
    }

    struct SFN *rootDirectory=pvolume->rootDirectory;

    struct file_t *result=malloc(sizeof(struct file_t));
    if(!result){
        errno=ENOMEM;
        return NULL;
    }

    int found=0;


    char fixedName[11];

    {

        int temp=0;
        int check=0;

        for(int i=0;i<11;i++){
            if(file_name[i]=='.')check=i;
            if(file_name[i]=='\0'){
                temp=i;
                break;
            }
        }

        int extLength=temp-check-1;


        int i;
        for ( i = 0; i < check; i++) {
            fixedName[i]=file_name[i];
        }

        if(check) {
            for ( i = 0; i < check; i++) {
                fixedName[i]=file_name[i];
            }

            for (; i < 8; i++) {
                fixedName[i]=' ';
            }

            for(int z=0;z<extLength;z++,i++,check++){
                fixedName[i]=file_name[check+1];
            }

        }
        else{
            for ( i = 0; i < temp; i++) {
                fixedName[i]=file_name[i];
            }
        }

        for (; i < 11; i++) {
            fixedName[i]=' ';
        }

    }

    for(unsigned i=0;i<pvolume->fatInfo.maximum_number_of_files;i++){
        if(CompareFatWords(rootDirectory->filename,fixedName)==0){
            if(rootDirectory->file_attributes==212){
                errno=EISDIR;
                return NULL;
            }
            found=1;
            result->fileInfo=*rootDirectory;
            break;
        }
        rootDirectory++;
    }


    if(found==0){
        errno=ENOENT;
        return NULL;
    }

    result->pos=0;
    result->fat=pvolume;

    result->fatChain= get_chain_fat12(pvolume->FAT1,pvolume->fatInfo.size_of_fat*pvolume->fatInfo.bytes_per_sector,result->fileInfo.low_order_address_of_first_cluster);

    if(!result->fatChain){
        free(result);
        errno=ENOMEM;
        return NULL;
    }

    return result;
}
int file_close(struct file_t* stream){
    if(!stream){
        errno=EFAULT;
        return -1;
    }

    free(stream->fatChain);
    free(stream);
    return 0;
}



size_t file_read(void *ptr, size_t size, size_t nmemb, struct file_t *stream){

    if(!ptr||!stream){
        errno=EFAULT;
        return -1;
    }

    int sizeOfCluster=stream->fat->fatInfo.bytes_per_sector*stream->fat->fatInfo.sectors_per_clusters;

    char *tempCluster=malloc(sizeOfCluster);
    if(!tempCluster){
        errno=ENOMEM;
        return -1;
    }


    int done=0;
    int currentCluster;
    int needNewCluster=1;
    int elementCounter=0;
    int memoryToRead=(int)nmemb*size;
    int memoryRead=0;
    int posInCluster;
    char *tempPtr=ptr;

    while(1) {
        if(needNewCluster) {
            needNewCluster=0;
            currentCluster=stream->pos/sizeOfCluster;
            if (disk_read(stream->fat->disk, stream->fatChain->clusters[currentCluster] - 2, tempCluster,
                          stream->fat->fatInfo.sectors_per_clusters) == -1) {
                errno = ERANGE;
                free(tempCluster);
                return -1;
            }
        }

            for(;;elementCounter++){
                posInCluster=stream->pos%sizeOfCluster;
                if(memoryRead==memoryToRead){
                    done=1;
                    break;
                }
                if(posInCluster+size<sizeOfCluster){
                    stream->pos+=size;
                    memcpy(tempPtr+memoryRead,tempCluster+posInCluster,size);
                    memoryRead+=(int)size;
                }
                else{
                    needNewCluster=1;
                    break;
                }
            }

        if(done)break;
    }


    free(tempCluster);


    return elementCounter;
}
int32_t file_seek(struct file_t* stream, int32_t offset, int whence){
    if(whence==SEEK_SET){
        stream->pos=0+offset;
    }
    if(whence==SEEK_CUR){
        stream->pos+=offset;
    }
    if(whence==SEEK_END){
        stream->pos=stream->fileInfo.size+offset;
    }
    return 0;
}


struct dir_t* dir_open(struct volume_t* pvolume, const char* dir_path){
    return NULL;
}
int dir_read(struct dir_t* pdir, struct dir_entry_t* pentry){
    return 0;
}
int dir_close(struct dir_t* pdir){
    return 0;
}