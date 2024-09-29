#include <fcntl.h>
#include <stdint.h>
#include <stdio.h>
#include <unistd.h>

#define sxcscript_path "test/5.txt"
#define sxcscript_capacity (1 << 16)
#define sxcscript_buf_capacity (1 << 10)

enum bool {
    false = 0,
    true = 1,
};
enum sxcscript_kind {
    sxcscript_kind_null,
    sxcscript_kind_nop,
    sxcscript_kind_push,
    sxcscript_kind_label,
    sxcscript_kind_push_val,
    sxcscript_kind_push_var,
    sxcscript_kind_call,
    sxcscript_kind_jmp,
    sxcscript_kind_jze,
    sxcscript_kind_mov,
    sxcscript_kind_movi,
    sxcscript_kind_add,
    sxcscript_kind_sub,
    sxcscript_kind_mul,
    sxcscript_kind_div,
    sxcscript_kind_mod,
};
enum sxcscript_global {
    sxcscript_global_sp,
    sxcscript_global_bp,
    sxcscript_global_ip,
};
struct sxcscript_token {
    const char* data;
    int32_t size;
};
struct sxcscript_node {
    enum sxcscript_kind kind;
    struct sxcscript_token* token;
    union {
        int32_t label_i;
        struct sxcscript_node* label_break;
        struct sxcscript_node* label_continue;
        int32_t literal;
    } val;
    struct sxcscript_node* hs1;
    struct sxcscript_node* hs2;
    struct sxcscript_node* hs3;
    enum bool throug;
    struct sxcscript_node* prev;
    struct sxcscript_node* next;
};
struct sxcscript_label {
    struct sxcscript_node* node;
    int32_t arg_size;
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
    struct sxcscript_label label[sxcscript_capacity];
    struct sxcscript_node* free;
    struct sxcscript_node* parsed;
    struct sxcscript_node* var;
    int32_t label_size;
};

uint64_t xorshift(uint64_t* x) {
    *x ^= *x << 13;
    *x ^= *x >> 7;
    *x ^= *x << 17;
    return *x;
}
enum bool sxcscript_token_eq(struct sxcscript_token* a, struct sxcscript_token* b) {
    if (a->size != b->size) {
        return false;
    }
    for (int i = 0; i < a->size; i++) {
        if (a->data[i] != b->data[i]) {
            return false;
        }
    }
    return true;
}
enum bool sxcscript_token_eq_str(struct sxcscript_token* a, const char* b) {
    for (int i = 0; i < a->size; i++) {
        if (a->data[i] != b[i] || b[i] == '\0') {
            return false;
        }
    }
    return true;
}
int32_t sxcscript_token_to_int32(struct sxcscript_token* token) {
    enum bool is_neg = token->data[0] == '-';
    int i = is_neg ? 1 : 0;
    int32_t ret = 0;
    for (; i < token->size; i++) {
        ret = (ret * 10) + token->data[i] - '0';
    }
    return is_neg ? -ret : ret;
}
void sxcscript_node_free(struct sxcscript_node** free, struct sxcscript_node* node) {
    node->prev = *free;
    (*free)->next = node;
    *free = node;
}
struct sxcscript_node* sxcscript_node_alloc(struct sxcscript_node** free) {
    struct sxcscript_node* node = *free;
    *free = (*free)->prev;
    *node = (struct sxcscript_node){.prev = NULL, .next = NULL};
    return node;
}
struct sxcscript_node* sxcscript_node_insert(struct sxcscript_node** free, struct sxcscript_node* next) {
    struct sxcscript_node* node = sxcscript_node_alloc(free);
    struct sxcscript_node* prev = next->prev;
    next->prev = node;
    *node = (struct sxcscript_node){.prev = prev, .next = next};
    if (prev != NULL) {
        prev->next = node;
    }
    return node;
}
struct sxcscript_node* sxcscript_node_find(struct sxcscript_node* src, struct sxcscript_node* this) {
    for (struct sxcscript_node* itr = src->prev; itr != NULL; itr = itr->prev) {
        if (itr->token == this->token) {
            return itr;
        }
    }
    for (struct sxcscript_node* itr = src->prev; itr != NULL; itr = itr->prev) {
        if (sxcscript_token_eq(itr->token, this->token)) {
            return itr;
        }
    }
    return NULL;
}
int32_t sxcscript_node_left(struct sxcscript_node* node) {
    int32_t ret = 0;
    for (struct sxcscript_node* itr = node; itr->prev != NULL; itr = itr->prev) {
        ret++;
    }
    return ret;
}
void sxcscript_node_init(struct sxcscript* sxcscript) {
    sxcscript->free = sxcscript->node;
    *(sxcscript->free) = (struct sxcscript_node){.prev = NULL, .next = NULL};
    for (int i = 1; i < sxcscript_capacity; i++) {
        sxcscript_node_free(&sxcscript->free, &sxcscript->node[i]);
    }
    sxcscript->parsed = sxcscript_node_alloc(&sxcscript->free);
    sxcscript->var = sxcscript_node_alloc(&sxcscript->free);
    sxcscript->label_size = 0;
}
void sxcscript_tokenize(const char* src, struct sxcscript_token* token) {
    struct sxcscript_token* token_itr = token;
    *token_itr = (struct sxcscript_token){src, 0};
    for (const char* src_itr = src; *src_itr != '\0'; src_itr++) {
        if (*src_itr == ' ' || *src_itr == '\n') {
            if (token_itr->size != 0) {
                token_itr++;
            }
        } else if (*src_itr == '(' || *src_itr == ')' || *src_itr == ',' || *src_itr == '.') {
            if (token_itr->size != 0) {
                token_itr++;
            }
            *(token_itr++) = (struct sxcscript_token){src_itr, 1};
        } else {
            if (token_itr->size == 0) {
                token_itr->data = src_itr;
            }
            token_itr->size++;
        }
    }
}
void sxcscript_parse_push(struct sxcscript_node** free, struct sxcscript_node* parsed, enum sxcscript_kind kind, struct sxcscript_token* token) {
    struct sxcscript_node* node = sxcscript_node_insert(free, parsed);
    *node = (struct sxcscript_node){.kind = kind, .token = token, .prev = node->prev, .next = node->next};
}
void sxcscript_parse_expr(struct sxcscript* sxcscript, struct sxcscript_token** token_itr) {
    struct sxcscript_token* token_this = *token_itr;
    struct sxcscript_node* node_this = sxcscript->parsed;
    if (sxcscript_token_eq_str(token_this, "(")) {
        (*token_itr)++;
        while (!sxcscript_token_eq_str(*token_itr, ")")) {
            sxcscript_parse_expr(sxcscript, token_itr);
            if (sxcscript_token_eq_str(*token_itr, ",")) {
                (*token_itr)++;
            }
        }
        (*token_itr)++;
    } else if (sxcscript_token_eq_str(token_this, ".")) {
        (*token_itr)++;
        sxcscript_parse_expr(sxcscript, token_itr);
    } else if (sxcscript_token_eq_str(token_this, "if")) {
        struct sxcscript_token* token_if = *token_itr;
        struct sxcscript_token* token_else = NULL;
        (*token_itr)++;
        sxcscript_parse_expr(sxcscript, token_itr);
        sxcscript_parse_push(&sxcscript->free, sxcscript->parsed, sxcscript_kind_jze, token_if);
        sxcscript_parse_expr(sxcscript, token_itr);
        if (sxcscript_token_eq_str(*token_itr, "else")) {
            token_else = *token_itr;
            (*token_itr)++;
            sxcscript_parse_push(&sxcscript->free, sxcscript->parsed, sxcscript_kind_jmp, token_else);
            sxcscript_parse_push(&sxcscript->free, sxcscript->parsed, sxcscript_kind_label, token_if);
            sxcscript_parse_expr(sxcscript, token_itr);
            sxcscript_parse_push(&sxcscript->free, sxcscript->parsed, sxcscript_kind_label, token_else);
        } else {
            sxcscript_parse_push(&sxcscript->free, sxcscript->parsed, sxcscript_kind_label, token_if);
        }
    } else if (sxcscript_token_eq_str(token_this, "loop")) {
        struct sxcscript_token* token_loop = *token_itr;
        (*token_itr)++;
        sxcscript_parse_push(&sxcscript->free, sxcscript->parsed, sxcscript_kind_label, token_loop);
        sxcscript_parse_expr(sxcscript, token_itr);
        sxcscript_parse_push(&sxcscript->free, sxcscript->parsed, sxcscript_kind_jmp, token_loop);
    } else if (sxcscript_token_eq_str(*token_itr + 1, "(")) {
        (*token_itr)++;
        sxcscript_parse_expr(sxcscript, token_itr);
        sxcscript_parse_push(&sxcscript->free, sxcscript->parsed, sxcscript_kind_call, token_this);
    } else {
        sxcscript_parse_push(&sxcscript->free, sxcscript->parsed, sxcscript_kind_push, token_this);
        (*token_itr)++;
    }
}
void sxcscript_p4arse(struct sxcscript* sxcscript) {
    struct sxcscript_token* token_itr = sxcscript->token;
    sxcscript_parse_expr(sxcscript, &token_itr);
}
struct sxcscript_node* sxcscript_analyze_pop(struct sxcscript_node** free, struct sxcscript_node*** stack_end) {
    struct sxcscript_node* node = *(--(*stack_end));
    node->throug = true;
    return node;
}
void sxcscript_analyze(struct sxcscript* sxcscript) {
    struct sxcscript_node* parsed_itr = sxcscript->parsed;
    struct sxcscript_node* stack[sxcscript_buf_capacity];
    struct sxcscript_node** stack_end = stack;
    struct sxcscript_node* hs1;
    struct sxcscript_node* hs2;
    struct sxcscript_node* hs3;
    while (parsed_itr->prev != NULL) {
        parsed_itr = parsed_itr->prev;
    }
    for (; parsed_itr->kind != sxcscript_kind_null; parsed_itr = parsed_itr->next) {
        if (parsed_itr->kind == sxcscript_kind_push) {
            if ('0' <= parsed_itr->token->data[0] && parsed_itr->token->data[0] <= '9' || parsed_itr->token->data[0] == '-') {
                parsed_itr->val.literal = sxcscript_token_to_int32(parsed_itr->token);
                parsed_itr->kind = sxcscript_kind_push_val;
            } else {
                parsed_itr->kind = sxcscript_kind_push_var;
            }
            *(stack_end++) = parsed_itr;
        } else if (parsed_itr->kind == sxcscript_kind_call) {
            if (sxcscript_token_eq_str(parsed_itr->token, "mov")) {
                hs2 = sxcscript_node_pop(&sxcscript->free, &stack_end);
                hs1 = sxcscript_node_pop(&sxcscript->free, &stack_end);
                if (hs2->kind == sxcscript_kind_push_val) {
                    parsed_itr->kind = sxcscript_kind_movi;
                }
                parsed_itr->hs1 = hs1;
                parsed_itr->hs2 = hs2;
            }
        }
    }
}
void sxcscript_init(struct sxcscript* sxcscript, const char* src) {
    sxcscript_node_init(sxcscript);
    sxcscript_tokenize(src, sxcscript->token);
    sxcscript_parse(sxcscript);
    sxcscript_analyze(sxcscript);
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