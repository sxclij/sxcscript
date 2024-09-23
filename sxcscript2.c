#include <fcntl.h>
#include <stdint.h>
#include <unistd.h>

#define script_path "testscript.txt"
#define script_capacity (1 << 16)

enum script_kind {
    script_kind_null,
    script_kind_nop,
    script_kind_push,
    script_kind_call,
    script_kind_ret,
    script_kind_add,
    script_kind_sub,
    script_kind_mul,
    script_kind_div,
    script_kind_mod,
};
struct script_node {
    enum script_kind kind;
    const char* s;
    struct script_node* prev;
    struct script_node* next;
};

struct script_node* script_node_push(struct script_node** dst_itr, const char* s, enum script_kind kind) {
    struct script_node* prev = *dst_itr;
    (*dst_itr)++;
    struct script_node* this = *dst_itr;
    prev->next = this;
    this->prev = prev;
    this->kind = kind;
    this->s = s;
    return this;
}
void script_parse_nexttoken(const char** src_itr) {
    if (**src_itr == '(' || **src_itr == ')' || **src_itr == ',') {
        (*src_itr)++;
    } else {
        while (('0' <= **src_itr && **src_itr <= '9') ||
               ('a' <= **src_itr && **src_itr <= 'z') ||
               ('A' <= **src_itr && **src_itr <= 'Z') ||
               **src_itr == '_') {
            (*src_itr)++;
        }
    }
    while (**src_itr == ' ' ||
           **src_itr == '\n') {
        (*src_itr)++;
    }
}
void script_parse_expr(struct script_node** dst_itr, const char** src_itr) {
    const char* this_s = *src_itr;
    if (**src_itr == '(') {
        script_parse_nexttoken(src_itr);
        script_parse_expr(dst_itr, src_itr);
        script_parse_nexttoken(src_itr);
        return;
    }
    script_parse_nexttoken(src_itr);
    if (**src_itr == '(') {
        script_parse_nexttoken(src_itr);
        script_parse_expr(dst_itr, src_itr);
        script_node_push(dst_itr, this_s, script_kind_call);
        script_parse_nexttoken(src_itr);
        return;
    }
    script_node_push(dst_itr, this_s, script_kind_push);
    if (**src_itr == ',') {
        script_parse_nexttoken(src_itr);
        script_parse_expr(dst_itr, src_itr);
    }
}
void script_load(struct script_node* dst, const char* src) {
    struct script_node* dst_itr = dst;
    const char* src_itr = src;
    dst->kind = script_kind_nop;
    script_parse_expr(&dst_itr, &src_itr);
    script_node_push(&dst_itr, NULL, script_kind_null);
}

int main() {
    char buf[script_capacity];
    int32_t mem[script_capacity];
    struct script_node script_node[script_capacity];

    int fd = open(script_path, O_RDONLY);
    int buf_n = read(fd, buf, sizeof(buf) - 1);
    buf[buf_n] = '\0';
    close(fd);
    write(STDOUT_FILENO, buf, buf_n);
    write(STDOUT_FILENO, "\n", 1);

    script_load(script_node, buf);
    return 0;
}