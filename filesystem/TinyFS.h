/*
 * DO NOT MODIFY THIS FILE
*/
    
#ifndef __LibFS_h__
#define __LibFS_h__

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#define MAX_FILES 100 
#define MAX_OPEN_FILES 5
#define MAX_FILE_SIZE (BLOCK_SIZE * 5)
#define NUM_DIRECT_POINTERS 5               // this filesystem only have direct pointers
#define MAX_FILENAME_LENGTH 128

#define E_FILE_EXISTS -2
#define E_NO_SUCH_FILE -3
#define E_TOO_MANY_OPEN_FILES -4
#define E_BAD_FD -5
#define E_NO_SPACE -6
#define E_FILE_TOO_BIG -7
#define E_SEEK_OUT_OF_BOUNDS -8
#define E_FILE_IN_USE -9
       
// File system generic call
int FS_Boot(char *path);

// file ops
int File_Create(char *file);
int File_Open(char *file);
int File_Read(int fd, void *buffer, int size);
int File_Write(int fd, void *buffer, int size);
int File_Close(int fd);
int File_Delete(char *file);

// Only Graduate Students uncomment this
int File_Seek(int fd, int offset);

#endif


