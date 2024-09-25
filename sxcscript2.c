#include <fcntl.h>
#include <stdint.h>
#include <unistd.h>

#define sxcscript_path "test/2.txt"
#define sxcscript_capacity (1 << 16)

enum sxcscript_kind {
    sxcscript_kind_null,
    sxcscript_kind_nop,
};
struct sxcscript_token {
    const char* data;
    uint32_t size;
};
struct sxcscript_node {
    enum sxcscript_kind kind;
    struct sxcscript_token* token;
    struct sxcscript_node* prev;
    struct sxcscript_node* next;
};
struct sxcscript {
    int32_t mem[sxcscript_capacity];
    struct sxcscript_token token[sxcscript_capacity];
    struct sxcscript_node node[sxcscript_capacity];
    struct sxcscript_node* free_itr;
    struct sxcscript_node* main_itr;
};

void sxcscript_free(struct sxcscript* sxcscript, struct sxcscript_node* this) {
    this->prev = sxcscript->free_itr;
    sxcscript->free_itr->next = this;
    sxcscript->free_itr = this;
}
struct sxcscript_node* sxcscript_allocate(struct sxcscript* sxcscript) {
    struct sxcscript_node* this = sxcscript->free_itr;
    sxcscript->free_itr = sxcscript->free_itr->prev;
    return this;
}
void sxcscript_tokenize_next(const char* src_itr, struct sxcscript_token** token_itr) {
    if ((*token_itr)->size == 0) {
        return;
    }
    (*token_itr)++;
    (*token_itr)->data = src_itr;
    (*token_itr)->size = 0;
}
void sxcscript_tokenize(const char* src, struct sxcscript* sxcscript) {
    struct sxcscript_token* token_itr = sxcscript->token;
    const char* src_itr = src;
    token_itr->size = 0;
    token_itr->data = src_itr;
    while (*src_itr) {
        if (*src_itr == ' ' || *src_itr == '\n') {
            sxcscript_tokenize_next(src_itr, &token_itr);
        } else if (*src_itr == '(' || *src_itr == ')' || *src_itr == ',' || *src_itr == '.') {
            sxcscript_tokenize_next(src_itr, &token_itr);
            token_itr->data = src_itr;
            token_itr->size = 1;
            sxcscript_tokenize_next(src_itr, &token_itr);
        } else {
            if (token_itr->size == 0) {
                token_itr->data = src_itr;
            }
            token_itr->size++;
        }
        src_itr++;
    }
}
void sxcscript_load(const char* src, struct sxcscript* sxcscript) {
    sxcscript->free_itr = sxcscript->node;
    for (uint32_t i = 0; i < sxcscript_capacity - 1; i++) {
        sxcscript_free(sxcscript, &sxcscript->node[i + 1]);
    }
    sxcscript->main_itr = sxcscript_allocate(sxcscript);
    sxcscript->main_itr->kind = sxcscript_kind_null;
    sxcscript->main_itr->token = NULL;
    sxcscript->main_itr->prev = NULL;
    sxcscript->main_itr->next = NULL;
    sxcscript_tokenize(src, sxcscript);
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

    sxcscript_load(src, &sxcscript);
    return 0;
}