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
     * You must add at least 5 more testcases to cover the following:
     *   - Non existent file delete: E_NO_SUCH_FILE test
     *   - open file delete: E_FILE_IN_USE test
     *   - E_BAD_FD test
     *   - E_TOO_MANY_OPEN_FILES test
     *   - E_NO_SPACE test
     * ------------------------------------------------------ */


    /* Only for graduate students:
    * You must add at least five more test cases to test File_Seek(), including at least two edge cases for error handling
    */

    return 0;
}
