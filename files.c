#include <stdio.h>
#include <string.h>
#include <strings.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <time.h>
#include <endian.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <dirent.h>
#include "client.h"
#include "bigint.h"
#include "bitfield.h"

char* dirname = "downloads";
int SIZE = 50;
char* format = ".bin";

int createdir(){
    int check;
    DIR* dir = opendir(dirname);
    if (dir) {
    /* Directory exists. */
    closedir(dir);
    return 1;
    }
    check = mkdir(dirname,0777);

    if (!check){
        printf("Directory created\n");
        return 1;
    }   
    else {
        printf("Unable to create directory\n");
        return -1;
    }

}



FILE *createfile(uint8_t *data, int data_length, char* name){
    // printf("in create file\n");
    char *filename, *fulldir;
    FILE *fp;
	
    filename=malloc(SIZE*sizeof(char));
    fulldir=malloc(SIZE*sizeof(char));

	strcpy(filename, name);

    strcpy(fulldir, dirname);
    strcat(fulldir, "/");
    strcat(fulldir, filename);
    //or .txt or whatever format we want
    strcat(fulldir, format);

    if ((fp = fopen(fulldir, "wb+")) != NULL ) {
        // printf("File created\n");
        fwrite(data, 1, data_length, fp);
        // fwrite(data, sizeof(data), 1, fp);
        // for(int i = 0; i < data_length; i++){
        //     printf("%d\n",data[i]);
        // }
        // fclose(fp);
    //write to file using fwrite, fprintf or fputs depending on how we want to store it

//  printf("read file to test\n");
//     unsigned char buffer[10];
//     FILE *ptr;
//     ptr = fopen("downloads/1.bin","rb");  // r for read, b for binary

//     fread(buffer,1,10,ptr); // read 10 bytes to our buffer
//         for(int i = 0; i < 10; i++){
//             printf("%d\n",buffer[i]);
//         }
//         fclose(fp);
//         return fp;
//     } else {
//         printf("Unable to create file\n");
        return fp;
    }
   
    // End function
    return NULL;
}

int checkexistance(char* name){
    char *fulldir;
    fulldir=malloc(SIZE*sizeof(char));
    strcpy(fulldir, dirname);
    strcat(fulldir, "/");
    strcat(fulldir, name);
    strcat(fulldir, format);

    if( access(fulldir, F_OK ) == 0 ) {
        return 1;
    } else {
        return 0;
    }
}

int checkcomplete(int i){
    //i is total files there should be
    for (int j = 0; j < i; j++){
        char str[50];
        sprintf(str, "%d", j);
        if(checkexistance(str)==0){
            return 0;
        }
    }
    return 1;
}

int combineallfiles(int n, int sizeofeach){
printf("in combineallfiles\n");
    if (checkcomplete(n)==0){
        return 0;
    }
printf("files are complete\n");
    // int X = (n+1) * sizeofeach;
    char finalfile[50];
    strcpy(finalfile,"completed");
    strcat(finalfile,format);
    FILE *fp = fopen(finalfile, "wb");
    fseek(fp, 0, SEEK_SET);
    int i = 0;
printf("writing to completed files now\n");
    while(i < n){
        char filename[50];
        char name[16];
        sprintf(name, "%d", i);
        strcpy(filename, dirname);
        strcat(filename, "/");
        strcat(filename,name);
        strcat(filename,format);
        FILE *fp1 = fopen(filename, "rb");
        if (fp1 == NULL){
         puts("Could not open files");
         return 0;
        }
        char c;
        while ((c = fgetc(fp1)) != EOF)
            fputc(c, fp);
        i++;
        fclose(fp1);
    }
    fclose(fp);
    rename("completed.bin",final_filename);
    return 0;
}

char *createfilename(int index) {
    char *name = calloc(1,10);
    sprintf(name, "%d", index);
    return name;
}

int readFile(uint8_t *buffer, int begin, int length, int index) {
    char filename[50];
    char name[10];
    sprintf(name, "%d", index);
    strcpy(filename, dirname);
    strcat(filename, "/");
    strcat(filename,name);
    strcat(filename,format);
    FILE *fp = fopen(filename, "rb");
    int i = 0;
    while (i<begin) {
        fgetc(fp);
        i++;
    }
    char c;
    i = 0;
    while ((c=fgetc(fp)) != EOF && i < length) {
        buffer[i++] = c;
    }

    fclose(fp);
    if (i != length) {
        return 0;
    }

    return 1;
}

//input empty bitfield and n which represent length/file to check up to
//size should be in bytes
int getbitfieldfromfiles(uint8_t *bitfield, int n, long int size){
    //check if file 0.bin to n-1.bin exists and if size is > size
    for (int i = 0; i < n; i++){
        char filename[50];
        char name[16];
        sprintf(name, "%d", i);
        strcpy(filename, dirname);
        strcat(filename, "/");
        strcat(filename,name);
        strcat(filename,format);
        FILE *fp = fopen(filename, "rb");
        if (fp == NULL){
            //file not found
            // printf("%d does not exist\n",i);
            continue;
        }
        else {
            // printf("%d exists\n",i);
            fseek(fp, 0L, SEEK_END);
            long int res = ftell(fp);
            if(res >= size){
                // printf("%d exists and is full\n",i);
                set_bit(bitfield,i);
            }
        }
    }
    return 0;
}
