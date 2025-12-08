#include "TinyFS.h"
#include "TinyDisk.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

/************************************************************
 *  TINYFS DISK LAYOUT 
 *
 *  Block 0 : SUPERBLOCK
 *      - must store MAGIC_NUMBER at the start
 *
 *  Block 1 : INODE BITMAP
 *      - array of MAX_FILES ints (0 = free, 1 = used)
 *
 *  Block 2 : DATA BITMAP
 *      - array of NUM_BLOCKS ints (0 = free, 1 = used)
 *
 * Block 3 ... ??? : INODE BLOCKS
 *      Each inode contains:
 *          * filename[MAX_FILENAME_LENGTH]
 *          * int size
 *          * int dataBlocks[NUM_DIRECT_POINTERS]   // exactly 5
 *
 *  Remaining blocks after inode blocks are DATA BLOCKS
 ************************************************************/

/* ------------------------- */
/*       CONFIG CONSTANTS    */
/* ------------------------- */

#define MAGIC_NUMBER 0x12345678

/* File descriptors returned to user start at 3 (like stdin=0, stdout=1, stderr=2) */
#define FD_OFFSET 3

/* Size of Open File Table (max open files at once) */
#define OPEN_FILE_TABLE_SIZE 5

/* Block roles */
#define SUPERBLOCK_INDEX     0
#define INODE_BITMAP_INDEX   1
#define DATA_BITMAP_INDEX    2

/* ------------------------- */
/*       IN-MEMORY TYPES     */
/* ------------------------- */

typedef struct {
    char filename[MAX_FILENAME_LENGTH];
    int  size;
    int  dataBlocks[NUM_DIRECT_POINTERS];  // direct pointers to data blocks
} Inode;

typedef struct {
    int used;        // 0 = free, 1 = in use
    int inodeIndex;  // which inode this fd refers to
    int filePointer; // current byte offset within file
} OpenFile;

/* ------------------------- */
/*        GLOBAL STATE       */
/* ------------------------- */

/* Bitmaps live both in memory and on disk */
static int inodeBitmap[MAX_FILES];
static int dataBitmap[NUM_BLOCKS];

/* Open File Table */
static OpenFile oft[OPEN_FILE_TABLE_SIZE];

/* Layout variables (computed in FS_Boot) */
static int INODES_PER_BLOCK       = 0;
static int INODE_TABLE_START      = 0;
static int INODE_TABLE_BLOCKS     = 0;
static int DATA_BLOCK_START       = 0;

/* For FS_Sync (if used) */
static char g_disk_path[256] = {0};

/* ------------------------- */
/*      HELPER FUNCTIONS     */
/* ------------------------- */

/* Initialize Open File Table */
static void initOFT(void) {
    for (int i = 0; i < OPEN_FILE_TABLE_SIZE; i++) {
        oft[i].used = 0;
        oft[i].inodeIndex = -1;
        oft[i].filePointer = 0;
    }
}

/* Write both bitmaps back to disk */
static void syncBitmapsToDisk(void) {
    Disk_Write(INODE_BITMAP_INDEX, (char *)inodeBitmap);
    Disk_Write(DATA_BITMAP_INDEX, (char *)dataBitmap);
}

/* Compute the block and offset inside that block where a given inode lives */
static void readInode(int inodeIndex, Inode *ino) {
    int block   = INODE_TABLE_START + (inodeIndex / INODES_PER_BLOCK);
    int offset  = (inodeIndex % INODES_PER_BLOCK) * (int)sizeof(Inode);

    char buf[BLOCK_SIZE];
    Disk_Read(block, buf);
    memcpy(ino, buf + offset, sizeof(Inode));
}

static void writeInode(int inodeIndex, const Inode *ino) {
    int block   = INODE_TABLE_START + (inodeIndex / INODES_PER_BLOCK);
    int offset  = (inodeIndex % INODES_PER_BLOCK) * (int)sizeof(Inode);

    char buf[BLOCK_SIZE];
    Disk_Read(block, buf);
    memcpy(buf + offset, ino, sizeof(Inode));
    Disk_Write(block, buf);
}

/* Allocate a free inode in the bitmap */
static int allocateInode(void) {
    for (int i = 0; i < MAX_FILES; i++) {
        if (inodeBitmap[i] == 0) {
            inodeBitmap[i] = 1;
            Disk_Write(INODE_BITMAP_INDEX, (char *)inodeBitmap);
            return i;
        }
    }
    return -1;  // no free inode
}

/* Free an inode in the bitmap */
static void freeInode(int inodeIndex) {
    if (inodeIndex < 0 || inodeIndex >= MAX_FILES) return;
    inodeBitmap[inodeIndex] = 0;
    Disk_Write(INODE_BITMAP_INDEX, (char *)inodeBitmap);
}

/* Allocate a free data block in the bitmap (only from DATA_BLOCK_START onward) */
static int allocateDataBlock(void) {
    for (int i = DATA_BLOCK_START; i < NUM_BLOCKS; i++) {
        if (dataBitmap[i] == 0) {
            dataBitmap[i] = 1;
            Disk_Write(DATA_BITMAP_INDEX, (char *)dataBitmap);
            return i;
        }
    }
    return -1;  // no free space
}

/* Free a data block (mark bitmap 0) */
static void freeDataBlock(int blockIndex) {
    if (blockIndex < 0 || blockIndex >= NUM_BLOCKS) return;
    dataBitmap[blockIndex] = 0;
    Disk_Write(DATA_BITMAP_INDEX, (char *)dataBitmap);
}

/* Linear search over inodes to find a filename */
static int lookupFile(const char *name) {
    for (int i = 0; i < MAX_FILES; i++) {
        if (inodeBitmap[i]) {
            Inode ino;
            readInode(i, &ino);
            if (strcmp(ino.filename, name) == 0) {
                return i;
            }
        }
    }
    return -1;
}

/* Convert user-facing fd to index in oft[] */
static int fdToIndex(int fd) {
    int idx = fd - FD_OFFSET;
    if (idx < 0 || idx >= OPEN_FILE_TABLE_SIZE) return -1;
    if (!oft[idx].used) return -1;
    return idx;
}

/* ------------------------- */
/*        FS_Boot()          */
/* ------------------------- */

int FS_Boot(char *path) {
    printf("FS_Boot %s\n", path);

    if (Disk_Init() == -1) {
        printf("Disk_Init() failed\n");
        return E_DISK_ERROR;
    }

    /* Remember the path for possible FS_Sync */
    if (path != NULL) {
        strncpy(g_disk_path, path, sizeof(g_disk_path) - 1);
        g_disk_path[sizeof(g_disk_path) - 1] = '\0';
    }

    /* Compute layout based on inode size */
    INODES_PER_BLOCK   = BLOCK_SIZE / (int)sizeof(Inode);
    INODE_TABLE_BLOCKS = (MAX_FILES + INODES_PER_BLOCK - 1) / INODES_PER_BLOCK;
    INODE_TABLE_START  = 3;
    DATA_BLOCK_START   = INODE_TABLE_START + INODE_TABLE_BLOCKS;

    /* Try to load an existing disk image */
    if (Disk_Load(path) == 0) {
        char buf[BLOCK_SIZE];
        Disk_Read(SUPERBLOCK_INDEX, buf);

        int magic = 0;
        memcpy(&magic, buf, sizeof(int));

        if (magic != MAGIC_NUMBER) {
            // not a valid TinyFS filesystem
            return E_DISK_ERROR;
        }

        // load bitmaps into memory
        Disk_Read(INODE_BITMAP_INDEX, (char *)inodeBitmap);
        Disk_Read(DATA_BITMAP_INDEX, (char *)dataBitmap);

        initOFT();
        return 0;
    }

    /* Otherwise: create a new filesystem */

    char buf[BLOCK_SIZE];

    // superblock
    memset(buf, 0, BLOCK_SIZE);
    memcpy(buf, &MAGIC_NUMBER, sizeof(int));
    Disk_Write(SUPERBLOCK_INDEX, buf);

    // inode bitmap (all free)
    memset(inodeBitmap, 0, sizeof(inodeBitmap));
    Disk_Write(INODE_BITMAP_INDEX, (char *)inodeBitmap);

    // data bitmap (all free)
    memset(dataBitmap, 0, sizeof(dataBitmap));
    Disk_Write(DATA_BITMAP_INDEX, (char *)dataBitmap);

    // inode table blocks (zeroed)
    for (int i = 0; i < INODE_TABLE_BLOCKS; i++) {
        memset(buf, 0, BLOCK_SIZE);
        Disk_Write(INODE_TABLE_START + i, buf);
    }

    // data blocks (zeroed)
    for (int b = DATA_BLOCK_START; b < NUM_BLOCKS; b++) {
        memset(buf, 0, BLOCK_SIZE);
        Disk_Write(b, buf);
    }

    initOFT();

    // save freshly created disk image
    if (Disk_Save(path) < 0) {
        return E_DISK_ERROR;
    }

    return 0;
}

/* Optional helper: sync current in-memory disk to file */
int FS_Sync(void) {
    if (g_disk_path[0] == '\0') {
        return E_DISK_ERROR;
    }
    if (Disk_Save(g_disk_path) < 0) {
        return E_DISK_ERROR;
    }
    return 0;
}

/* ------------------------- */
/*        File_Create()      */
/* ------------------------- */

int File_Create(char *file) {
    if (file == NULL || strlen(file) == 0) {
        // Treat empty name as error (could pick another error if header defines one)
        return E_FILE_EXISTS; // or some generic error
    }

    // check if file already exists
    if (lookupFile(file) != -1) {
        return E_FILE_EXISTS;
    }

    int inodeIndex = allocateInode();
    if (inodeIndex < 0) {
        return E_NO_SPACE;
    }

    Inode ino;
    memset(&ino, 0, sizeof(Inode));
    strncpy(ino.filename, file, MAX_FILENAME_LENGTH - 1);
    ino.filename[MAX_FILENAME_LENGTH - 1] = '\0';
    ino.size = 0;
    for (int i = 0; i < NUM_DIRECT_POINTERS; i++) {
        ino.dataBlocks[i] = -1;
    }

    writeInode(inodeIndex, &ino);
    return 0;
}

/* ------------------------- */
/*        File_Open()        */
/* ------------------------- */

int File_Open(char *file) {
    int inodeIndex = lookupFile(file);
    if (inodeIndex < 0) {
        return E_NO_SUCH_FILE;
    }

    // find a free OFT entry
    for (int i = 0; i < OPEN_FILE_TABLE_SIZE; i++) {
        if (!oft[i].used) {
            oft[i].used        = 1;
            oft[i].inodeIndex  = inodeIndex;
            oft[i].filePointer = 0;
            return i + FD_OFFSET;  // user-facing fd
        }
    }

    return E_TOO_MANY_OPEN_FILES;
}

/* ------------------------- */
/*        File_Read()        */
/* ------------------------- */

int File_Read(int fd, void *buffer, int size) {
    if (size < 0 || buffer == NULL) {
        return 0;
    }

    int idx = fdToIndex(fd);
    if (idx < 0) {
        return E_BAD_FD;
    }

    OpenFile *of = &oft[idx];
    Inode ino;
    readInode(of->inodeIndex, &ino);

    int fp = of->filePointer;
    if (fp >= ino.size) {
        return 0; // EOF
    }

    int bytesToRead = size;
    if (fp + bytesToRead > ino.size) {
        bytesToRead = ino.size - fp;
    }

    int copied = 0;

    while (copied < bytesToRead) {
        int blockIndex = fp / BLOCK_SIZE;
        if (blockIndex >= NUM_DIRECT_POINTERS) {
            break; // beyond max file size
        }

        int diskBlock = ino.dataBlocks[blockIndex];
        if (diskBlock < 0) {
            break; // hole / not allocated
        }

        char blockBuf[BLOCK_SIZE];
        Disk_Read(diskBlock, blockBuf);

        int blockOffset = fp % BLOCK_SIZE;
        int chunk = BLOCK_SIZE - blockOffset;
        if (chunk > (bytesToRead - copied)) {
            chunk = bytesToRead - copied;
        }

        memcpy((char *)buffer + copied, blockBuf + blockOffset, chunk);

        fp      += chunk;
        copied  += chunk;
    }

    of->filePointer = fp;
    return copied;
}

/* ------------------------- */
/*        File_Write()       */
/* ------------------------- */

int File_Write(int fd, void *buffer, int size) {
    if (size < 0 || buffer == NULL) {
        return 0;
    }

    int idx = fdToIndex(fd);
    if (idx < 0) {
        return E_BAD_FD;
    }

    OpenFile *of = &oft[idx];
    Inode ino;
    readInode(of->inodeIndex, &ino);

    int fp = of->filePointer;
    int written = 0;

    while (written < size) {
        int blockIndex = fp / BLOCK_SIZE;
        if (blockIndex >= NUM_DIRECT_POINTERS) {
            // would exceed maximum file size
            return E_FILE_TOO_BIG;
        }

        // allocate block if needed
        if (ino.dataBlocks[blockIndex] < 0) {
            int newBlock = allocateDataBlock();
            if (newBlock < 0) {
                return E_NO_SPACE;
            }

            // zero out new data block
            char zeroBuf[BLOCK_SIZE];
            memset(zeroBuf, 0, BLOCK_SIZE);
            Disk_Write(newBlock, zeroBuf);

            ino.dataBlocks[blockIndex] = newBlock;
        }

        int diskBlock = ino.dataBlocks[blockIndex];

        char blockBuf[BLOCK_SIZE];
        Disk_Read(diskBlock, blockBuf);

        int blockOffset = fp % BLOCK_SIZE;
        int chunk = BLOCK_SIZE - blockOffset;
        if (chunk > (size - written)) {
            chunk = size - written;
        }

        memcpy(blockBuf + blockOffset, (char *)buffer + written, chunk);
        Disk_Write(diskBlock, blockBuf);

        fp      += chunk;
        written += chunk;
    }

    if (fp > ino.size) {
        ino.size = fp;
    }

    // save updated inode
    writeInode(of->inodeIndex, &ino);

    of->filePointer = fp;
    return written;
}

/* ------------------------- */
/*        File_Close()       */
/* ------------------------- */

int File_Close(int fd) {
    int idx = fdToIndex(fd);
    if (idx < 0) {
        return E_BAD_FD;
    }

    oft[idx].used        = 0;
    oft[idx].inodeIndex  = -1;
    oft[idx].filePointer = 0;

    return 0;
}

/* ------------------------- */
/*        File_Delete()      */
/* ------------------------- */

int File_Delete(char *file) {
    int inodeIndex = lookupFile(file);
    if (inodeIndex < 0) {
        return E_NO_SUCH_FILE;
    }

    // If file is currently open, do not delete
    for (int i = 0; i < OPEN_FILE_TABLE_SIZE; i++) {
        if (oft[i].used && oft[i].inodeIndex == inodeIndex) {
            return E_FILE_IN_USE;
        }
    }

    Inode ino;
    readInode(inodeIndex, &ino);

    // Free all data blocks used by this inode
    for (int i = 0; i < NUM_DIRECT_POINTERS; i++) {
        if (ino.dataBlocks[i] >= 0) {
            freeDataBlock(ino.dataBlocks[i]);
            ino.dataBlocks[i] = -1;
        }
    }

    // Clear inode on disk (optional but nice)
    memset(&ino, 0, sizeof(Inode));
    writeInode(inodeIndex, &ino);

    // Mark inode as free in bitmap
    freeInode(inodeIndex);

    return 0;
}

