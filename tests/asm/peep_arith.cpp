// ASM-HAS: shl $3, %rax
// ASM-HAS: add $1, %rax
// ASM-HAS: sub $1, %rax
// ASM-HAS: imul $5, %rax, %rax
// ASM-HAS: imul $3, %rax, %rax
// ASM-HAS: cmp $10, %rax
// ASM-HAS: cmp $100, %rax
// ASM-HAS: jge
// ASM-HAS: jl
// ASM-NOT: imul %rcx
// ASM-NOT: push
// ASM-NOT: pop
int g = 7;
int main() {
    int x = g;
    print_int(x * 8);
    print_int(x + 1);
    print_int(x - 1);
    print_int(x * 5);
    print_int(x < 10);
    print_int(3 * x);
    print_int(100 < x);
    if (x < 10) { print_int(100); }
    if (x >= 10) { print_int(200); }
    return 0;
}
