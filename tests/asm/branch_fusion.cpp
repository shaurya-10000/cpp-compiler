// ASM-HAS: jge
// ASM-HAS: jne
// ASM-NOT: setl
// ASM-NOT: sete
// ASM-NOT: movzbq
// ASM-NOT: push
int main() {
    int n = 0;
    int i = 0;
    while (i < 10) {
        i = i + 1;
        if (i % 2 == 0) { n = n + i; }
    }
    print_int(n);
    return 0;
}
