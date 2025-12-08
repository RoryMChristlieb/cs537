/*
 * DO NOT MODIFY THIS FILE
 */

#ifndef __Disk_H__
#define __Disk_H__

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>

// a few disk parameters
#define BLOCK_SIZE  512
#define NUM_BLOCKS  1000


// disk error
#define E_DISK_ERROR -1

typedef struct block {
  char data[BLOCK_SIZE];
} Block;

int Disk_Init();
int Disk_Save(char* file);
int Disk_Load(char* file);
int Disk_Write(int block, char* buffer);
int Disk_Read(int block, char* buffer);

#endif