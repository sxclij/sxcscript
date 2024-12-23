#include <fcntl.h>
#include <sys/resource.h>
#include <unistd.h>

#define stacksize (128 * 1014 * 1024)
#define sxcscript_path "test/05.txt"
#define sxcscript_mem_size (1 << 20)
#define sxcscript_compile_size (1 << 20)
#define sxcscript_global_size (1 << 8)
#define sxcscript_stack_size (1 << 8)

enum bool {
    false = 0,
    true = 1,
};
enum sxcscript_kind {
    sxcscript_kind_null,
    sxcscript_kind_nop,
    sxcscript_kind_push,
    sxcscript_kind_label,
    sxcscript_kind_label_fnend,
    sxcscript_kind_call,
    sxcscript_kind_return,
    sxcscript_kind_jmp,
    sxcscript_kind_jze,
    sxcscript_kind_const_get,
    sxcscript_kind_local_get,
    sxcscript_kind_local_set,
    sxcscript_kind_global_get,
    sxcscript_kind_global_set,
    sxcscript_kind_addr,
    sxcscript_kind_add,
    sxcscript_kind_sub,
    sxcscript_kind_mul,
    sxcscript_kind_div,
    sxcscript_kind_mod,
    sxcscript_kind_not,
    sxcscript_kind_and,
    sxcscript_kind_eq,
    sxcscript_kind_lt,
    sxcscript_kind_open,
    sxcscript_kind_close,
    sxcscript_kind_read,
    sxcscript_kind_write,
    sxcscript_kind_usleep,
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
union sxcscript_node_val {
    int label_i;
    int literal;
};
struct sxcscript_node {
    enum sxcscript_kind kind;
    struct sxcscript_token* token;
    union sxcscript_node_val val;
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
    *token_itr = (struct sxcscript_token){NULL, 0};
}
void sxcscript_parse_push(struct sxcscript_node** node_itr, enum sxcscript_kind kind, struct sxcscript_token* token, union sxcscript_node_val val) {
    *((*node_itr)++) = (struct sxcscript_node){.kind = kind, .token = token, .val = val};
}
void sxcscript_parse_expr(struct sxcscript_label* label, struct sxcscript_node** node_itr, struct sxcscript_token** token_itr, int* label_size, int break_i, int continue_i) {
    struct sxcscript_token* token_this = *token_itr;
    if (sxcscript_token_eq_str(token_this, "(")) {
        (*token_itr)++;
        while (!sxcscript_token_eq_str(*token_itr, ")")) {
            sxcscript_parse_expr(label, node_itr, token_itr, label_size, break_i, continue_i);
            if (sxcscript_token_eq_str(*token_itr, ",")) {
                (*token_itr)++;
            }
        }
        (*token_itr)++;
    } else if (sxcscript_token_eq_str(token_this, ".")) {
        (*token_itr)++;
        sxcscript_parse_expr(label, node_itr, token_itr, label_size, break_i, continue_i);
    } else if (sxcscript_token_eq_str(token_this, "fn")) {
        int fn_label = (*label_size)++;
        int arg_size = 0;
        (*token_itr)++;
        label[fn_label].token = *token_itr;
        *token_itr += 2;
        while (!sxcscript_token_eq_str(*token_itr, ")")) {
            sxcscript_parse_push(node_itr, sxcscript_kind_const_get, *token_itr, (union sxcscript_node_val){0});
            (*token_itr)++;
            arg_size++;
            if (sxcscript_token_eq_str(*token_itr, ",")) {
                (*token_itr)++;
            }
        }
        struct sxcscript_node* arg_itr = (*node_itr) - 1;
        for (int i = 0; i < arg_size; i++) {
            arg_itr->val.literal = -4 - i;
            arg_itr--;
        }
        (*token_itr)++;
        sxcscript_parse_push(node_itr, sxcscript_kind_label, NULL, (union sxcscript_node_val){.label_i = fn_label});
        sxcscript_parse_push(node_itr, sxcscript_kind_const_get, NULL, (union sxcscript_node_val){.literal = -2});
        sxcscript_parse_push(node_itr, sxcscript_kind_const_get, NULL, (union sxcscript_node_val){.literal = -2});
        sxcscript_parse_push(node_itr, sxcscript_kind_local_get, NULL, (union sxcscript_node_val){0});
        sxcscript_parse_push(node_itr, sxcscript_kind_const_get, NULL, (union sxcscript_node_val){.literal = 0 + arg_size});
        sxcscript_parse_push(node_itr, sxcscript_kind_sub, NULL, (union sxcscript_node_val){0});
        sxcscript_parse_push(node_itr, sxcscript_kind_local_set, NULL, (union sxcscript_node_val){0});
        sxcscript_parse_expr(label, node_itr, token_itr, label_size, break_i, continue_i);
        sxcscript_parse_push(node_itr, sxcscript_kind_return, NULL, (union sxcscript_node_val){0});
        sxcscript_parse_push(node_itr, sxcscript_kind_label_fnend, NULL, (union sxcscript_node_val){0});
    } else if (sxcscript_token_eq_str(token_this, "if")) {
        int if_label = (*label_size)++;
        int else_label = (*label_size)++;
        (*token_itr)++;
        sxcscript_parse_expr(label, node_itr, token_itr, label_size, break_i, continue_i);
        sxcscript_parse_push(node_itr, sxcscript_kind_jze, NULL, (union sxcscript_node_val){.label_i = if_label});
        sxcscript_parse_expr(label, node_itr, token_itr, label_size, break_i, continue_i);
        if (sxcscript_token_eq_str(*token_itr, "else")) {
            (*token_itr)++;
            sxcscript_parse_push(node_itr, sxcscript_kind_jmp, NULL, (union sxcscript_node_val){.label_i = else_label});
            sxcscript_parse_push(node_itr, sxcscript_kind_label, NULL, (union sxcscript_node_val){.label_i = if_label});
            sxcscript_parse_expr(label, node_itr, token_itr, label_size, break_i, continue_i);
            sxcscript_parse_push(node_itr, sxcscript_kind_label, NULL, (union sxcscript_node_val){.label_i = else_label});
        } else {
            sxcscript_parse_push(node_itr, sxcscript_kind_label, NULL, (union sxcscript_node_val){.label_i = if_label});
        }
    } else if (sxcscript_token_eq_str(token_this, "loop")) {
        int start_label = (*label_size)++;
        int end_label = (*label_size)++;
        (*token_itr)++;
        sxcscript_parse_push(node_itr, sxcscript_kind_label, NULL, (union sxcscript_node_val){.label_i = start_label});
        sxcscript_parse_expr(label, node_itr, token_itr, label_size, end_label, start_label);
        sxcscript_parse_push(node_itr, sxcscript_kind_jmp, NULL, (union sxcscript_node_val){.label_i = start_label});
        sxcscript_parse_push(node_itr, sxcscript_kind_label, NULL, (union sxcscript_node_val){.label_i = end_label});
    } else if (sxcscript_token_eq_str(token_this, "return")) {
        (*token_itr)++;
        sxcscript_parse_expr(label, node_itr, token_itr, label_size, break_i, continue_i);
        sxcscript_parse_push(node_itr, sxcscript_kind_return, NULL, (union sxcscript_node_val){0});
    } else if (sxcscript_token_eq_str(token_this, "break")) {
        (*token_itr)++;
        sxcscript_parse_push(node_itr, sxcscript_kind_jmp, NULL, (union sxcscript_node_val){.label_i = break_i});
    } else if (sxcscript_token_eq_str(token_this, "continue")) {
        (*token_itr)++;
        sxcscript_parse_push(node_itr, sxcscript_kind_jmp, NULL, (union sxcscript_node_val){.label_i = continue_i});
    } else if (sxcscript_token_eq_str(*token_itr + 1, "(")) {
        (*token_itr)++;
        sxcscript_parse_expr(label, node_itr, token_itr, label_size, break_i, continue_i);
        sxcscript_parse_push(node_itr, sxcscript_kind_call, token_this, (union sxcscript_node_val){0});
    } else {
        sxcscript_parse_push(node_itr, sxcscript_kind_const_get, token_this, (union sxcscript_node_val){0});
        sxcscript_parse_push(node_itr, sxcscript_kind_local_get, token_this, (union sxcscript_node_val){0});
        (*token_itr)++;
    }
}
void sxcscript_parse(struct sxcscript_token* token, struct sxcscript_node* node, struct sxcscript_label* label, int* label_size) {
    struct sxcscript_token* token_itr = token;
    struct sxcscript_node* node_itr = node;
    while (token_itr->data != NULL) {
        sxcscript_parse_expr(label, &node_itr, &token_itr, label_size, -1, -1);
    }
}
void sxcscript_analyze_primitive(struct sxcscript_node* node) {
    for (struct sxcscript_node* node_itr = node; node_itr->kind != sxcscript_kind_null; node_itr++) {
        if (node_itr->token == NULL) {
            continue;
        } else if (sxcscript_token_eq_str(node_itr->token, "addr")) {
            node_itr->kind = sxcscript_kind_addr;
            (node_itr - 1)->kind = sxcscript_kind_nop;
        } else if (sxcscript_token_eq_str(node_itr->token, "get")) {
            node_itr->kind = sxcscript_kind_global_get;
        } else if (sxcscript_token_eq_str(node_itr->token, "set")) {
            node_itr->kind = sxcscript_kind_global_set;
        } else if (sxcscript_token_eq_str(node_itr->token, "const_get")) {
            node_itr->kind = sxcscript_kind_const_get;
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
void sxcscript_analyze_var(struct sxcscript_node* node, struct sxcscript_token** local_token, int* local_offset) {
    int offset_size = 0;
    int local_size = 0;
    for (struct sxcscript_node* node_itr = node; node_itr->kind != sxcscript_kind_null; node_itr++) {
        if (node_itr->kind == sxcscript_kind_label_fnend) {
            offset_size = 0;
            local_size = 0;
        }
        if (node_itr->kind != sxcscript_kind_const_get) {
            continue;
        }
        if (node_itr->token == NULL) {
            continue;
        }
        if (('0' <= node_itr->token->data[0] && node_itr->token->data[0] <= '9') || node_itr->token->data[0] == '-') {
            node_itr->val.literal = sxcscript_token_to_int32(node_itr->token);
            (node_itr + 1)->kind = sxcscript_kind_nop;
            continue;
        }
        for (int i = 0;; i++) {
            if (i == local_size) {
                local_token[local_size] = node_itr->token;
                if (node_itr->val.literal != 0) {
                    local_offset[local_size++] = node_itr->val.literal;
                } else {
                    local_offset[local_size++] = offset_size;
                }
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
            label[node_itr->val.label_i].inst_i = inst_itr - mem;
        } else if (node_itr->kind == sxcscript_kind_const_get) {
            *(inst_itr++) = (union sxcscript_mem){.kind = node_itr->kind};
            *(inst_itr++) = (union sxcscript_mem){.val = node_itr->val.literal};
        } else if (node_itr->kind == sxcscript_kind_jmp || node_itr->kind == sxcscript_kind_jze) {
            *(inst_itr++) = (union sxcscript_mem){.kind = node_itr->kind};
            *(inst_itr++) = (union sxcscript_mem){.val = node_itr->val.label_i};
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
    sxcscript_analyze_primitive(node);
    sxcscript_analyze_var(node, local_token, local_offset);
    sxcscript_analyze_toinst(mem, node, label, label_size);
}
void sxcscript_link(union sxcscript_mem* mem, struct sxcscript_label* label) {
    for (union sxcscript_mem* inst_itr = mem + sxcscript_global_size; inst_itr->kind != sxcscript_kind_null; inst_itr++) {
        if (inst_itr->kind == sxcscript_kind_jmp || inst_itr->kind == sxcscript_kind_jze || inst_itr->kind == sxcscript_kind_call) {
            inst_itr += 1;
            inst_itr->val = label[inst_itr->val].inst_i;
        } else if (inst_itr->kind == sxcscript_kind_const_get) {
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
void sxcscript_run(union sxcscript_mem* mem) {
    int result;
    int a1;
    int a2;
    int a3;
    while (mem[mem[sxcscript_global_ip].val].kind != sxcscript_kind_null) {
        switch (mem[mem[sxcscript_global_ip].val].kind) {
            case sxcscript_kind_const_get:
                (mem[sxcscript_global_ip].val)++;
                mem[(mem[sxcscript_global_sp].val)++].val = mem[mem[sxcscript_global_ip].val].val;
                break;
            case sxcscript_kind_local_get:
                mem[(mem[sxcscript_global_sp].val - 1)].val = mem[mem[sxcscript_global_bp].val + mem[mem[sxcscript_global_sp].val - 1].val].val;
                break;
            case sxcscript_kind_local_set:
                mem[mem[sxcscript_global_bp].val + mem[mem[sxcscript_global_sp].val - 2].val].val = mem[mem[sxcscript_global_sp].val - 1].val;
                mem[sxcscript_global_sp].val -= 2;
                break;
            case sxcscript_kind_global_get:
                mem[mem[sxcscript_global_sp].val - 1].val = mem[mem[mem[sxcscript_global_sp].val - 1].val].val;
                break;
            case sxcscript_kind_global_set:
                mem[mem[mem[sxcscript_global_sp].val - 2].val].val = mem[mem[sxcscript_global_sp].val - 1].val;
                mem[sxcscript_global_sp].val -= 2;
                break;
            case sxcscript_kind_addr:
                mem[(mem[sxcscript_global_sp].val - 1)].val = mem[sxcscript_global_bp].val + mem[mem[sxcscript_global_sp].val - 1].val;
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
            case sxcscript_kind_not:
                mem[mem[sxcscript_global_sp].val - 1].val = !mem[mem[sxcscript_global_sp].val - 1].val;
                break;
            case sxcscript_kind_and:
                mem[mem[sxcscript_global_sp].val - 2].val &= mem[mem[sxcscript_global_sp].val - 1].val;
                mem[sxcscript_global_sp].val -= 1;
                break;
            case sxcscript_kind_eq:
                mem[mem[sxcscript_global_sp].val - 2].val = mem[mem[sxcscript_global_sp].val - 2].val == mem[mem[sxcscript_global_sp].val - 1].val;
                mem[sxcscript_global_sp].val -= 1;
                break;
            case sxcscript_kind_lt:
                mem[mem[sxcscript_global_sp].val - 2].val = mem[mem[sxcscript_global_sp].val - 2].val < mem[mem[sxcscript_global_sp].val - 1].val;
                mem[sxcscript_global_sp].val -= 1;
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
            case sxcscript_kind_open:
                a1 = mem[mem[sxcscript_global_sp].val - 2].val;
                a2 = mem[mem[sxcscript_global_sp].val - 1].val;
                mem[mem[sxcscript_global_sp].val - 2].val = open((const char*)&mem[a1], a2);
                mem[sxcscript_global_sp].val -= 1;
                break;
            case sxcscript_kind_close:
                a1 = mem[mem[sxcscript_global_sp].val - 1].val;
                mem[mem[sxcscript_global_sp].val - 1].val = close(a1);
                break;
            case sxcscript_kind_read:
                a1 = mem[mem[sxcscript_global_sp].val - 3].val;
                a2 = mem[mem[sxcscript_global_sp].val - 2].val;
                a3 = mem[mem[sxcscript_global_sp].val - 1].val;
                mem[mem[sxcscript_global_sp].val - 3].val = read(a1, &mem[a2], a3);
                mem[sxcscript_global_sp].val -= 2;
                break;
            case sxcscript_kind_write:
                a1 = mem[mem[sxcscript_global_sp].val - 3].val;
                a2 = mem[mem[sxcscript_global_sp].val - 2].val;
                a3 = mem[mem[sxcscript_global_sp].val - 1].val;
                mem[mem[sxcscript_global_sp].val - 3].val = write(a1, &mem[a2], a3);
                mem[sxcscript_global_sp].val -= 2;
                break;
            case sxcscript_kind_usleep:
                a1 = mem[mem[sxcscript_global_sp].val - 1].val;
                mem[mem[sxcscript_global_sp].val - 1].val = usleep(a1);
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
    sxcscript_run(mem);
}
void init() {
    struct rlimit rlim = (struct rlimit){.rlim_cur = stacksize, .rlim_max = stacksize};
    setrlimit(RLIMIT_STACK, &rlim);
}
int main() {
    init();
    sxcscript();
}
