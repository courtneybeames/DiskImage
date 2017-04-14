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

//produces fat table
void getFatTable(FILE * fp, int addressOfFirstFAT, int bytesPerFAT){
    int j;
    int i=0;
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

// open disk file for reading
    FILE *fp = fopen(argv[1], "r"); 

//calculations and grabbing info from disk
    char * filename=argv[2];
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
    
    
//reading root directory into an array of structs
    root_attr RootDirArray[*maxRootDirEntries];
    fseek(fp, addressOfRootDirectory, SEEK_SET);
    fread(RootDirArray, sizeof(root_attr), *maxRootDirEntries, fp);
  
    int sizeArr[10];

    getFatTable(fp, addressOfFirstFAT, bytesPerFAT);

    fclose(fp);
    int found=0;
    char a;
    int i;
//goes through root directory and checks files
    for(i=0; i<*maxRootDirEntries; i++){
        if(RootDirArray[i].file_name[0] != 0 && RootDirArray[i].file_size !=0 && RootDirArray[i].first_cluster !=0 && RootDirArray[i].file_attr != 0x0F){
            //changes filename to correct format
            char newFileName[9];
            char newName[14];
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
            newName[13]='\0';
//gets address to start reading at
            int fatSectorNum = 33 + RootDirArray[i].first_cluster - 2;
            int startOfFAT = fatSectorNum * (*bytesPerSector);

            int newFatNumber=RootDirArray[i].first_cluster;
//opens disk file for reading
            int img = open(argv[1], O_RDONLY, 0777);
            struct stat stats; 
            fstat(img, &stats);                                                   
            char* p = mmap(0, stats.st_size, PROT_READ, MAP_SHARED, img, 0);

            if(strcmp(newName,filename) ==0){
                //found file: opens file and writes to it
                found=1;
                int file=open(filename, O_RDWR | O_CREAT | O_APPEND, 0777);
                int counter=0;
                int filesize= RootDirArray[i].file_size;
                //continues to read and write to file while fat table entries still valid
                while(newFatNumber<0xff0 && newFatNumber>0){

                    if(filesize>512){
                        write(file, p+startOfFAT, 512);
                        filesize -=512;
                    }else{
                        write(file, p+startOfFAT, filesize);
                    }
                    //grabs new fat number and from that, new address to read from
                    newFatNumber=fatTable[newFatNumber];
                    fatSectorNum = 33 + newFatNumber - 2;
                    startOfFAT = fatSectorNum * (*bytesPerSector);
                }
                close(file);


            }
            close(img);
        }
    }

    if(found==0){
        printf("File not found\n");
    }

}