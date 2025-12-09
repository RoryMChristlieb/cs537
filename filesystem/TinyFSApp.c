#include <assert.h>
#include "TinyFS.h"
#include "TinyDisk.h"

void custom_assert(int condition, char *msg, int expected, int actual) {
    if (condition) {
        printf("Assertion succeeded:\n %s (Expected: %d, Actual: %d)\n\n", msg, expected, actual);
    } else {
        printf("\033[31mAssertion failed:\n %s (Expected: %d, Actual: %d)\033[0m\n\n", msg, expected, actual);
        assert(condition); 
    }
}

int main() {
    /* ------------------------------------------------------ *
     *                         FS_Boot                         *
     * ------------------------------------------------------ */
    int result = FS_Boot("filesystem.img");
    custom_assert(result == 0, "FS_Boot initializes filesystem", 0, result);


    /* ------------------------------------------------------ *
     *                      File_Create                        *
     * ------------------------------------------------------ */
    result = File_Create("alpha.txt");
    custom_assert(result == 0, "File_Create: create alpha.txt", 0, result);

    /* ------------------------------------------------------ *
     *          E_FILE_EXISTS test: Duplicate file create     *
     * ------------------------------------------------------ */
    result = File_Create("alpha.txt");
    custom_assert(result == E_FILE_EXISTS,"File_Create: duplicate file returns E_FILE_EXISTS",E_FILE_EXISTS, result);



    /* ------------------------------------------------------ *
     *                       File_Open                         *
     * ------------------------------------------------------ */
    int fd = File_Open("alpha.txt");
    custom_assert(fd >= 0, "File_Open: open existing file", 0, fd);

    /* ------------------------------------------------------ *
     *                       Non Existent File_Open           *
     * ------------------------------------------------------ */
    int badOpen = File_Open("doesnotexist.txt");
    custom_assert(badOpen == E_NO_SUCH_FILE,"File_Open: nonexistent file returns E_NO_SUCH_FILE",E_NO_SUCH_FILE, badOpen);


    /* ------------------------------------------------------ *
     *                        File_Write                       *
     * ------------------------------------------------------ */
    char msg[] = "Hello TinyFS";
    int written = File_Write(fd, msg, strlen(msg));
    custom_assert(written == (int)strlen(msg),"File_Write: write simple string",(int)strlen(msg), written);



    /* ------------------------------------------------------ *
     *                        File_Read                        *
     * ------------------------------------------------------ */
    char buffer[50] = {0};
    int read = File_Read(fd, buffer, sizeof(buffer));
    custom_assert(read >= 0,"File_Read: reading from valid fd",0, read);


    /* ------------------------------------------------------ *
     *                         File_Close                      *
     * ------------------------------------------------------ */
    result = File_Close(fd);
    custom_assert(result == 0, "File_Close: closing open file", 0, result);


    /* ------------------------------------------------------ *
     *              File_Delete                               *
     * ------------------------------------------------------ */
    result = File_Delete("alpha.txt");
    custom_assert(result == 0, "File_Delete: delete closed file", 0, result);


    /* ------------------------------------------------------ *
     *     Non existent file delete: E_NO_SUCH_FILE test      *
     * ------------------------------------------------------ */
    result = File_Delete("nonexistent_file.txt");
    custom_assert(result == E_NO_SUCH_FILE, "File_Delete: deleting non-existent file returns E_NO_SUCH_FILE", E_NO_SUCH_FILE, result);


    /* ------------------------------------------------------ *
     *     open file delete: E_FILE_IN_USE test               *
     * ------------------------------------------------------ */
    result = File_Create("beta.txt");
    custom_assert(result == 0, "File_Create: create beta.txt for open file test", 0, result);
    
    int fd_beta = File_Open("beta.txt");
    custom_assert(fd_beta >= 0, "File_Open: open beta.txt", 0, fd_beta);
    
    result = File_Delete("beta.txt");
    custom_assert(result == E_FILE_IN_USE, "File_Delete: deleting open file returns E_FILE_IN_USE", E_FILE_IN_USE, result);
    
    File_Close(fd_beta);
    File_Delete("beta.txt");


    /* ------------------------------------------------------ *
     *     E_BAD_FD test                                      *
     * ------------------------------------------------------ */
    int invalid_fd = 999;
    char read_buf[10] = {0};
    result = File_Read(invalid_fd, read_buf, 10);
    custom_assert(result == E_BAD_FD, "File_Read: invalid fd returns E_BAD_FD", E_BAD_FD, result);

    result = File_Write(invalid_fd, "test", 4);
    custom_assert(result == E_BAD_FD, "File_Write: invalid fd returns E_BAD_FD", E_BAD_FD, result);

    result = File_Close(invalid_fd);
    custom_assert(result == E_BAD_FD, "File_Close: invalid fd returns E_BAD_FD", E_BAD_FD, result);
    

    /* ------------------------------------------------------ *
     *     E_TOO_MANY_OPEN_FILES test                         *
     * ------------------------------------------------------ */
    int fds[5];
    for (int i = 0; i < 5; i++) {
        char filename[20];
        sprintf(filename, "file%d.txt", i);
        result = File_Create(filename);
        custom_assert(result == 0, "File_Create: create file for open limit test", 0, result);
        
        fds[i] = File_Open(filename);
        custom_assert(fds[i] >= 0, "File_Open: open file for limit test", 0, fds[i]);
    }
    
    // Try to open a 6th file
    result = File_Create("file6.txt");
    custom_assert(result == 0, "File_Create: create 6th file", 0, result);
    
    int fd_overflow = File_Open("file6.txt");
    custom_assert(fd_overflow == E_TOO_MANY_OPEN_FILES, "File_Open: opening 6th file returns E_TOO_MANY_OPEN_FILES", E_TOO_MANY_OPEN_FILES, fd_overflow);
    
    for (int i = 0; i < 5; i++) {
        File_Close(fds[i]);
        char filename[20];
        sprintf(filename, "file%d.txt", i);
        File_Delete(filename);
    }
    File_Delete("file6.txt");


    /* ------------------------------------------------------ *
     *     E_NO_SPACE test                                    *
     * ------------------------------------------------------ */
    int created_count = 0;
    for (int i = 0; i < MAX_FILES; i++) {
        char filename[30];
        sprintf(filename, "inode_test_%d.txt", i);
        result = File_Create(filename);
        if (result == 0) {
            created_count++;
        } else if (result == E_NO_SPACE) {
            break; //Run out of inodes
        }
    }
    
    printf("Created %d files before hitting limit\n", created_count);
    
    // The breaking point
    result = File_Create("overflow_file.txt");
    custom_assert(result == E_NO_SPACE, "File_Create: creating file when inodes exhausted returns E_NO_SPACE", E_NO_SPACE, result);
    
    // Clean up our mess
    for (int i = 0; i < created_count; i++) {
        char filename[30];
        sprintf(filename, "inode_test_%d.txt", i);
        File_Delete(filename);
    }

    return 0;
}
