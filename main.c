#include "file_reader.h"


#include <stdio.h>

int main() {

    struct disk_t *disk= disk_open_from_file("../fat12test.bin");
    struct volume_t *fatAf= fat_open(disk,0);



    struct file_t *fileH=file_open(fatAf,"BEST.TX");

    char data[5000];

    file_read(data,1,3000,fileH);


    return 0;
}
