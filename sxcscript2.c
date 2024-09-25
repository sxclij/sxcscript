#include <fcntl.h>
#include <stdint.h>
#include <unistd.h>

#define sxcscript_path "test/2.txt"
#define sxcscript_capacity (1 << 16)

enum sxcscript_kind {
    sxcscript_kind_null,
    sxcscript_kind_nop,
    sxcscript_kind_push,
    sxcscript_kind_call,
    sxcscript_kind_ret,
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
};

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
void sxcscript_parse_expr(struct sxcscript* sxcscript, struct sxcscript_token** token_itr, struct sxcscript_node** node_itr) {
    struct sxcscript_token* token_this = *token_itr;
    if ((*token_itr)->data[0] == '(') {
        (*token_itr)++;
        while ((*token_itr)->data[0] != ')') {
            sxcscript_parse_expr(sxcscript, token_itr, node_itr);
            if ((*token_itr)->data[0] == ',') {
                (*token_itr)++;
            }
        }
        (*token_itr)++;
    } else if ((*token_itr)->data[0] == '.') {
        (*token_itr)++;
        sxcscript_parse_expr(sxcscript, token_itr, node_itr);
    } else if ((*token_itr + 1)->data[0] == '(') {
        (*token_itr)++;
        sxcscript_parse_expr(sxcscript, token_itr, node_itr);
        *((*node_itr)++) = (struct sxcscript_node){
            .kind = sxcscript_kind_call,
            .token = token_this,
            .prev = NULL,
            .next = NULL,
        };
    } else {
        *((*node_itr)++) = (struct sxcscript_node) {
            .kind = sxcscript_kind_push,
            .token = token_this,
            .prev = NULL,
            .next = NULL,
        };
        (*token_itr)++;
    }
}
void sxcscript_parse(struct sxcscript* sxcscript) {
    struct sxcscript_token* token_itr = sxcscript->token;
    struct sxcscript_node* node_itr = sxcscript->node;
    sxcscript_parse_expr(sxcscript, &token_itr, &node_itr);
}
void sxcscript_load(const char* src, struct sxcscript* sxcscript) {
    sxcscript_tokenize(src, sxcscript);
    sxcscript_parse(sxcscript);
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