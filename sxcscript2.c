#include <fcntl.h>
#include <unistd.h>

#define script_path "testscript.txt"
#define script_capacity (1 << 16)

int main() {
    char buf[script_capacity];
    int fd = open(script_path, O_RDONLY);
    int buf_n = read(fd, buf, sizeof(buf) - 1);
    buf[buf_n] = '\0';
    close(fd);
    write(STDOUT_FILENO, buf, buf_n);
}