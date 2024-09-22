#include <unistd.h>

int main() {
    write(STDOUT_FILENO, "good morning world!\n", 21);
}