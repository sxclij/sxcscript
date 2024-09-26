#include <fcntl.h>
#include <stdint.h>
#include <stdio.h>
#include <unistd.h>

#define sxcscript_path "test/3.txt"
#define sxcscript_capacity (1 << 16)

enum bool {
    false = 0,
    true = 1,
};
enum sxcscript_kind {
    sxcscript_kind_null,
};
struct sxcscript_token {
    const char* data;
    int32_t size;
};
struct sxcscript_node {
    enum sxcscript_kind kind;
    struct sxcscript_token* token;
    struct sxcscript_node* prev;
    struct sxcscript_node* next;
};
struct sxcscript_inst {
    enum sxcscript_kind kind;
    int32_t value;
};
struct sxcscript {
    int32_t mem[sxcscript_capacity];
    struct sxcscript_inst inst[sxcscript_capacity];
    struct sxcscript_token token[sxcscript_capacity];
    struct sxcscript_node node[sxcscript_capacity];
    struct sxcscript_node* free;
    struct sxcscript_node* parsed;
    struct sxcscript_node* var;
    struct sxcscript_node* label;
};

void sxcscript_node_free(struct sxcscript_node** free, struct sxcscript_node* node) {
    node->prev = *free;
    (*free)->next = node;
    *free = node;
}
struct sxcscript_node* sxcscript_node_alloc(struct sxcscript_node** free) {
    struct sxcscript_node* node = *free;
    *free = (*free)->prev;
    return node;
}
void sxcscript_node_init(struct sxcscript* sxcscript) {
    sxcscript->free = sxcscript->node;
    for(int i = 0; i < sxcscript_capacity-1; i++) {
        sxcscript_node_free(&sxcscript->free, &sxcscript->node[i]);
    }
    sxcscript->parsed = sxcscript_node_alloc(&sxcscript->free);
    sxcscript->var = sxcscript_node_alloc(&sxcscript->free);
    sxcscript->label = sxcscript_node_alloc(&sxcscript->free);
}
void sxcscript_init(struct sxcscript* sxcscript, const char* src) {
    sxcscript_node_init(sxcscript);
}

int main() {
    char src[sxcscript_capacity];
    static struct sxcscript sxcscript;

    int fd = open(sxcscript_path, O_RDONLY);
    int src_n = read(fd, src, sizeof(src) - 1);
    src[src_n] = '\0';
    close(fd);
    write(STDOUT_FILENO, src, src_n);
    write(STDOUT_FILENO, "\n", 1);

    sxcscript_init(&sxcscript, src);

    return 0;
}