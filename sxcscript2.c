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
};
struct script_node {
    enum script_kind kind;
    uint64_t s;
    struct script_node* prev;
    struct script_node* next;
};

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
    while (**src_itr == ' ' || **src_itr == '\n') {
        (*src_itr)++;
    }
}
void script_parse_push(struct script_node** dst_itr, const char* s, enum script_kind kind) {
    struct script_node* prev = *dst_itr;
    (*dst_itr)++;
    struct script_node* this = *dst_itr;
    prev->next = this;
    this->prev = prev;
    this->kind = kind;
    this->s = 0;
    for (int32_t i = 0; s[i] != ' ' && s[i] != '(' && s[i] != ')' && s[i] != ','; i++) {
        this->s <<= 8;
        if ('0' <= s[0] && s[0] <= '9') {
            this->s |= s[i] - '0';
        } else {
            this->s |= s[i];
        }
    }
    return;
}
void script_parse_expr(struct script_node** dst_itr, const char** src_itr) {
    const char* this_s = *src_itr;
    if (**src_itr == '(') {
        script_parse_nexttoken(src_itr);
        while (**src_itr != ')') {
            script_parse_expr(dst_itr, src_itr);
            while (**src_itr == ',' || **src_itr == '\n') {
                script_parse_nexttoken(src_itr);
            }
        }
        script_parse_nexttoken(src_itr);
        return;
    }
    script_parse_nexttoken(src_itr);
    if (**src_itr == '(') {
        script_parse_expr(dst_itr, src_itr);
        script_parse_push(dst_itr, this_s, script_kind_call);
        return;
    }
    script_parse_push(dst_itr, this_s, script_kind_push);
}
void script_load(struct script_node* dst, const char* src) {
    struct script_node* dst_itr = dst;
    const char* src_itr = src;
    dst->kind = script_kind_nop;
    script_parse_expr(&dst_itr, &src_itr);
}
void script_exec(struct script_node* script_node, int32_t* mem) {
    int32_t* sp = &mem[256];
    struct script_node* pc = script_node;
    while (pc != NULL) {
        switch (pc->kind) {
            case script_kind_nop:
                break;
            case script_kind_push:
                *(sp++) = pc->s;
                break;
            case script_kind_call:
                switch (pc->s) {
                    case 7171958:  // mov
                        mem[sp[-2]] = mem[sp[-1]];
                        sp -= 2;
                        break;
                    case 1836021353:  // movi
                        mem[sp[-2]] = sp[-1];
                        sp -= 2;
                        break;
                    case 6382692:  // add
                        mem[sp[-2]] += mem[sp[-1]];
                        sp -= 1;
                        break;
                    default:
                        break;
                }
                break;
        }
        pc = pc->next;
    }
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
    script_exec(script_node, mem);
    return 0;
}