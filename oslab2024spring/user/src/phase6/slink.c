#include "ulib.h"


#define TEST_FILE "testfile.txt"
#define SYMLINK "symlink.txt"
#define BUFFER_SIZE 1024

void create_file_with_content(const char *path, const char *content) {
    int fd = open(path, O_WRONLY | O_CREATE | O_TRUNC);

    if (fd < 0) {
        printf("Failed to create file %s\n", path);
        exit(1);
    }
    write(fd, content, strlen(content)); 

    close(fd);
}

void read_file_and_print(const char *path) {
    char buffer[BUFFER_SIZE];
    int fd = open(path, O_RDONLY);
    if (fd < 0) {
        printf("Failed to open file %s\n", path);
        exit(1);
    }
    int bytesRead = read(fd, buffer, BUFFER_SIZE - 1);
    if (bytesRead < 0) {
        printf("Failed to read file %s\n", path);
        close(fd);
        exit(1);
    }
    buffer[bytesRead] = '\0'; // Null terminate the string
    printf("Content of %s: %s\n", path, buffer);
    close(fd);
}

void test_symlink() {
    const char *content = "Hello, this is a test file.";

    // Create a file and write content to it
    create_file_with_content(TEST_FILE, content);

    // Create a symlink pointing to the created file
    if (symlink(TEST_FILE, SYMLINK) < 0) {
        printf("Failed to create symlink %s\n", SYMLINK);
        exit(1);
    }

    // Read and print the content via the symlink
    read_file_and_print(SYMLINK);

    // Clean up: remove the test file and the symlink
    unlink(TEST_FILE);

    unlink(SYMLINK);
}

int main() {
    printf("symlinktest begins.\n");
    test_symlink();
    printf("symlinktest ends.\n");
    return 0;
}