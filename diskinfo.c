#include <pthread.h>
#include <string.h>
#include <unistd.h>    
#include <stdio.h>      
#include <stdlib.h>     
#include <stdbool.h>
#include <stdint.h>

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

//goes through all fat table entries and changes from little endian 
int getFatTable(FILE * fp, int addressOfFirstFAT, int bytesPerFAT){
    int j;
    int usedSectorsInFAT=0;
    for(j=addressOfFirstFAT; j<addressOfFirstFAT + bytesPerFAT; j+=3){
        int *counter = malloc(sizeof(int));
        fseek(fp, j, SEEK_SET);
        fread(counter, 1, 3, fp);

        int mover= (*counter & 0xff0000)>>16 | (*counter &  0x00ff00) | (*counter &  0x0000ff)<<16; 
       
        int x=(mover & 0xff0000)>>16 | (mover & 0x000f00);
        int y=(mover & 0x0000ff)<<4 | (mover & 0x00f000)>>12;

        if(x != 0){
            usedSectorsInFAT++;
        }
        if(y != 0){
            usedSectorsInFAT++;
        }
        
    }
    return usedSectorsInFAT;
}

int main(int argc, char *argv[]) {

// get disk file

    if(argc<2){
        printf("Error: not enough arguments.\n");
        exit(1);
    }
    if(argc>2){
        printf("Error: too many arguments.\n");
        exit(1);
    }

    FILE *fp = fopen(argv[1], "r");

//grab info from file:
    char * osname = grabCharFromFile(3, 8, fp);
    char * diskLabel = grabCharFromFile(43, 11, fp);

    int * bytesPerSector = grabIntFromFile(11, 2, fp);
    int * totalSectorCount = grabIntFromFile(19, 2, fp);
    int * numReserved = grabIntFromFile(14, 2, fp);
    int * numFATS = grabIntFromFile(16, 1, fp);
    int * sectorsPerFAT = grabIntFromFile(22, 2, fp);
    int * maxRootDirEntries = grabIntFromFile(17, 2, fp);

//calculations
    int totalSize = (*bytesPerSector) * (*totalSectorCount);
    int addressOfFirstFAT= (0 + *numReserved) * (*bytesPerSector);
    int bytesPerFAT = (*bytesPerSector) * (*sectorsPerFAT);
    
//count how many fat entries there are
    int usedSectorsInFAT=getFatTable(fp, addressOfFirstFAT, bytesPerFAT);
    
//more calculations
    int addressOfRootDirectory= addressOfFirstFAT + ((*numFATS) * (*sectorsPerFAT) * (*bytesPerSector));
    int addressOfDataRegion= addressOfRootDirectory + *maxRootDirEntries * 32;

//create struct array
    root_attr RootDirArray[*maxRootDirEntries];
    fseek(fp, addressOfRootDirectory, SEEK_SET);
    fread(RootDirArray, sizeof(root_attr), *maxRootDirEntries, fp);

    char *volLabel;
    int numFilesInRoot=0;
    int i;
//count how many files are in the root directory
//and check volume label
    for(i=0; i<*maxRootDirEntries; i++){
        if(RootDirArray[i].file_name[0] != 0 && RootDirArray[i].file_size !=0 && RootDirArray[i].first_cluster !=0 && RootDirArray[i].file_attr != 0x0F){
            numFilesInRoot++;
        }
        if(RootDirArray[i].file_attr == 0x08){
            volLabel= RootDirArray[i].file_name;

        }
    }
    
    printf("OS Name: %s \n", osname); 

    printf("Label of the disk: %s \n", volLabel);

    printf("Total size of the disk: %d \n", totalSize);

    printf("Free size of the disk: %d \n", totalSize-(usedSectorsInFAT*(*bytesPerSector)));

    printf("The number of files in the root directory: %d \n", numFilesInRoot);

    printf("Number of FAT copies: %d \n", *numFATS);

    printf("Sectors per FAT: %d \n", *sectorsPerFAT);


    return 0;
}