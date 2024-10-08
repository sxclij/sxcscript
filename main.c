#include <fcntl.h>
#include <stdint.h>
#include <unistd.h>

#define sxcscript_path "test/01.txt"
#define sxcscript_mem_capacity (1 << 16)
#define sxcscript_compile_capacity (1 << 16)
#define sxcscript_buf_capacity (1 << 10)
#define sxcscript_global_capacity (1 << 8)

enum bool {
    false = 0,
    true = 1,
};
enum sxcscript_kind {
    sxcscript_kind_null,
    sxcscript_kind_nop,
    sxcscript_kind_push,
    sxcscript_kind_label,
    sxcscript_kind_call,
    sxcscript_kind_return,
    sxcscript_kind_jmp,
    sxcscript_kind_jze,
    sxcscript_kind_const_get,
    sxcscript_kind_const_set,
    sxcscript_kind_local_get,
    sxcscript_kind_local_set,
    sxcscript_kind_add,
    sxcscript_kind_sub,
    sxcscript_kind_mul,
    sxcscript_kind_div,
    sxcscript_kind_mod,
    sxcscript_kind_not,
    sxcscript_kind_and,
    sxcscript_kind_eq,
    sxcscript_kind_lt,
    sxcscript_kind_read,
    sxcscript_kind_write,
    sxcscript_kind_usleep,
};
enum sxcscript_global {
    sxcscript_global_ip,
    sxcscript_global_sp,
    sxcscript_global_bp,
};
struct sxcscript_token {
    const char* data;
    int32_t size;
};
union sxcscript_node_val {
    int32_t label_i;
    int32_t literal;
};
struct sxcscript_node {
    enum sxcscript_kind kind;
    struct sxcscript_token* token;
    union sxcscript_node_val val;
};
struct sxcscript_label {
    struct sxcscript_token* token;
    int32_t arg_size;
    int32_t inst_i;
};
union sxcscript_mem {
    enum sxcscript_kind kind;
    int32_t val;
};
struct sxcscript {
    union sxcscript_mem mem[sxcscript_mem_capacity];
    struct sxcscript_token token[sxcscript_compile_capacity];
    struct sxcscript_node node[sxcscript_compile_capacity];
    struct sxcscript_label label[sxcscript_compile_capacity];
    union sxcscript_mem* inst_begin;
    union sxcscript_mem* data_begin;
    int32_t label_size;
};

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
int32_t sxcscript_token_to_int32(struct sxcscript_token* token) {
    enum bool is_neg = token->data[0] == '-';
    int i = is_neg ? 1 : 0;
    int32_t ret = 0;
    for (; i < token->size; i++) {
        ret = (ret * 10) + token->data[i] - '0';
    }
    return is_neg ? -ret : ret;
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
void sxcscript_parse_push(struct sxcscript_node** node_itr, enum sxcscript_kind kind, struct sxcscript_token* token, union sxcscript_node_val val) {
    *((*node_itr)++) = (struct sxcscript_node){.kind = kind, .token = token, .val = val};
}
void sxcscript_parse_expr(struct sxcscript* sxcscript, struct sxcscript_node** node_itr, struct sxcscript_token** token_itr, int break_i, int continue_i) {
    struct sxcscript_token* token_this = *token_itr;
    struct sxcscript_node* node_this = *node_itr;
    if (sxcscript_token_eq_str(token_this, "(")) {
        (*token_itr)++;
        while (!sxcscript_token_eq_str(*token_itr, ")")) {
            sxcscript_parse_expr(sxcscript, node_itr, token_itr, break_i, continue_i);
            if (sxcscript_token_eq_str(*token_itr, ",")) {
                (*token_itr)++;
            }
        }
        (*token_itr)++;
    } else if (sxcscript_token_eq_str(token_this, ".")) {
        (*token_itr)++;
        sxcscript_parse_expr(sxcscript, node_itr, token_itr, break_i, continue_i);
    } else if (sxcscript_token_eq_str(token_this, "fn")) {
        int32_t fn_label = sxcscript->label_size++;
        int32_t arg_size = 0;
        (*token_itr)++;
        sxcscript->label[fn_label].token = *token_itr;
        *token_itr += 2;
        while (!sxcscript_token_eq_str(*token_itr, ")")) {
            sxcscript_parse_push(node_itr, sxcscript_kind_const_get, token_this, (union sxcscript_node_val){0});
            arg_size++;
            if (sxcscript_token_eq_str(*token_itr, ",")) {
                (*token_itr)++;
            }
        }
        (*token_itr)++;
        sxcscript_parse_push(node_itr, sxcscript_kind_jze, NULL, (union sxcscript_node_val){.label_i = fn_label});
        sxcscript_parse_expr(sxcscript, node_itr, token_itr, break_i, continue_i);
    } else if (sxcscript_token_eq_str(token_this, "if")) {
        int32_t if_label = sxcscript->label_size++;
        int32_t else_label = sxcscript->label_size++;
        (*token_itr)++;
        sxcscript_parse_expr(sxcscript, node_itr, token_itr, break_i, continue_i);
        sxcscript_parse_push(node_itr, sxcscript_kind_jze, NULL, (union sxcscript_node_val){.label_i = if_label});
        sxcscript_parse_expr(sxcscript, node_itr, token_itr, break_i, continue_i);
        if (sxcscript_token_eq_str(*token_itr, "else")) {
            (*token_itr)++;
            sxcscript_parse_push(node_itr, sxcscript_kind_jmp, NULL, (union sxcscript_node_val){.label_i = else_label});
            sxcscript_parse_push(node_itr, sxcscript_kind_label, NULL, (union sxcscript_node_val){.label_i = if_label});
            sxcscript_parse_expr(sxcscript, node_itr, token_itr, break_i, continue_i);
            sxcscript_parse_push(node_itr, sxcscript_kind_label, NULL, (union sxcscript_node_val){.label_i = else_label});
        } else {
            sxcscript_parse_push(node_itr, sxcscript_kind_label, NULL, (union sxcscript_node_val){.label_i = if_label});
        }
    } else if (sxcscript_token_eq_str(token_this, "loop")) {
        int32_t start_label = sxcscript->label_size++;
        int32_t end_label = sxcscript->label_size++;
        (*token_itr)++;
        sxcscript_parse_push(node_itr, sxcscript_kind_label, NULL, (union sxcscript_node_val){.label_i = start_label});
        sxcscript_parse_expr(sxcscript, node_itr, token_itr, end_label, start_label);
        sxcscript_parse_push(node_itr, sxcscript_kind_jmp, NULL, (union sxcscript_node_val){.label_i = start_label});
        sxcscript_parse_push(node_itr, sxcscript_kind_label, NULL, (union sxcscript_node_val){.label_i = end_label});
    } else if (sxcscript_token_eq_str(token_this, "break")) {
        (*token_itr)++;
        sxcscript_parse_push(node_itr, sxcscript_kind_jmp, NULL, (union sxcscript_node_val){.label_i = break_i});
    } else if (sxcscript_token_eq_str(token_this, "continue")) {
        (*token_itr)++;
        sxcscript_parse_push(node_itr, sxcscript_kind_jmp, NULL, (union sxcscript_node_val){.label_i = continue_i});
    } else if (sxcscript_token_eq_str(*token_itr + 1, "(")) {
        (*token_itr)++;
        sxcscript_parse_expr(sxcscript, node_itr, token_itr, break_i, continue_i);
        sxcscript_parse_push(node_itr, sxcscript_kind_call, token_this, (union sxcscript_node_val){0});
    } else {
        sxcscript_parse_push(node_itr, sxcscript_kind_const_get, token_this, (union sxcscript_node_val){0});
        (*token_itr)++;
    }
}
void sxcscript_parse(struct sxcscript* sxcscript) {
    struct sxcscript_token* token_itr = sxcscript->token;
    struct sxcscript_node* node_itr = sxcscript->node;
    while (token_itr->size != 0) {
        sxcscript_parse_expr(sxcscript, &node_itr, &token_itr, -1, -1);
    }
}
void sxcscript_analyze_primitive(struct sxcscript_node* node) {
    for (struct sxcscript_node* node_itr = node; node_itr->kind != sxcscript_kind_null; node_itr++) {
        if (node_itr->token == NULL) {
            continue;
        } else if (sxcscript_token_eq_str(node_itr->token, "const_get")) {
            node_itr->kind = sxcscript_kind_const_get;
        } else if (sxcscript_token_eq_str(node_itr->token, "const_set")) {
            node_itr->kind = sxcscript_kind_const_set;
        } else if (sxcscript_token_eq_str(node_itr->token, "local_get")) {
            node_itr->kind = sxcscript_kind_local_get;
        } else if (sxcscript_token_eq_str(node_itr->token, "local_set")) {
            node_itr->kind = sxcscript_kind_local_set;
        } else if (sxcscript_token_eq_str(node_itr->token, "add")) {
            node_itr->kind = sxcscript_kind_add;
        } else if (sxcscript_token_eq_str(node_itr->token, "sub")) {
            node_itr->kind = sxcscript_kind_sub;
        } else if (sxcscript_token_eq_str(node_itr->token, "mul")) {
            node_itr->kind = sxcscript_kind_mul;
        } else if (sxcscript_token_eq_str(node_itr->token, "div")) {
            node_itr->kind = sxcscript_kind_div;
        } else if (sxcscript_token_eq_str(node_itr->token, "mod")) {
            node_itr->kind = sxcscript_kind_mod;
        } else if (sxcscript_token_eq_str(node_itr->token, "not")) {
            node_itr->kind = sxcscript_kind_not;
        } else if (sxcscript_token_eq_str(node_itr->token, "and")) {
            node_itr->kind = sxcscript_kind_and;
        } else if (sxcscript_token_eq_str(node_itr->token, "eq")) {
            node_itr->kind = sxcscript_kind_eq;
        } else if (sxcscript_token_eq_str(node_itr->token, "lt")) {
            node_itr->kind = sxcscript_kind_lt;
        } else if (sxcscript_token_eq_str(node_itr->token, "read")) {
            node_itr->kind = sxcscript_kind_read;
        } else if (sxcscript_token_eq_str(node_itr->token, "write")) {
            node_itr->kind = sxcscript_kind_write;
        } else if (sxcscript_token_eq_str(node_itr->token, "usleep")) {
            node_itr->kind = sxcscript_kind_usleep;
        }
    }
}
void sxcscript_analyze_var(struct sxcscript_node* node) {
    struct sxcscript_token* local_token[sxcscript_buf_capacity];
    int32_t local_offset[sxcscript_buf_capacity];
    int32_t offset_size = 0;
    int32_t local_size = 0;
    for (struct sxcscript_node* node_itr = node; node_itr->kind != sxcscript_kind_null; node_itr++) {
        if (node_itr->kind == sxcscript_kind_const_get) {
            if ('0' <= node_itr->token->data[0] && node_itr->token->data[0] <= '9' || node_itr->token->data[0] == '-') {
                node_itr->val.literal = sxcscript_token_to_int32(node_itr->token);
            } else {
                for (int32_t i = 0;; i++) {
                    if (i == local_size) {
                        local_token[local_size] = node_itr->token;
                        local_offset[local_size++] = offset_size;
                        node_itr->val.literal = offset_size++;
                        break;
                    }
                    if (sxcscript_token_eq(local_token[i], node_itr->token)) {
                        node_itr->val.literal = local_offset[i];
                        break;
                    }
                }
            }
        }
    }
}
void sxcscript_analyze_toinst(struct sxcscript* sxcscript, struct sxcscript_node* node) {
    union sxcscript_mem* inst_itr = sxcscript->inst_begin;
    for (struct sxcscript_node* node_itr = node; node_itr->kind == sxcscript_kind_null; node_itr++) {
        if (node_itr->kind == sxcscript_kind_label) {
            sxcscript->label[node_itr->val.label_i].inst_i = inst_itr - sxcscript->mem;
        } else if (node_itr->kind == sxcscript_kind_const_get) {
            *(inst_itr++) = (union sxcscript_mem){.kind = node_itr->kind};
            *(inst_itr++) = (union sxcscript_mem){.val = node_itr->val.literal};
        } else if (node_itr->kind == sxcscript_kind_jmp || node_itr->kind == sxcscript_kind_jze) {
            *(inst_itr++) = (union sxcscript_mem){.kind = node_itr->kind};
            *(inst_itr++) = (union sxcscript_mem){.val = node_itr->val.label_i};
        } else {
            *(inst_itr++) = (union sxcscript_mem){.kind = node_itr->kind};
        }
    }
    sxcscript->data_begin = inst_itr;
}
void sxcscript_analyze(struct sxcscript* sxcscript) {
    sxcscript_analyze_primitive(sxcscript->node);
    sxcscript_analyze_var(sxcscript->node);
    sxcscript_analyze_toinst(sxcscript, sxcscript->node);
}
void sxcscript_link(struct sxcscript* sxcscript) {
    for (union sxcscript_mem* inst_itr = sxcscript->inst_begin; inst_itr->kind != sxcscript_kind_null; inst_itr++) {
        if (inst_itr->kind == sxcscript_kind_jmp || inst_itr->kind == sxcscript_kind_jze) {
            inst_itr += 1;
            inst_itr->val = sxcscript->label[inst_itr->val].inst_i;
        } else if (inst_itr->kind == sxcscript_kind_const_get) {
            inst_itr += 1;
        }
    }
}
void sxcscript_init(struct sxcscript* sxcscript, const char* src) {
    sxcscript->inst_begin = sxcscript->mem + sxcscript_global_capacity;
    sxcscript->label_size = 0;
    sxcscript_tokenize(src, sxcscript->token);
    sxcscript_parse(sxcscript);
    sxcscript_analyze(sxcscript);
    sxcscript_link(sxcscript);
    sxcscript->mem[sxcscript_global_ip].val = sxcscript->inst_begin - sxcscript->mem;
    sxcscript->mem[sxcscript_global_sp].val = sxcscript->data_begin - sxcscript->mem;
    sxcscript->mem[sxcscript_global_bp].val = sxcscript->data_begin - sxcscript->mem + 256;
}
void sxcscript_exec(struct sxcscript* sxcscript) {
    while (sxcscript->mem[sxcscript->mem[sxcscript_global_ip].val].kind != sxcscript_kind_null) {
        switch (sxcscript->mem[sxcscript->mem[sxcscript_global_ip].val].kind) {
            case sxcscript_kind_const_get:
                (sxcscript->mem[sxcscript_global_ip].val)++;
                sxcscript->mem[(sxcscript->mem[sxcscript_global_sp].val)++].val = sxcscript->mem[sxcscript->mem[sxcscript_global_ip].val].val;
                break;
            case sxcscript_kind_const_set:
                sxcscript->mem[sxcscript->mem[sxcscript->mem[sxcscript_global_sp].val - 2].val].val = sxcscript->mem[sxcscript->mem[sxcscript_global_sp].val - 1].val;
                sxcscript->mem[sxcscript_global_sp].val -= 2;
                break;
            case sxcscript_kind_local_get:
                sxcscript->mem[(sxcscript->mem[sxcscript_global_sp].val - 1)].val = sxcscript->mem[sxcscript->mem[sxcscript_global_bp].val + sxcscript->mem[sxcscript->mem[sxcscript_global_sp].val - 1].val].val;
                break;
            case sxcscript_kind_local_set:
                sxcscript->mem[sxcscript->mem[sxcscript_global_bp].val + sxcscript->mem[sxcscript->mem[sxcscript_global_sp].val - 2].val].val = sxcscript->mem[sxcscript->mem[sxcscript_global_sp].val - 1].val;
                sxcscript->mem[sxcscript_global_sp].val -= 2;
                break;
            case sxcscript_kind_add:
                sxcscript->mem[sxcscript->mem[sxcscript_global_sp].val - 2].val += sxcscript->mem[sxcscript->mem[sxcscript_global_sp].val - 1].val;
                sxcscript->mem[sxcscript_global_sp].val -= 1;
                break;
            case sxcscript_kind_sub:
                sxcscript->mem[sxcscript->mem[sxcscript_global_sp].val - 2].val -= sxcscript->mem[sxcscript->mem[sxcscript_global_sp].val - 1].val;
                sxcscript->mem[sxcscript_global_sp].val -= 1;
                break;
            case sxcscript_kind_mul:
                sxcscript->mem[sxcscript->mem[sxcscript_global_sp].val - 2].val *= sxcscript->mem[sxcscript->mem[sxcscript_global_sp].val - 1].val;
                sxcscript->mem[sxcscript_global_sp].val -= 1;
                break;
            case sxcscript_kind_div:
                sxcscript->mem[sxcscript->mem[sxcscript_global_sp].val - 2].val /= sxcscript->mem[sxcscript->mem[sxcscript_global_sp].val - 1].val;
                sxcscript->mem[sxcscript_global_sp].val -= 1;
                break;
            case sxcscript_kind_mod:
                sxcscript->mem[sxcscript->mem[sxcscript_global_sp].val - 2].val %= sxcscript->mem[sxcscript->mem[sxcscript_global_sp].val - 1].val;
                sxcscript->mem[sxcscript_global_sp].val -= 1;
                break;
            case sxcscript_kind_not:
                sxcscript->mem[sxcscript->mem[sxcscript_global_sp].val - 1].val = !sxcscript->mem[sxcscript->mem[sxcscript_global_sp].val - 1].val;
                break;
            case sxcscript_kind_and:
                sxcscript->mem[sxcscript->mem[sxcscript_global_sp].val - 2].val &= sxcscript->mem[sxcscript->mem[sxcscript_global_sp].val - 1].val;
                sxcscript->mem[sxcscript_global_sp].val -= 1;
                break;
            case sxcscript_kind_eq:
                sxcscript->mem[sxcscript->mem[sxcscript_global_sp].val - 2].val = sxcscript->mem[sxcscript->mem[sxcscript_global_sp].val - 2].val == sxcscript->mem[sxcscript->mem[sxcscript_global_sp].val - 1].val;
                sxcscript->mem[sxcscript_global_sp].val -= 1;
                break;
            case sxcscript_kind_lt:
                sxcscript->mem[sxcscript->mem[sxcscript_global_sp].val - 2].val = sxcscript->mem[sxcscript->mem[sxcscript_global_sp].val - 2].val < sxcscript->mem[sxcscript->mem[sxcscript_global_sp].val - 1].val;
                sxcscript->mem[sxcscript_global_sp].val -= 1;
                break;
            case sxcscript_kind_jmp:
                sxcscript->mem[sxcscript_global_ip].val = sxcscript->mem[sxcscript->mem[sxcscript_global_ip].val + 1].val - 1;
                break;
            case sxcscript_kind_jze:
                if (sxcscript->mem[sxcscript->mem[sxcscript_global_sp].val - 1].val == 0) {
                    sxcscript->mem[sxcscript_global_ip].val = sxcscript->mem[sxcscript->mem[sxcscript_global_ip].val + 1].val - 1;
                }
                sxcscript->mem[sxcscript_global_sp].val -= 1;
                break;
            case sxcscript_kind_write:
                write(STDOUT_FILENO, &sxcscript->mem[sxcscript->mem[sxcscript_global_sp].val - 1].val, 1);
                sxcscript->mem[sxcscript_global_sp].val -= 1;
                break;
            case sxcscript_kind_usleep:
                usleep(sxcscript->mem[sxcscript->mem[sxcscript_global_sp].val - 1].val);
                sxcscript->mem[sxcscript_global_sp].val -= 1;
                break;
        }
        (sxcscript->mem[sxcscript_global_ip].val)++;
    }
}
int main() {
    char src[sxcscript_compile_capacity];
    static struct sxcscript sxcscript;

    int fd = open(sxcscript_path, O_RDONLY);
    int src_n = read(fd, src, sizeof(src) - 1);
    src[src_n] = '\0';
    close(fd);

    sxcscript_init(&sxcscript, src);

    sxcscript_exec(&sxcscript);

    return 0;
}