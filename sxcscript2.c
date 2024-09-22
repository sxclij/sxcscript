#include <fcntl.h>
#include <stdint.h>
#include <unistd.h>

#define script_path "testscript.txt"
#define script_capacity (1 << 16)

enum script_kind {
    script_kind_null,
    script_kind_push,
    script_kind_nop,
    script_kind_add,
    script_kind_sub,
    script_kind_ret,
};
struct script_node {
    enum script_kind kind;
    char s[16];
    struct script_node* a1;
    struct script_node* a2;
    struct script_node* prev;
    struct script_node* next;
};
struct script_node* script_push(struct script_node** dst_itr, enum script_kind kind) {
    struct script_node* prev = *dst_itr;
    (*dst_itr)++;
    struct script_node* this = *dst_itr;
    prev->next = this;
    this->prev = prev;
    this->kind = kind;
    return this;
}
void script_parse_val(struct script_node** dst_itr, const char** src_itr) {
    struct script_node* this = script_push(dst_itr, script_kind_push);
    uint32_t i = 0;
    while (**src_itr == ' ') {
        (*src_itr)++;
    }
    while (**src_itr != ' ') {
        this->s[i] = **src_itr;
        (*src_itr)++;
        i++;
    }
    this->s[i + 1] = '\0';
    while (**src_itr == ' ') {
        (*src_itr)++;
    }
}
void script_parse_add(struct script_node** dst_itr, const char** src_itr) {
    script_parse_val(dst_itr, src_itr);
    while (**src_itr == '+' || **src_itr == '-') {
        if (**src_itr == '+') {
            (*src_itr)++;
            script_parse_val(dst_itr, src_itr);
            script_push(dst_itr, script_kind_add);
        } else if (**src_itr == '-') {
            (*src_itr)++;
            script_parse_val(dst_itr, src_itr);
            script_push(dst_itr, script_kind_sub);
        }
    }
}
void script_load(struct script_node* dst, const char* src) {
    struct script_node* dst_itr = dst;
    const char* src_itr = src;
    dst->kind = script_kind_nop;
    script_parse_add(&dst_itr, &src_itr);
}

int main() {
    char buf[script_capacity];
    struct script_node script_node[script_capacity];

    int fd = open(script_path, O_RDONLY);
    int buf_n = read(fd, buf, sizeof(buf) - 1);
    buf[buf_n] = '\0';
    close(fd);
    write(STDOUT_FILENO, buf, buf_n);

    script_load(script_node, buf);
    return 0;
}