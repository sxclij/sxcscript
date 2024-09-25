#include <fcntl.h>
#include <stdint.h>
#include <unistd.h>

#define script_path "test/1.txt"
#define script_capacity (1 << 16)

int main() {
    char src[script_capacity];
    int32_t mem[script_capacity];

    int fd = open(script_path, O_RDONLY);
    int src_n = read(fd, src, sizeof(src) - 1);
    src[src_n] = '\0';
    close(fd);
    write(STDOUT_FILENO, src, src_n);
    write(STDOUT_FILENO, "\n", 1);
    return 0;
}