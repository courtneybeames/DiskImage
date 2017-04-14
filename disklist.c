#include <pthread.h>
#include <string.h>
#include <unistd.h>    
#include <stdio.h>      
#include <stdlib.h>     
#include <stdbool.h>
#include <stdint.h>
#include <math.h>

#define FILE_NAME_LEN 8
#define FILE_EXT_LEN 3
#define read_only_mask 0x01
#define hidden_mask 0x02
#define system_mask 0x04
#define vol_label_mask 0x08
#define subdirectory_mask 0x10
#define archive_mask 0x20
#define device_mask 0x40

typedef struct root_attr{
    char file_name[FILE_NAME_LEN];
    char file_ext[FILE_EXT_LEN];
    uint8_t file_attr;
    uint8_t reserved;
    uint8_t create_time_ms;
    uint16_t create_time;
    uint16_t create_date;
    uint16_t last_access;
    uint16_t ea_index;
    uint16_t last_modified_time;
    uint16_t last_modified_date;
    uint16_t first_cluster;
    uint32_t file_size;
}root_attr;

int lenHelper(unsigned x) {
    if(x>=1000000000) return 10;
    if(x>=100000000) return 9;
    if(x>=10000000) return 8;
    if(x>=1000000) return 7;
    if(x>=100000) return 6;
    if(x>=10000) return 5;
    if(x>=1000) return 4;
    if(x>=100) return 3;
    if(x>=10) return 2;
    return 1;
}

char * grabCharFromFile(int pointer, int length, FILE * fp){
    char* name=malloc(sizeof(int));
    fseek(fp, pointer, SEEK_SET);
    fread(name, 1, length, fp);
    return name;
}

int * grabIntFromFile(int pointer, int length, FILE * fp){
    int * name=malloc(sizeof(int));
    fseek(fp, pointer, SEEK_SET);
    fread(name, 1, length, fp);
    return name;
}

int main(int argc, char *argv[]) {
//file handling
    FILE *fp = fopen(argv[1], "r"); 
    
    
    int *bytesPerSector = grabIntFromFile(11,2,fp);
    int *totalSectorCount = grabIntFromFile(19,2,fp);

    int totalSize = (*bytesPerSector) * (*totalSectorCount);
    
    int *numReserved = grabIntFromFile(14,2,fp);

    int addressOfFirstFAT= (0 + *numReserved) * (*bytesPerSector);

    int * numFATS = grabIntFromFile(16,1,fp);

    int * sectorsPerFAT = grabIntFromFile(22,2,fp);

    int *maxRootDirEntries = grabIntFromFile(17,2,fp);

    int bytesPerFAT = (*bytesPerSector) * (*sectorsPerFAT);

    int addressOfRootDirectory= addressOfFirstFAT + ((*numFATS) * (*sectorsPerFAT) * (*bytesPerSector));
    int addressOfDataRegion= addressOfRootDirectory + *maxRootDirEntries * 32;
    
    root_attr RootDirArray[*maxRootDirEntries];
    
    fseek(fp, addressOfRootDirectory, SEEK_SET);
    fread(RootDirArray, sizeof(root_attr), *maxRootDirEntries, fp);
    int sizeArr[10];
    int i;
//goes through root directory and looks at files
    for(i=0; i<*maxRootDirEntries; i++){
        if(RootDirArray[i].file_name[0] != 0 && RootDirArray[i].file_size !=0 && RootDirArray[i].first_cluster !=0 && RootDirArray[i].file_attr != 0x0F){
            //modifies filename to eliminate space
            char newFileName[9];
            char newName[21];
            int k=0;
            while(k < 8){
                if(RootDirArray[i].file_name[k] == ' '){
                    break;
                }
                newFileName[k]=RootDirArray[i].file_name[k];
                k++;
            }

            newFileName[k]='.';
            k++;

            newFileName[k]='\0';
            strcpy(newName, newFileName);
            strcat(newName, RootDirArray[i].file_ext);

            k+=3;
            //pads filename
            char * space= " ";
            while(k<20){
                 strcat(newName,space);
                 k++;
            }

//grabs date and time info
            int year=(RootDirArray[i].create_date & 0xfe00)>>9;
            int month=(RootDirArray[i].create_date & 0x1e0)>>5;
            int day=RootDirArray[i].create_date & 0x1f;
            int hours=(RootDirArray[i].create_time & 0xf800)>>11;
            int minutes=(RootDirArray[i].create_time & 0x7E0)>>5;
//pads file size
            int nDigits = lenHelper(RootDirArray[i].file_size);
            char newFileSize[10-nDigits];
            memset(newFileSize,' ',10-nDigits);
            newFileSize[10-nDigits-1]='\0';

            if (RootDirArray[i].file_attr == 0x10){
                printf("D %d %s %s %d-%d-%d %d:%.2d\n", RootDirArray[i].file_size, newFileSize, newName,year+1980, month, day, hours,minutes);
            }else{
                printf("F %d %s %s %d-%d-%d %d:%.2d\n", RootDirArray[i].file_size, newFileSize, newName,year+1980, month, day, hours,minutes);            
            }
        }
    }

}