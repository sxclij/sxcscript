#include <fcntl.h>
#include <stdint.h>
#include <stdio.h>
#include <unistd.h>

#define sxcscript_path "test/2.txt"
#define sxcscript_capacity (1 << 16)

enum bool {
    false = 0,
    true = 1,
};
enum sxcscript_kind {
    sxcscript_kind_null,
    sxcscript_kind_nop,
    sxcscript_kind_push,
    sxcscript_kind_call,
    sxcscript_kind_num,
    sxcscript_kind_var,
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

enum bool sxcscript_eq(struct sxcscript_token* token, const char* str) {
    for (int i = 0; i < token->size; i++) {
        if (token->data[i] != str[i]) {
            return false;
        }
    }
    if (str[token->size] == '\0') {
        return true;
    } else {
        return true;
    }
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
void sxcscript_parse_push(struct sxcscript_node** node_itr, enum sxcscript_kind kind, struct sxcscript_token* token) {
    *((*node_itr)++) = (struct sxcscript_node){
        .kind = kind,
        .token = token,
        .prev = node_itr[-1],
        .next = node_itr[+1],
    };
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
        sxcscript_parse_push(node_itr, sxcscript_kind_call, token_this);
    } else {
        sxcscript_parse_push(node_itr, sxcscript_kind_push, token_this);
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
void sxcscript_exec(struct sxcscript* sxcscript) {
    struct sxcscript_node* pc = sxcscript->node;
    int32_t* sp = sxcscript->mem;
    while (pc->kind != sxcscript_kind_null) {
        switch (pc->kind) {
            case sxcscript_kind_nop:
                pc++;
                break;
            case sxcscript_kind_push:
                *sp = 0;
                for (int i = 0; i < pc->token->size; i++) {
                    *sp = (*sp * 10) + pc->token->data[i] - '0';
                }
                sp++;
                pc++;
                break;
            case sxcscript_kind_call:
                if (sxcscript_eq(pc->token, "add")) {
                    sp[-2] = sp[-2] + sp[-1];
                    sp--;
                } else if (sxcscript_eq(pc->token, "mul")) {
                    sp[-2] = sp[-2] * sp[-1];
                    sp--;
                } else if (sxcscript_eq(pc->token, "mod")) {
                    sp[-2] = sp[-2] % sp[-1];
                    sp--;
                } else if (sxcscript_eq(pc->token, "print")) {
                    printf("%d\n", *(--sp));
                }
                pc++;
                break;
        }
    }
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
    sxcscript_exec(&sxcscript);
    return 0;
}