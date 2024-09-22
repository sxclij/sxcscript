#include <fcntl.h>
#include <unistd.h>

#define script_path "testscript.txt"
#define script_capacity (1 << 16)

enum script_kind {
    null,
    add,
    ret,
};
struct script_node {
    enum script_kind kind;
    int amount;
    struct script_node* prev;
    struct script_node* next;
};

int main() {
    char buf[script_capacity];
    int fd = open(script_path, O_RDONLY);
    int buf_n = read(fd, buf, sizeof(buf) - 1);
    buf[buf_n] = '\0';
    close(fd);
    write(STDOUT_FILENO, buf, buf_n);
}