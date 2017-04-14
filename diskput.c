#include <pthread.h>
#include <string.h>
#include <unistd.h>    
#include <stdio.h>      
#include <stdlib.h>     
#include <stdbool.h>
#include <stdint.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/mman.h>


#define FILE_NAME_LEN 8
#define FILE_EXT_LEN 3
#define read_only_mask 0x01
#define hidden_mask 0x02
#define system_mask 0x04
#define vol_label_mask 0x08
#define subdirectory_mask 0x10
#define archive_mask 0x20
#define device_mask 0x40

int fatTable[4608];
int usedSectorsInFAT;

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

void getFatTable(FILE * fp, int addressOfFirstFAT, int bytesPerFAT){
    int j;
    int i=0;
    usedSectorsInFAT=0;
    for(j=addressOfFirstFAT; j<addressOfFirstFAT + bytesPerFAT; j+=3){
        int *counter = malloc(sizeof(int));
        fseek(fp, j, SEEK_SET);
        fread(counter, 1, 3, fp);

        int mover= (*counter & 0xff0000)>>16 | (*counter &  0x00ff00) | (*counter &  0x0000ff)<<16; 
       
        int x=(mover & 0xff0000)>>16 | (mover & 0x000f00);
        int y=(mover & 0x0000ff)<<4 | (mover & 0x00f000)>>12;

        if (i==4608 || i==4607 || i==4609){
            break;
        }

        if(x != 0){
            usedSectorsInFAT++;
        }
        if(y != 0){
            usedSectorsInFAT++;
        }

        fatTable[i]=x;
        fatTable[i+1]=y;
        i+=2;
        
    }
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

// open disk file

    FILE *fp = fopen(argv[1], "rw"); // read fild

    char * filename=argv[2];
//calculations and getting info from disk
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
//creating a root directory array of structs  
    root_attr RootDirArray[*maxRootDirEntries];
    
    fseek(fp, addressOfRootDirectory, SEEK_SET);
    fread(RootDirArray, sizeof(root_attr), *maxRootDirEntries, fp);
    int sizeArr[10];

    int found=0;
    char a;
    int i;

//opening disk to mmap
    int img = open(argv[1], O_RDWR);
    struct stat stats; 
    fstat(img, &stats);                                                   
    char* p = mmap(0, stats.st_size, PROT_READ | PROT_WRITE, MAP_SHARED, img, 0);

//getting values in fat table into an array
    getFatTable(fp, addressOfFirstFAT, bytesPerFAT);

    fclose(fp);

//opening file to put in
    int file=open(argv[2], O_RDWR | O_CREAT, 0777);

    struct stat fstats; 
    fstat(file, &fstats); 

    if(fstats.st_size == 0){
        printf("File not found.\n");
        exit(1);
    }

    if(fstats.st_size > totalSize-(usedSectorsInFAT*(*bytesPerSector))){
        printf("Not enough free space in the disk image.\n");
        exit(1);
    }


    char* p2 = mmap(0, fstats.st_size, PROT_READ | PROT_WRITE, MAP_SHARED, file, 0);
//getting first available spot in fat table
    int s=0;
    int fatEntryNumber=0;
    while(fatTable[s] != 0 && s<4608){
        fatEntryNumber++;
        s++;
    }

//where we want to place the file
    int addressToPutFile=(33+fatEntryNumber-2)* *bytesPerSector;

    s=0;
//getting first available spot in root directory
    while(RootDirArray[s].file_name[0] != 0 && s<224){
        s++;
    }

//copying file name into root directory
    char newFileName[8];
    char newExt[4];
    int k=0;
    int space;

    int index=addressOfRootDirectory+(s*32);
    while(k< 8){
        p[index]=argv[2][k];

        k++;
        index++;
        if(argv[2][k] == '.'){
            space=k;
            break;
        }
    }
    while(k<8){
        p[index]=' ';
        index++;
        k++;
    }


    int l=0;
    while(l<3){
        p[index]=argv[2][space+1];
        index++;
        space++;
        l++;
    }

//grab beginning address of root directory
    int original=addressOfRootDirectory+(s*32);

//put first logical cluster into root directory
    p[original+26]=fatEntryNumber & 0xff;
    p[original+27]=(fatEntryNumber & 0xff00) >>8;

    fstat(file, &stats);
    int fileSize= stats.st_size;
//put file size into root directory
    p[original+28]=(fileSize & 0xff);
    p[original+29]=(fileSize & 0xff00)>>8;
    p[original+30]=(fileSize & 0xff0000)>>16;
    p[original+31]=(fileSize & 0xff000000)>>24;
 
    int readThrough;
    int c=0;

    int flag=0;
    for(readThrough=0; readThrough<fileSize; readThrough++){
        if(c==512){
            //put new fat number into fat table
            if(fatEntryNumber%2 ==0){ //its even!!!
                int lowBits=(1+(3*fatEntryNumber)/2);
                int highBits=(3*fatEntryNumber)/2;
                int entryNum=fatEntryNumber+1;
                int checkH=(entryNum & 0xF00)>>8;
                int checkM=(entryNum & 0x0f0);
                int checkL=(entryNum & 0x00f);

                p[512+lowBits]= p[512+lowBits] | checkH;
                p[512+highBits]= checkM | checkL;
                p[5120+lowBits]= p[5120+lowBits] | checkH;
                p[5120+highBits]= checkM | checkL;
            }else{ //its odd
                int lowBits=(1+(3*fatEntryNumber)/2);
                int highBits=(3*fatEntryNumber)/2;
                int entryNum=fatEntryNumber+1;
                int checkH=(entryNum & 0xF00)>>4;
                int checkM=(entryNum & 0x0f0)>>4;
                int checkL=(entryNum & 0x00f)<<4;

                p[512+lowBits]= checkM | checkH;
                p[512+highBits]= p[512+highBits] | checkL;
                p[5120+lowBits]= checkM | checkH;
                p[5120+highBits]= p[5120+highBits] | checkL;
            }
            fatEntryNumber+=1;
            c=0;
        }
        //read byte by byte into data sector
        p[addressToPutFile+readThrough]=p2[readThrough];
        
        c++;
    }
    //put ending byte into fat table
    if(fatEntryNumber%2 ==0){
        int lowBits=(1+(3*fatEntryNumber)/2);
        int highBits=(3*fatEntryNumber)/2;
        p[512+lowBits]= p[512+lowBits] | 0x0F;
        p[512+highBits]= 0xFF;
        p[5120+lowBits]= p[5120+lowBits] | 0x0F;
        p[5120+highBits]= 0xFF;
    }else{
        int lowBits=(1+(3*fatEntryNumber)/2);
        int highBits=(3*fatEntryNumber)/2;
        p[512+lowBits]= 0xFF;
        p[512+highBits]= p[512+highBits] | 0xF0;
        p[5120+lowBits]= 0xFF;
        p[5120+highBits]= p[5120+highBits] | 0xF0;
    }


}