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
    sxcscript_kind_nop,
    sxcscript_kind_push,
    sxcscript_kind_push_val,
    sxcscript_kind_push_var,
    sxcscript_kind_call,
    sxcscript_kind_ret,
    sxcscript_kind_label,
    sxcscript_kind_jmp,
    sxcscript_kind_jze,
    sxcscript_kind_add,
    sxcscript_kind_sub,
    sxcscript_kind_mul,
    sxcscript_kind_div,
    sxcscript_kind_mod,
    sxcscript_kind_not,
    sxcscript_kind_and,
    sxcscript_kind_or,
    sxcscript_kind_xor,
    sxcscript_kind_eq,
    sxcscript_kind_ne,
    sxcscript_kind_lt,
    sxcscript_kind_le,
    sxcscript_kind_gt,
    sxcscript_kind_ge,
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
};

enum bool sxcscript_eq(struct sxcscript_token* token, const char* str) {
    for (int i = 0; i < token->size; i++) {
        if (token->data[i] != str[i]) {
            return false;
        }
        if (str[i] == '\0') {
            return false;
        }
    }
    if (str[token->size] == '\0') {
        return true;
    } else {
        return true;
    }
}
void sxcscript_insert(struct sxcscript_node* next, enum sxcscript_kind kind, struct sxcscript_token* token) {
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
void sxcscript_parse_push(struct sxcscript_node** node_itr, enum sxcscript_kind kind, struct sxcscript_token* ptr) {
    **node_itr = (struct sxcscript_node){
        .kind = kind,
        .token = ptr,
        .prev = *node_itr - 1,
        .next = *node_itr + 1,
    };
    *node_itr = *node_itr + 1;
}
void sxcscript_parse_expr(struct sxcscript* sxcscript, struct sxcscript_token** token_itr, struct sxcscript_node** node_itr) {
    struct sxcscript_token* token_this = *token_itr;
    struct sxcscript_node* node_this = *node_itr;
    if (sxcscript_eq(*token_itr, "(")) {
        (*token_itr)++;
        while (sxcscript_eq(*token_itr, ")") == false) {
            sxcscript_parse_expr(sxcscript, token_itr, node_itr);
            if (sxcscript_eq(*token_itr, ",")) {
                (*token_itr)++;
            }
        }
        (*token_itr)++;
    } else if (sxcscript_eq(*token_itr, ".")) {
        (*token_itr)++;
        sxcscript_parse_expr(sxcscript, token_itr, node_itr);
    } else if (sxcscript_eq(*token_itr, "if")) {
        (*token_itr)++;
        sxcscript_parse_expr(sxcscript, token_itr, node_itr);
        sxcscript_parse_push(node_itr, sxcscript_kind_jze, token_this);
        sxcscript_parse_expr(sxcscript, token_itr, node_itr);
        sxcscript_parse_push(node_itr, sxcscript_kind_label, token_this);
    } else if (sxcscript_eq(*token_itr, "loop")) {
        (*token_itr)++;
        sxcscript_parse_push(node_itr, sxcscript_kind_label, token_this);
        sxcscript_parse_expr(sxcscript, token_itr, node_itr);
        sxcscript_parse_push(node_itr, sxcscript_kind_jmp, token_this);
    } else if (sxcscript_eq(*token_itr, "add")) {
        (*token_itr)++;
        sxcscript_parse_expr(sxcscript, token_itr, node_itr);
        sxcscript_parse_push(node_itr, sxcscript_kind_add, token_this);
    } else if (sxcscript_eq(*token_itr, "sub")) {
        (*token_itr)++;
        sxcscript_parse_expr(sxcscript, token_itr, node_itr);
        sxcscript_parse_push(node_itr, sxcscript_kind_sub, token_this);
    } else if (sxcscript_eq(*token_itr, "mul")) {
        (*token_itr)++;
        sxcscript_parse_expr(sxcscript, token_itr, node_itr);
        sxcscript_parse_push(node_itr, sxcscript_kind_mul, token_this);
    } else if (sxcscript_eq(*token_itr, "div")) {
        (*token_itr)++;
        sxcscript_parse_expr(sxcscript, token_itr, node_itr);
        sxcscript_parse_push(node_itr, sxcscript_kind_div, token_this);
    } else if (sxcscript_eq(*token_itr, "mod")) {
        (*token_itr)++;
        sxcscript_parse_expr(sxcscript, token_itr, node_itr);
        sxcscript_parse_push(node_itr, sxcscript_kind_mod, token_this);
    } else if (sxcscript_eq(*token_itr, "not")) {
        (*token_itr)++;
        sxcscript_parse_expr(sxcscript, token_itr, node_itr);
        sxcscript_parse_push(node_itr, sxcscript_kind_not, token_this);
    } else if (sxcscript_eq(*token_itr, "and")) {
        (*token_itr)++;
        sxcscript_parse_expr(sxcscript, token_itr, node_itr);
        sxcscript_parse_push(node_itr, sxcscript_kind_and, token_this);
    } else if (sxcscript_eq(*token_itr, "or")) {
        (*token_itr)++;
        sxcscript_parse_expr(sxcscript, token_itr, node_itr);
        sxcscript_parse_push(node_itr, sxcscript_kind_or, token_this);
    } else if (sxcscript_eq(*token_itr, "xor")) {
        (*token_itr)++;
        sxcscript_parse_expr(sxcscript, token_itr, node_itr);
        sxcscript_parse_push(node_itr, sxcscript_kind_xor, token_this);
    } else if (sxcscript_eq(*token_itr, "eq")) {
        (*token_itr)++;
        sxcscript_parse_expr(sxcscript, token_itr, node_itr);
        sxcscript_parse_push(node_itr, sxcscript_kind_eq, token_this);
    } else if (sxcscript_eq(*token_itr, "ne")) {
        (*token_itr)++;
        sxcscript_parse_expr(sxcscript, token_itr, node_itr);
        sxcscript_parse_push(node_itr, sxcscript_kind_ne, token_this);
    } else if (sxcscript_eq(*token_itr, "lt")) {
        (*token_itr)++;
        sxcscript_parse_expr(sxcscript, token_itr, node_itr);
        sxcscript_parse_push(node_itr, sxcscript_kind_lt, token_this);
    } else if (sxcscript_eq(*token_itr, "le")) {
        (*token_itr)++;
        sxcscript_parse_expr(sxcscript, token_itr, node_itr);
        sxcscript_parse_push(node_itr, sxcscript_kind_le, token_this);
    } else if (sxcscript_eq(*token_itr, "gt")) {
        (*token_itr)++;
        sxcscript_parse_expr(sxcscript, token_itr, node_itr);
        sxcscript_parse_push(node_itr, sxcscript_kind_gt, token_this);
    } else if (sxcscript_eq(*token_itr, "ge")) {
        (*token_itr)++;
        sxcscript_parse_expr(sxcscript, token_itr, node_itr);
        sxcscript_parse_push(node_itr, sxcscript_kind_ge, token_this);
    } else if (sxcscript_eq(*token_itr + 1, "(")) {
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
void sxcscript_toinst(struct sxcscript* sxcscript) {
    struct sxcscript_node* node_itr = sxcscript->node;
    struct sxcscript_inst* inst_itr = sxcscript->inst;
    while (node_itr->kind != sxcscript_kind_null) {
        switch (node_itr->kind) {
            case sxcscript_kind_nop:
                break;
            case sxcscript_kind_push:
                if ('0' <= node_itr->token->data[0] && node_itr->token->data[0] <= '9') {
                    inst_itr->kind = sxcscript_kind_push_val;
                    inst_itr->value = 0;
                    for (int i = 0; i < node_itr->token->size; i++) {
                        inst_itr->value = (inst_itr->value * 10) + node_itr->token->data[i] - '0';
                    }
                } else {
                    inst_itr->kind = sxcscript_kind_push_var;
                }
                break;
            default:
                break;
        }
        node_itr = node_itr->next;
    }
}
void sxcscript_load(const char* src, struct sxcscript* sxcscript) {
    sxcscript_tokenize(src, sxcscript);
    sxcscript_parse(sxcscript);
    sxcscript_toinst(sxcscript);
}
void sxcscript_exec(struct sxcscript* sxcscript) {
    struct sxcscript_node* pc = sxcscript->node;
    int32_t* sp = sxcscript->mem;
    while (pc->kind != sxcscript_kind_null) {
        switch (pc->kind) {
            case sxcscript_kind_nop:
                pc = pc->next;
                break;
            case sxcscript_kind_push:
                *sp = 0;
                for (int i = 0; i < pc->token->size; i++) {
                    *sp = (*sp * 10) + pc->token->data[i] - '0';
                }
                sp++;
                pc = pc->next;
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