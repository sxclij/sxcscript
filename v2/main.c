#include <fcntl.h>
#include <sys/resource.h>
#include <unistd.h>

#define sxcapp_stacksize (128 * 1024 * 1024)
#define sxcscript_path "test/01.txt"
#define sxcscript_mem_capacity (16 * 1024 * 1024)
#define sxcscript_compile_capacity (2 * 1024)

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
    struct sxcscript_token b = {.data = str, .size = str_size};
    return sxcscript_token_iseq(a, &b);
}
void sxcscript_run(union sxcscript_mem* mem) {
}
void sxcscript_readfile(char* dst) {
    int fd = open(sxcscript_path, O_RDONLY);
    int n = read(fd, dst, sxcscript_compile_capacity);
    dst[n] = '\0';
    close(fd);
}
void sxcscript_tokenize(char* src, struct sxcscript_token* token) {
    struct sxcscript_token* token_itr = token;
    struct sxcscript_token token_this = {.data = src, .size = 0};
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
void sxcscript_parse_expr(struct sxcscript_token** token_itr, struct sxcscript_node** node_itr, struct sxcscript_label** label_itr, struct sxcscript_label* label, int break_i, int continue_i) {
    if (sxcscript_token_iseq_str(*token_itr, "(")) {
        (*token_itr)++;
        while (!sxcscript_token_iseq_str(*token_itr, ")")) {
            sxcscript_parse_expr(token_itr, node_itr, label_itr, label, break_i, continue_i);
            if (sxcscript_token_iseq_str(*token_itr, ",")) {
                (*token_itr)++;
            }
        }
        (*token_itr)++;
    } else if (sxcscript_token_iseq_str(*token_itr + 1, "(")) {
        struct sxcscript_token* token_this = *token_itr;
        (*token_itr)++;
        sxcscript_parse_expr(token_itr, node_itr, label_itr, label, break_i, continue_i);
        sxcscript_parse_push(node_itr, sxcscript_kind_call, token_this, 0);
    } else {
        sxcscript_parse_push(node_itr, sxcscript_kind_nop, *token_itr, 0);
        sxcscript_parse_push(node_itr, sxcscript_kind_const_get, *token_itr, 0);
        (*token_itr)++;
    }
}
void sxcscript_parse(struct sxcscript_token* token, struct sxcscript_node* node, struct sxcscript_label* label) {
    struct sxcscript_token* token_itr = token;
    struct sxcscript_node* node_itr = node;
    struct sxcscript_label* label_itr = label;
    while (token_itr->data != NULL) {
        sxcscript_parse_expr(&token_itr, &node_itr, &label_itr, label, -1, -1);
    }
}
void 
void sxcscript_link(struct sxcscript_node* node, struct sxcscript_label* label, union sxcscript_mem* mem) {
}
void sxcscript_init(union sxcscript_mem* mem) {
    char src[sxcscript_compile_capacity];
    struct sxcscript_token token[sxcscript_compile_capacity / sizeof(struct sxcscript_token)];
    struct sxcscript_node node[sxcscript_compile_capacity / sizeof(struct sxcscript_node)];
    struct sxcscript_label label[sxcscript_compile_capacity / sizeof(struct sxcscript_label)];
    union sxcscript_mem* global_begin;
    union sxcscript_mem* inst_begin;
    union sxcscript_mem* data_begin;
    sxcscript_readfile(src);
    sxcscript_tokenize(src, token);
    sxcscript_parse(token, node, label);
    sxcscript_analyze(node, mem);
    sxcscript_link(node, label, mem);
}
void sxcscript() {
    static union sxcscript_mem mem[sxcscript_mem_capacity];
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