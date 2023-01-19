#include "file_reader.h"


#include <stdio.h>

//#include "SmartPointers.h"

int main() {

    struct disk_t *disk= disk_open_from_file("../fat12test.img");
    struct volume_t *fatAf= fat_open(disk,0);



    struct file_t *fileH=file_open(fatAf,"SHEET.BIN");

    char data[5000];

    file_read(data,1,1667,fileH);

    file_close(fileH);
    fat_close(fatAf);
    disk_close(disk);


    return 0;
}
