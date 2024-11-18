#include <fcntl.h>
#include <sys/resource.h>
#include <unistd.h>

#define stacksize (128 * 1014 * 1024)
#define sxcscript_path "test/08.rs"
#define sxcscript_mem_size (1 << 20)
#define sxcscript_compile_size (1 << 20)
#define sxcscript_buf_size (1 << 10)
#define sxcscript_global_size (1 << 12)
#define sxcscript_stack_size (1 << 8)

enum bool {
    false = 0,
    true = 1,
};
enum sxcscript_kind {
    sxcscript_kind_null,
    sxcscript_kind_nop,
    sxcscript_kind_push_const,
    sxcscript_kind_push_varaddr,
    sxcscript_kind_test01,
    sxcscript_kind_test02,
    sxcscript_kind_test03,
    sxcscript_kind_global_get,
    sxcscript_kind_global_set,
    sxcscript_kind_call,
    sxcscript_kind_return,
    sxcscript_kind_jmp,
    sxcscript_kind_jze,
    sxcscript_kind_or,
    sxcscript_kind_and,
    sxcscript_kind_eq,
    sxcscript_kind_ne,
    sxcscript_kind_lt,
    sxcscript_kind_gt,
    sxcscript_kind_add,
    sxcscript_kind_sub,
    sxcscript_kind_mul,
    sxcscript_kind_div,
    sxcscript_kind_mod,
    sxcscript_kind_usleep,
    sxcscript_kind_ext,
    sxcscript_kind_label,
    sxcscript_kind_label_fnend,
};
enum sxcscript_global {
    sxcscript_global_null = 0,
    sxcscript_global_ip = 1,
    sxcscript_global_sp = 2,
    sxcscript_global_bp = 3,
};
struct sxcscript_token {
    const char* data;
    int size;
};
struct sxcscript_node {
    enum sxcscript_kind kind;
    struct sxcscript_token* token;
    int val;
};
struct sxcscript_label {
    struct sxcscript_token* token;
    int arg_size;
    int inst_i;
};
union sxcscript_mem {
    enum sxcscript_kind kind;
    int val;
};
struct sxcscript {
    union sxcscript_mem mem[sxcscript_mem_size];
    struct sxcscript_token token[sxcscript_compile_size];
    struct sxcscript_node node[sxcscript_compile_size];
    struct sxcscript_label label[sxcscript_compile_size];
    union sxcscript_mem* inst_begin;
    union sxcscript_mem* data_begin;
    int label_size;
};

void sxcscript_parse_expression(struct sxcscript_token** token_itr, struct sxcscript_node** node_itr, struct sxcscript_label* label, int* label_size, int label_break, int label_continue);

enum bool sxcscript_isdigit(const char* str) {
    char ch = str[0];
    return (ch >= '0' && ch <= '9') || ch == '-';
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
    if (b[a->size] != '\0') {
        return false;
    }
    return true;
}
int sxcscript_token_to_int32(struct sxcscript_token* token) {
    enum bool is_neg = token->data[0] == '-';
    int i = is_neg ? 1 : 0;
    int ret = 0;
    for (; i < token->size; i++) {
        ret = (ret * 10) + token->data[i] - '0';
    }
    return is_neg ? -ret : ret;
}
void sxcscript_readfile(char* dst) {
    int fd = open(sxcscript_path, O_RDONLY);
    int dst_n = read(fd, dst, sxcscript_compile_size);
    dst[dst_n] = '\0';
    close(fd);
}
void sxcscript_tokenize(const char* src, struct sxcscript_token* token) {
    struct sxcscript_token* token_itr = token;
    *token_itr = (struct sxcscript_token){src, 0};
    for (const char* src_itr = src; *src_itr != '\0'; src_itr++) {
        if (*src_itr == ' ' || *src_itr == '\n') {
            if (token_itr->size != 0) {
                token_itr++;
            }
        } else if (*src_itr == '(' || *src_itr == ')' || *src_itr == ',' || *src_itr == '.' || *src_itr == '*' || *src_itr == '&') {
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
    if (token_itr->size != 0) {
        token_itr++;
    }
    *token_itr = (struct sxcscript_token){NULL, 0};
}
void sxcscript_parse_push(struct sxcscript_node** node_itr, enum sxcscript_kind kind, struct sxcscript_token* token, int val) {
    **node_itr = (struct sxcscript_node){.kind = kind, .token = token, .val = val};
    *node_itr += 1;
}
void sxcscript_parse_primary(struct sxcscript_token** token_itr, struct sxcscript_node** node_itr, struct sxcscript_label* label, int* label_size, int label_break, int label_continue) {
    char ch = (*token_itr)->data[0];
    if (sxcscript_token_eq_str(*token_itr, "(")) {
        *token_itr += 1;
        while (!sxcscript_token_eq_str(*token_itr, ")")) {
            sxcscript_parse_expression(token_itr, node_itr, label, label_size, label_break, label_continue);
            if (sxcscript_token_eq_str(*token_itr, ",")) {
                *token_itr += 1;
            }
        }
        *token_itr += 1;
    } else if (sxcscript_isdigit((*token_itr)->data)) {
        sxcscript_parse_push(node_itr, sxcscript_kind_push_const, *token_itr, 0);
        *token_itr += 1;
    } else {
        sxcscript_parse_push(node_itr, sxcscript_kind_push_varaddr, *token_itr, 0);
        sxcscript_parse_push(node_itr, sxcscript_kind_global_get, NULL, 0);
        *token_itr += 1;
    }
}
void sxcscript_parse_postfix(struct sxcscript_token** token_itr, struct sxcscript_node** node_itr, struct sxcscript_label* label, int* label_size, int label_break, int label_continue) {
    struct sxcscript_token* token_start = *token_itr;
    if (sxcscript_token_eq_str(*token_itr + 1, "(")) {
        *token_itr += 1;
        sxcscript_parse_expression(token_itr, node_itr, label, label_size, label_break, label_continue);
        if (sxcscript_token_eq_str(token_start, "return")) {
            sxcscript_parse_push(node_itr, sxcscript_kind_return, NULL, 0);
        } else if (sxcscript_token_eq_str(token_start, "usleep")) {
            sxcscript_parse_push(node_itr, sxcscript_kind_usleep, NULL, 0);
        } else if (sxcscript_token_eq_str(token_start, "ext")) {
            sxcscript_parse_push(node_itr, sxcscript_kind_ext, NULL, 0);
        } else {
            sxcscript_parse_push(node_itr, sxcscript_kind_call, token_start, 0);
        }
    } else {
        sxcscript_parse_primary(token_itr, node_itr, label, label_size, label_break, label_continue);
    }
}
void sxcscript_parse_unary(struct sxcscript_token** token_itr, struct sxcscript_node** node_itr, struct sxcscript_label* label, int* label_size, int label_break, int label_continue) {
    if (sxcscript_token_eq_str(*token_itr, "&")) {
        *token_itr += 1;
        sxcscript_parse_push(node_itr, sxcscript_kind_push_varaddr, *token_itr, 0);
        *token_itr += 1;
    } else if (sxcscript_token_eq_str(*token_itr, "*")) {
        *token_itr += 1;
        sxcscript_parse_postfix(token_itr, node_itr, label, label_size, label_break, label_continue);
        sxcscript_parse_push(node_itr, sxcscript_kind_global_get, NULL, 0);
    } else {
        sxcscript_parse_postfix(token_itr, node_itr, label, label_size, label_break, label_continue);
    }
}
void sxcscript_parse_mul(struct sxcscript_token** token_itr, struct sxcscript_node** node_itr, struct sxcscript_label* label, int* label_size, int label_break, int label_continue) {
    sxcscript_parse_unary(token_itr, node_itr, label, label_size, label_break, label_continue);
    while (1) {
        if (sxcscript_token_eq_str(*token_itr, "*")) {
            *token_itr += 1;
            sxcscript_parse_unary(token_itr, node_itr, label, label_size, label_break, label_continue);
            sxcscript_parse_push(node_itr, sxcscript_kind_mul, NULL, 0);
        } else if (sxcscript_token_eq_str(*token_itr, "/")) {
            *token_itr += 1;
            sxcscript_parse_unary(token_itr, node_itr, label, label_size, label_break, label_continue);
            sxcscript_parse_push(node_itr, sxcscript_kind_div, NULL, 0);
        } else if (sxcscript_token_eq_str(*token_itr, "%")) {
            *token_itr += 1;
            sxcscript_parse_unary(token_itr, node_itr, label, label_size, label_break, label_continue);
            sxcscript_parse_push(node_itr, sxcscript_kind_mod, NULL, 0);
        } else {
            break;
        }
    }
}
void sxcscript_parse_add(struct sxcscript_token** token_itr, struct sxcscript_node** node_itr, struct sxcscript_label* label, int* label_size, int label_break, int label_continue) {
    sxcscript_parse_mul(token_itr, node_itr, label, label_size, label_break, label_continue);
    while (1) {
        if (sxcscript_token_eq_str(*token_itr, "+")) {
            *token_itr += 1;
            sxcscript_parse_mul(token_itr, node_itr, label, label_size, label_break, label_continue);
            sxcscript_parse_push(node_itr, sxcscript_kind_add, NULL, 0);
        } else if (sxcscript_token_eq_str(*token_itr, "-")) {
            *token_itr += 1;
            sxcscript_parse_mul(token_itr, node_itr, label, label_size, label_break, label_continue);
            sxcscript_parse_push(node_itr, sxcscript_kind_sub, NULL, 0);
        } else {
            break;
        }
    }
}
void sxcscript_parse_rel(struct sxcscript_token** token_itr, struct sxcscript_node** node_itr, struct sxcscript_label* label, int* label_size, int label_break, int label_continue) {
    sxcscript_parse_add(token_itr, node_itr, label, label_size, label_break, label_continue);
    while (1) {
        if (sxcscript_token_eq_str(*token_itr, "<")) {
            *token_itr += 1;
            sxcscript_parse_add(token_itr, node_itr, label, label_size, label_break, label_continue);
            sxcscript_parse_push(node_itr, sxcscript_kind_lt, NULL, 0);
        } else if (sxcscript_token_eq_str(*token_itr, ">")) {
            *token_itr += 1;
            sxcscript_parse_add(token_itr, node_itr, label, label_size, label_break, label_continue);
            sxcscript_parse_push(node_itr, sxcscript_kind_gt, NULL, 0);
        } else {
            break;
        }
    }
}
void sxcscript_parse_eq(struct sxcscript_token** token_itr, struct sxcscript_node** node_itr, struct sxcscript_label* label, int* label_size, int label_break, int label_continue) {
    sxcscript_parse_rel(token_itr, node_itr, label, label_size, label_break, label_continue);
    while (1) {
        if (sxcscript_token_eq_str(*token_itr, "==")) {
            *token_itr += 1;
            sxcscript_parse_rel(token_itr, node_itr, label, label_size, label_break, label_continue);
            sxcscript_parse_push(node_itr, sxcscript_kind_eq, NULL, 0);
        } else if (sxcscript_token_eq_str(*token_itr, "!=")) {
            *token_itr += 1;
            sxcscript_parse_rel(token_itr, node_itr, label, label_size, label_break, label_continue);
            sxcscript_parse_push(node_itr, sxcscript_kind_ne, NULL, 0);
        } else {
            break;
        }
    }
}
void sxcscript_parse_and(struct sxcscript_token** token_itr, struct sxcscript_node** node_itr, struct sxcscript_label* label, int* label_size, int label_break, int label_continue) {
    sxcscript_parse_eq(token_itr, node_itr, label, label_size, label_break, label_continue);
    while (sxcscript_token_eq_str(*token_itr, "&") && sxcscript_token_eq_str(*token_itr + 1, "&")) {
        *token_itr += 2;
        sxcscript_parse_eq(token_itr, node_itr, label, label_size, label_break, label_continue);
        sxcscript_parse_push(node_itr, sxcscript_kind_and, NULL, 0);
    }
}
void sxcscript_parse_or(struct sxcscript_token** token_itr, struct sxcscript_node** node_itr, struct sxcscript_label* label, int* label_size, int label_break, int label_continue) {
    sxcscript_parse_and(token_itr, node_itr, label, label_size, label_break, label_continue);
    while (sxcscript_token_eq_str(*token_itr, "||")) {
        *token_itr += 1;
        sxcscript_parse_and(token_itr, node_itr, label, label_size, label_break, label_continue);
        sxcscript_parse_push(node_itr, sxcscript_kind_or, NULL, 0);
    }
}
void sxcscript_parse_assign(struct sxcscript_token** token_itr, struct sxcscript_node** node_itr, struct sxcscript_label* label, int* label_size, int label_break, int label_continue) {
    sxcscript_parse_or(token_itr, node_itr, label, label_size, label_break, label_continue);
    while (sxcscript_token_eq_str(*token_itr, "=")) {
        *token_itr += 1;
        sxcscript_parse_or(token_itr, node_itr, label, label_size, label_break, label_continue);
        sxcscript_parse_push(node_itr, sxcscript_kind_global_set, NULL, 0);
    }
}
void sxcscript_parse_expression(struct sxcscript_token** token_itr, struct sxcscript_node** node_itr, struct sxcscript_label* label, int* label_size, int label_break, int label_continue) {
    if (sxcscript_token_eq_str(*token_itr, "if")) {
        int label_if = (*label_size)++;
        int label_else = (*label_size)++;
        *token_itr += 1;
        sxcscript_parse_expression(token_itr, node_itr, label, label_size, label_break, label_continue);
        sxcscript_parse_push(node_itr, sxcscript_kind_jze, NULL, label_if);
        sxcscript_parse_expression(token_itr, node_itr, label, label_size, label_break, label_continue);
        if (sxcscript_token_eq_str(*token_itr, "else")) {
            *token_itr += 1;
            sxcscript_parse_push(node_itr, sxcscript_kind_jmp, NULL, label_else);
            sxcscript_parse_push(node_itr, sxcscript_kind_label, NULL, label_if);
            sxcscript_parse_expression(token_itr, node_itr, label, label_size, label_break, label_continue);
            sxcscript_parse_push(node_itr, sxcscript_kind_label, NULL, label_else);
        } else {
            sxcscript_parse_push(node_itr, sxcscript_kind_label, NULL, label_if);
        }
    } else if (sxcscript_token_eq_str(*token_itr, "loop")) {
        int label_start = (*label_size)++;
        int label_end = (*label_size)++;
        *token_itr += 1;
        sxcscript_parse_push(node_itr, sxcscript_kind_label, NULL, label_start);
        sxcscript_parse_expression(token_itr, node_itr, label, label_size, label_end, label_start);
        sxcscript_parse_push(node_itr, sxcscript_kind_jmp, NULL, label_start);
        sxcscript_parse_push(node_itr, sxcscript_kind_label, NULL, label_end);
    } else if (sxcscript_token_eq_str(*token_itr, "break")) {
        *token_itr += 1;
        sxcscript_parse_push(node_itr, sxcscript_kind_jmp, NULL, label_break);
    } else if (sxcscript_token_eq_str(*token_itr, "continue")) {
        *token_itr += 1;
        sxcscript_parse_push(node_itr, sxcscript_kind_jmp, NULL, label_continue);
    } else if (sxcscript_token_eq_str(*token_itr, "fn")) {
        int label_fn = (*label_size)++;
        int arg_size = 0;
        *token_itr += 1;
        label[label_fn].token = *token_itr;
        *token_itr += 2;
        while (!sxcscript_token_eq_str(*token_itr, ")")) {
            sxcscript_parse_push(node_itr, sxcscript_kind_push_varaddr, *token_itr, 0);
            *token_itr += 1;
            arg_size++;
            if (sxcscript_token_eq_str(*token_itr, ",")) {
                *token_itr += 1;
            }
        }
        struct sxcscript_node* arg_itr = *node_itr - 1;
        for (int i = 0; i < arg_size; i++) {
            arg_itr->val = -4 - i;
            arg_itr--;
        }
        *token_itr += 1;
        sxcscript_parse_push(node_itr, sxcscript_kind_label, NULL, label_fn);
        sxcscript_parse_push(node_itr, sxcscript_kind_push_varaddr, NULL, -2);
        sxcscript_parse_push(node_itr, sxcscript_kind_push_varaddr, NULL, -2);
        sxcscript_parse_push(node_itr, sxcscript_kind_global_get, NULL, 0);
        sxcscript_parse_push(node_itr, sxcscript_kind_push_const, NULL, arg_size);
        sxcscript_parse_push(node_itr, sxcscript_kind_sub, NULL, 0);
        sxcscript_parse_push(node_itr, sxcscript_kind_global_set, NULL, 0);
        sxcscript_parse_expression(token_itr, node_itr, label, label_size, label_break, label_continue);
        sxcscript_parse_push(node_itr, sxcscript_kind_return, NULL, 0);
        sxcscript_parse_push(node_itr, sxcscript_kind_label_fnend, NULL, 0);
    } else {
        sxcscript_parse_assign(token_itr, node_itr, label, label_size, label_break, label_continue);
    }
}
void sxcscript_parse(struct sxcscript_token* token, struct sxcscript_node* node, struct sxcscript_label* label, int* label_size) {
    struct sxcscript_token* token_itr = token;
    struct sxcscript_node* node_itr = node;
    while (token_itr->data != NULL) {
        sxcscript_parse_expression(&token_itr, &node_itr, label, label_size, -1, -1);
    }
}
void sxcscript_analyze_push(struct sxcscript_node* node, struct sxcscript_token** local_token, int* local_offset) {
    int offset_size = 0;
    int local_size = 0;
    for (struct sxcscript_node* node_itr = node; node_itr->kind != sxcscript_kind_null; node_itr++) {
        if (node_itr->kind == sxcscript_kind_label_fnend) {
            offset_size = 0;
            local_size = 0;
            continue;
        }
        if (node_itr->token == NULL) {
            continue;
        }
        if (node_itr->kind == sxcscript_kind_push_const) {
            node_itr->val = sxcscript_token_to_int32(node_itr->token);
        } else if (node_itr->kind == sxcscript_kind_push_varaddr) {
            for (int i = 0;; i++) {
                if (i == local_size) {
                    local_token[local_size] = node_itr->token;
                    if (node_itr->val != 0) {
                        local_offset[local_size++] = node_itr->val;
                    } else {
                        local_offset[local_size++] = offset_size;
                    }
                    node_itr->val = offset_size++;
                    break;
                }
                if (sxcscript_token_eq(local_token[i], node_itr->token)) {
                    node_itr->val = local_offset[i];
                    break;
                }
            }
        }
    }
}
int sxcscript_analyze_toinst_searchlabel(struct sxcscript_label* label, int label_size, struct sxcscript_node* node) {
    for (int i = 0; i < label_size; i++) {
        if (label[i].token == NULL) {
            continue;
        }
        if (sxcscript_token_eq(label[i].token, node->token)) {
            return i;
        }
    }
    return -1;
}
void sxcscript_analyze_toinst(union sxcscript_mem* mem, struct sxcscript_node* node, struct sxcscript_label* label, int* label_size) {
    union sxcscript_mem* inst_itr = mem + sxcscript_global_size;
    for (struct sxcscript_node* node_itr = node; node_itr->kind != sxcscript_kind_null; node_itr++) {
        if (node_itr->kind == sxcscript_kind_label) {
            label[node_itr->val].inst_i = inst_itr - mem;
        } else if (node_itr->kind == sxcscript_kind_push_const || node_itr->kind == sxcscript_kind_push_varaddr) {
            *(inst_itr++) = (union sxcscript_mem){.kind = node_itr->kind};
            *(inst_itr++) = (union sxcscript_mem){.val = node_itr->val};
        } else if (node_itr->kind == sxcscript_kind_jmp || node_itr->kind == sxcscript_kind_jze) {
            *(inst_itr++) = (union sxcscript_mem){.kind = node_itr->kind};
            *(inst_itr++) = (union sxcscript_mem){.val = node_itr->val};
        } else if (node_itr->kind == sxcscript_kind_call) {
            *(inst_itr++) = (union sxcscript_mem){.kind = node_itr->kind};
            *(inst_itr++) = (union sxcscript_mem){.val = sxcscript_analyze_toinst_searchlabel(label, *label_size, node_itr)};
        } else if (node_itr->kind == sxcscript_kind_nop) {
            continue;
        } else {
            *(inst_itr++) = (union sxcscript_mem){.kind = node_itr->kind};
        }
    }
    mem[sxcscript_global_ip].val = sxcscript_global_size;
    mem[sxcscript_global_bp].val = inst_itr - mem;
    mem[sxcscript_global_sp].val = inst_itr - mem + sxcscript_stack_size;
}
void sxcscript_analyze(union sxcscript_mem* mem, struct sxcscript_node* node, struct sxcscript_token** local_token, int* local_offset, struct sxcscript_label* label, int* label_size) {
    sxcscript_analyze_push(node, local_token, local_offset);
    sxcscript_analyze_toinst(mem, node, label, label_size);
}
void sxcscript_link(union sxcscript_mem* mem, struct sxcscript_label* label) {
    for (union sxcscript_mem* inst_itr = mem + sxcscript_global_size; inst_itr->kind != sxcscript_kind_null; inst_itr++) {
        if (inst_itr->kind == sxcscript_kind_jmp || inst_itr->kind == sxcscript_kind_jze || inst_itr->kind == sxcscript_kind_call) {
            inst_itr += 1;
            inst_itr->val = label[inst_itr->val].inst_i;
        } else if (inst_itr->kind == sxcscript_kind_push_const || inst_itr->kind == sxcscript_kind_push_varaddr) {
            inst_itr += 1;
        }
    }
}
void sxcscript_out_push(char* buf, int* buf_size, char ch) {
    buf[(*buf_size)++] = ch;
}
void sxcscript_out(union sxcscript_mem* mem, char* buf) {
    int fd = open("Scratch.txt", (O_WRONLY | O_CREAT | O_TRUNC));
    int buf_size = 0;
    for (int i = 1; i < 200000; i++) {
        int x = mem[i].val;
        int m = 1000000000;
        if (x == 0) {
            sxcscript_out_push(buf, &buf_size, '0');
            sxcscript_out_push(buf, &buf_size, '\n');
            continue;
        }
        if (x < 0) {
            sxcscript_out_push(buf, &buf_size, '-');
            x = -x;
        }
        while (x / m == 0) {
            m /= 10;
        }
        while (m != 0) {
            char ch = x / m % 10 + 48;
            sxcscript_out_push(buf, &buf_size, ch);
            m /= 10;
        }
        sxcscript_out_push(buf, &buf_size, '\n');
    }
    write(fd, buf, buf_size);
    close(fd);
}
void sxcscript_run_ext(union sxcscript_mem* mem) {
    int kind = mem[mem[sxcscript_global_sp].val - 1].val;
    int a1;
    int a2;
    int a3;
    mem[sxcscript_global_sp].val -= 1;
    switch (kind) {
        case 6:
            a1 = mem[mem[sxcscript_global_sp].val - 1].val;
            mem[mem[sxcscript_global_sp].val - 1].val = write(1, &a1, 1);
            break;
        default:
            break;
    }
}
void sxcscript_run(union sxcscript_mem* mem) {
    int result;
    while (mem[mem[sxcscript_global_ip].val].kind != sxcscript_kind_null) {
        switch (mem[mem[sxcscript_global_ip].val].kind) {
            case sxcscript_kind_null:
                break;
            case sxcscript_kind_nop:
                break;
            case sxcscript_kind_push_const:
                (mem[sxcscript_global_ip].val)++;
                mem[(mem[sxcscript_global_sp].val)++].val = mem[mem[sxcscript_global_ip].val].val;
                break;
            case sxcscript_kind_push_varaddr:
                (mem[sxcscript_global_ip].val)++;
                mem[(mem[sxcscript_global_sp].val)++].val = mem[sxcscript_global_bp].val + mem[mem[sxcscript_global_ip].val].val;
                break;
            case sxcscript_kind_test01:
                break;
            case sxcscript_kind_test02:
                break;
            case sxcscript_kind_test03:
                break;
            case sxcscript_kind_global_get:
                mem[mem[sxcscript_global_sp].val - 1].val = mem[mem[mem[sxcscript_global_sp].val - 1].val].val;
                break;
            case sxcscript_kind_global_set:
                mem[mem[mem[sxcscript_global_sp].val - 2].val].val = mem[mem[sxcscript_global_sp].val - 1].val;
                mem[sxcscript_global_sp].val -= 2;
                break;
            case sxcscript_kind_call:
                mem[(mem[sxcscript_global_sp].val) + 0].val = mem[sxcscript_global_ip].val + 1;
                mem[(mem[sxcscript_global_sp].val) + 1].val = mem[sxcscript_global_sp].val;
                mem[(mem[sxcscript_global_sp].val) + 2].val = mem[sxcscript_global_bp].val;
                mem[sxcscript_global_ip].val = mem[mem[sxcscript_global_ip].val + 1].val - 1;
                mem[sxcscript_global_bp].val = mem[sxcscript_global_sp].val + 3;
                mem[sxcscript_global_sp].val += sxcscript_stack_size;
                break;
            case sxcscript_kind_return:
                result = mem[mem[sxcscript_global_sp].val - 1].val;
                mem[sxcscript_global_ip].val = mem[mem[sxcscript_global_bp].val - 3].val;
                mem[sxcscript_global_sp].val = mem[mem[sxcscript_global_bp].val - 2].val;
                mem[sxcscript_global_bp].val = mem[mem[sxcscript_global_bp].val - 1].val;
                mem[mem[sxcscript_global_sp].val].val = result;
                mem[sxcscript_global_sp].val++;
                break;
            case sxcscript_kind_jmp:
                mem[sxcscript_global_ip].val = mem[mem[sxcscript_global_ip].val + 1].val - 1;
                break;
            case sxcscript_kind_jze:
                if (mem[mem[sxcscript_global_sp].val - 1].val == 0) {
                    mem[sxcscript_global_ip].val = mem[mem[sxcscript_global_ip].val + 1].val - 1;
                } else {
                    mem[sxcscript_global_ip].val += 1;
                }
                mem[sxcscript_global_sp].val -= 1;
                break;
            case sxcscript_kind_or:
                mem[mem[sxcscript_global_sp].val - 2].val |= mem[mem[sxcscript_global_sp].val - 1].val;
                mem[sxcscript_global_sp].val -= 1;
                break;
            case sxcscript_kind_and:
                mem[mem[sxcscript_global_sp].val - 2].val &= mem[mem[sxcscript_global_sp].val - 1].val;
                mem[sxcscript_global_sp].val -= 1;
                break;
            case sxcscript_kind_eq:
                mem[mem[sxcscript_global_sp].val - 2].val = mem[mem[sxcscript_global_sp].val - 2].val == mem[mem[sxcscript_global_sp].val - 1].val;
                mem[sxcscript_global_sp].val -= 1;
                break;
            case sxcscript_kind_ne:
                mem[mem[sxcscript_global_sp].val - 2].val = mem[mem[sxcscript_global_sp].val - 2].val == mem[mem[sxcscript_global_sp].val - 1].val;
                mem[sxcscript_global_sp].val -= 1;
                break;
            case sxcscript_kind_lt:
                mem[mem[sxcscript_global_sp].val - 2].val = mem[mem[sxcscript_global_sp].val - 2].val < mem[mem[sxcscript_global_sp].val - 1].val;
                mem[sxcscript_global_sp].val -= 1;
                break;
            case sxcscript_kind_gt:
                mem[mem[sxcscript_global_sp].val - 2].val = mem[mem[sxcscript_global_sp].val - 2].val > mem[mem[sxcscript_global_sp].val - 1].val;
                mem[sxcscript_global_sp].val -= 1;
                break;
            case sxcscript_kind_add:
                mem[mem[sxcscript_global_sp].val - 2].val += mem[mem[sxcscript_global_sp].val - 1].val;
                mem[sxcscript_global_sp].val -= 1;
                break;
            case sxcscript_kind_sub:
                mem[mem[sxcscript_global_sp].val - 2].val -= mem[mem[sxcscript_global_sp].val - 1].val;
                mem[sxcscript_global_sp].val -= 1;
                break;
            case sxcscript_kind_mul:
                mem[mem[sxcscript_global_sp].val - 2].val *= mem[mem[sxcscript_global_sp].val - 1].val;
                mem[sxcscript_global_sp].val -= 1;
                break;
            case sxcscript_kind_div:
                mem[mem[sxcscript_global_sp].val - 2].val /= mem[mem[sxcscript_global_sp].val - 1].val;
                mem[sxcscript_global_sp].val -= 1;
                break;
            case sxcscript_kind_mod:
                mem[mem[sxcscript_global_sp].val - 2].val %= mem[mem[sxcscript_global_sp].val - 1].val;
                mem[sxcscript_global_sp].val -= 1;
                break;
            case sxcscript_kind_usleep:
                mem[mem[sxcscript_global_sp].val - 1].val = usleep(mem[mem[sxcscript_global_sp].val - 1].val);
                break;
            case sxcscript_kind_ext:
                sxcscript_run_ext(mem);
                break;
            default:
                break;
        }
        (mem[sxcscript_global_ip].val)++;
    }
}

void sxcscript_init(union sxcscript_mem* mem) {
    char src[sxcscript_compile_size];
    char buf[sxcscript_compile_size];
    struct sxcscript_token token[sxcscript_compile_size / sizeof(struct sxcscript_token)];
    struct sxcscript_node node[sxcscript_compile_size / sizeof(struct sxcscript_node)];
    struct sxcscript_label label[sxcscript_compile_size / sizeof(struct sxcscript_label)];
    struct sxcscript_token* local_token[sxcscript_compile_size / sizeof(struct sxcscript_token)];
    int local_offset[sxcscript_compile_size / sizeof(int)];
    int label_size = 0;
    sxcscript_readfile(src);
    sxcscript_tokenize(src, token);
    sxcscript_parse(token, node, label, &label_size);
    sxcscript_analyze(mem, node, local_token, local_offset, label, &label_size);
    sxcscript_link(mem, label);
    sxcscript_out(mem, buf);
}
void sxcscript() {
    static union sxcscript_mem mem[sxcscript_mem_size];
    sxcscript_init(mem);
    // sxcscript_run(mem);
}
void init() {
    struct rlimit rlim = (struct rlimit){.rlim_cur = stacksize, .rlim_max = stacksize};
    setrlimit(RLIMIT_STACK, &rlim);
}
int main() {
    init();
    sxcscript();
}
