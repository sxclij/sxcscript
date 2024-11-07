#include <fcntl.h>
#include <sys/resource.h>
#include <unistd.h>

#define sxcapp_stacksize (128 * 1024 * 1024)
#define sxcscript_path "test/01.txt"
#define sxcscript_mem_size (16 * 1024 * 1024)
#define sxcscript_compile_size (2 * 1024)  // デバッグ用に小さくしてるので後で戻す
#define sxcscript_global_size (1024)

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
    sxcscript_global_ip = 0,
    sxcscript_global_sp = 1,
    sxcscript_global_bp = 2,
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

enum bool sxcscript_token_iseq(struct sxcscript_token* a, struct sxcscript_token* b) {
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
enum bool sxcscript_token_iseq_str(struct sxcscript_token* a, const char* str) {
    int str_size = 0;
    while (str[str_size] != '\0') {
        str_size++;
    }
    struct sxcscript_token b = (struct sxcscript_token){.data = str, .size = str_size};
    return sxcscript_token_iseq(a, &b);
}
void sxcscript_run(union sxcscript_mem* mem) {
}
void sxcscript_readfile(char* dst) {
    int fd = open(sxcscript_path, O_RDONLY);
    int n = read(fd, dst, sxcscript_compile_size);
    dst[n] = '\0';
    close(fd);
}
void sxcscript_tokenize(char* src, struct sxcscript_token* token) {
    struct sxcscript_token* token_itr = token;
    struct sxcscript_token token_this = (struct sxcscript_token){.data = src, .size = 0};
    for (char* itr = src; *itr != '\0'; itr++) {
        if (*itr == ' ' || *itr == '\n') {
            if (token_this.size > 0) {
                *(token_itr++) = token_this;
            }
            token_this = (struct sxcscript_token){.data = itr + 1, .size = 0};
        } else if (*itr == ',' || *itr == '(' || *itr == ')') {
            if (token_this.size > 0) {
                *(token_itr++) = token_this;
            }
            *(token_itr++) = (struct sxcscript_token){.data = itr, .size = 1};
            token_this = (struct sxcscript_token){.data = itr + 1, .size = 0};
        } else {
            token_this.size++;
        }
    }
    *(token_itr++) = (struct sxcscript_token){.data = NULL, .size = 0};
}
void sxcscript_parse_push(struct sxcscript_node** node_itr, enum sxcscript_kind kind, struct sxcscript_token* token, int val) {
    *((*node_itr)++) = (struct sxcscript_node){.kind = kind, .token = token, .val = val};
}
void sxcscript_parse_expr(struct sxcscript_token** token_itr, struct sxcscript_node** node_itr, struct sxcscript_label* label, int* label_size, int break_i, int continue_i) {
    if (sxcscript_token_iseq_str(*token_itr, "(")) {
        (*token_itr)++;
        while (!sxcscript_token_iseq_str(*token_itr, ")")) {
            sxcscript_parse_expr(token_itr, node_itr, label, label_size, break_i, continue_i);
            if (sxcscript_token_iseq_str(*token_itr, ",")) {
                (*token_itr)++;
            }
        }
        (*token_itr)++;
    } else if (sxcscript_token_iseq_str(*token_itr + 1, "(")) {
        struct sxcscript_token* token_this = *token_itr;
        (*token_itr)++;
        sxcscript_parse_expr(token_itr, node_itr, label, label_size, break_i, continue_i);
        sxcscript_parse_push(node_itr, sxcscript_kind_call, token_this, 0);
    } else {
        sxcscript_parse_push(node_itr, sxcscript_kind_const_get, *token_itr, 0);
        sxcscript_parse_push(node_itr, sxcscript_kind_local_get, *token_itr, 0);
        (*token_itr)++;
    }
}
void sxcscript_parse(struct sxcscript_token* token, struct sxcscript_node* node, struct sxcscript_label* label, int* label_size) {
    struct sxcscript_token* token_itr = token;
    struct sxcscript_node* node_itr = node;
    while (token_itr->data != NULL) {
        sxcscript_parse_expr(&token_itr, &node_itr, label, label_size, -1, -1);
    }
}
int sxcscript_analyze_searchlabel(struct sxcscript_label* label, int label_size, struct sxcscript_node* node) {
    for (int i = 0; i < label_size; i++) {
        if (label[i].token == NULL) {
            continue;
        }
        if (sxcscript_token_iseq(label[i].token, node->token)) {
            return i;
        }
    }
    return -1;
}
void sxcscript_analyze(union sxcscript_mem* inst, struct sxcscript_node* node, struct sxcscript_token* local_token, int* local_offset, struct sxcscript_label* label, int* label_size) {
    union sxcscript_mem* inst_itr = inst;
    int offset_size = 0;
    int local_size = 0;
    for (struct sxcscript_node* node_itr = node; node_itr->kind != sxcscript_kind_null; node_itr++) {
        if (node_itr->token == NULL) {
            continue;
        } else if (sxcscript_token_iseq_str(node_itr->token, "addr")) {
            node_itr->kind = sxcscript_kind_nop;
            (node_itr - 1)->kind = sxcscript_kind_nop;
        } else if (sxcscript_token_iseq_str(node_itr->token, "local_get")) {
            node_itr->kind = sxcscript_kind_local_get;
        } else if (sxcscript_token_iseq_str(node_itr->token, "local_set")) {
            node_itr->kind = sxcscript_kind_local_set;
        } else if (sxcscript_token_iseq_str(node_itr->token, "global_get")) {
            node_itr->kind = sxcscript_kind_global_get;
        } else if (sxcscript_token_iseq_str(node_itr->token, "global_set")) {
            node_itr->kind = sxcscript_kind_global_set;
        } else if (sxcscript_token_iseq_str(node_itr->token, "add")) {
            node_itr->kind = sxcscript_kind_add;
        } else if (sxcscript_token_iseq_str(node_itr->token, "sub")) {
            node_itr->kind = sxcscript_kind_sub;
        } else if (sxcscript_token_iseq_str(node_itr->token, "mul")) {
            node_itr->kind = sxcscript_kind_mul;
        } else if (sxcscript_token_iseq_str(node_itr->token, "div")) {
            node_itr->kind = sxcscript_kind_div;
        } else if (sxcscript_token_iseq_str(node_itr->token, "mod")) {
            node_itr->kind = sxcscript_kind_mod;
        } else if (sxcscript_token_iseq_str(node_itr->token, "not")) {
            node_itr->kind = sxcscript_kind_not;
        } else if (sxcscript_token_iseq_str(node_itr->token, "and")) {
            node_itr->kind = sxcscript_kind_and;
        } else if (sxcscript_token_iseq_str(node_itr->token, "eq")) {
            node_itr->kind = sxcscript_kind_eq;
        } else if (sxcscript_token_iseq_str(node_itr->token, "lt")) {
            node_itr->kind = sxcscript_kind_lt;
        } else if (sxcscript_token_iseq_str(node_itr->token, "return")) {
            node_itr->kind = sxcscript_kind_return;
        } else if (sxcscript_token_iseq_str(node_itr->token, "read")) {
            node_itr->kind = sxcscript_kind_read;
        } else if (sxcscript_token_iseq_str(node_itr->token, "write")) {
            node_itr->kind = sxcscript_kind_write;
        } else if (sxcscript_token_iseq_str(node_itr->token, "usleep")) {
            node_itr->kind = sxcscript_kind_usleep;
        }
    }
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
            enum bool is_neg = node_itr->token->data[0] == '-';
            int i = is_neg ? 1 : 0;
            int x = 0;
            for (; i < node_itr->token->size; i++) {
                x = (x * 10) + node_itr->token->data[i] - '0';
            }
            node_itr->val = is_neg ? -x : x;
        } else {
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
                if (sxcscript_token_iseq(local_token[i], node_itr->token)) {
                    node_itr->val = local_offset[i];
                    break;
                }
            }
        }
    }
    for (struct sxcscript_node* node_itr = node; node_itr->kind != sxcscript_kind_null; node_itr++) {
        if (node_itr->kind == sxcscript_kind_label) {
            label[node_itr->val].inst_i = inst_itr - inst;
        } else if (node_itr->kind == sxcscript_kind_const_get) {
            *(inst_itr++) = (union sxcscript_mem){.kind = node_itr->kind};
            *(inst_itr++) = (union sxcscript_mem){.val = node_itr->val};
        } else if (node_itr->kind == sxcscript_kind_jmp || node_itr->kind == sxcscript_kind_jze) {
            *(inst_itr++) = (union sxcscript_mem){.kind = node_itr->kind};
            *(inst_itr++) = (union sxcscript_mem){.val = node_itr->val};
        } else if (node_itr->kind == sxcscript_kind_call) {
            *(inst_itr++) = (union sxcscript_mem){.kind = node_itr->kind};
            *(inst_itr++) = (union sxcscript_mem){.val = sxcscript_analyze_searchlabel(label, label_size, node_itr)};
        } else {
            *(inst_itr++) = (union sxcscript_mem){.kind = node_itr->kind};
        }
    }
}
void sxcscript_link(struct sxcscript_node* node, struct sxcscript_label* label, union sxcscript_mem* mem) {
}
void sxcscript_init(union sxcscript_mem* mem) {
    char src[sxcscript_compile_size];
    struct sxcscript_token token[sxcscript_compile_size / sizeof(struct sxcscript_token)];
    struct sxcscript_node node[sxcscript_compile_size / sizeof(struct sxcscript_node)];
    struct sxcscript_label label[sxcscript_compile_size / sizeof(struct sxcscript_label)];
    struct sxcscript_token* local_token[sxcscript_compile_size / sizeof(struct sxcscript_token)];
    int local_offset[sxcscript_compile_size / sizeof(int)];
    int label_size = 0;
    sxcscript_readfile(src);
    sxcscript_tokenize(src, token);
    sxcscript_parse(token, node, label, &label_size);
    sxcscript_analyze(mem + sxcscript_global_size, node, local_token, local_offset, label, &label_size);
    sxcscript_link(node, label, mem + sxcscript_global_size);
}
void sxcscript() {
    static union sxcscript_mem mem[sxcscript_mem_size];
    sxcscript_init(mem);
    sxcscript_run(mem);
}
void init() {
    struct rlimit rlim = (struct rlimit){.rlim_cur = sxcapp_stacksize, .rlim_max = sxcapp_stacksize};
    setrlimit(RLIMIT_STACK, &rlim);
}
int main() {
    init();
    sxcscript();
}