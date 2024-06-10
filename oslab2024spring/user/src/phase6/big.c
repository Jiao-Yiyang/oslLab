#include "ulib.h"

#define TEST_FILE "largefile.bin"
#define BUFFER_SIZE 4096 
#define LARGE_FILE_SIZE ((11 + 1024 + 1024) * BUFFER_SIZE)

void fill_buffer(char *buffer, size_t size, char fill_char) {
    memset(buffer, fill_char, size);
}

void create_large_file(const char *path) {
    assert(path);
    int fd = open(path, O_WRONLY | O_CREATE | O_TRUNC);
    if (fd < 0) {
        printf("Failed to create file %s\n", path);
        exit(1);
    }

    char *buffer = malloc(BUFFER_SIZE);
    fill_buffer(buffer, BUFFER_SIZE, 'A');
    
    size_t bytes_written = 0;
    while (bytes_written < LARGE_FILE_SIZE) {
        if (write(fd, buffer, BUFFER_SIZE) != BUFFER_SIZE) {
            printf("Failed to write to file %s\n", path);
            close(fd);
            exit(1);
        }
        bytes_written += BUFFER_SIZE;
    }
    
    close(fd);
    free(buffer);
}

void read_and_verify_large_file(const char *path) {
    assert(path);
    int fd = open(path, O_RDONLY);
    if (fd < 0) {
        printf("Failed to open file %s\n", path);
        exit(1);
    }

    char *buffer = malloc(BUFFER_SIZE);
    size_t bytes_read = 0;
    while (bytes_read < LARGE_FILE_SIZE) {
        int n = read(fd, buffer, BUFFER_SIZE);
        if (n < 0) {
            printf("Failed to read file %s\n", path);
            close(fd);
            exit(1);
        }
        for (int i = 0; i < n; i++) {
            if (buffer[i] != 'A') {
                printf("Data verification failed at offset %zu\n", bytes_read + i);
                close(fd);
                exit(1);
            }
        }
        bytes_read += n;
    }

    close(fd);
    free(buffer);
}

void test_large_file() {
    // Create a large file and write data to it
    create_large_file(TEST_FILE);
    
    // Read and verify the content of the large file
    read_and_verify_large_file(TEST_FILE);
    
    // Clean up: remove the test file
    unlink(TEST_FILE);
}

int main() {
    printf("large file test begins.\n");
    test_large_file();
    printf("large file test ends.\n");
    return 0;
}
