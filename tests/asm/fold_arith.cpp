// ASM-HAS: mov $28, %rax
// ASM-HAS: mov $1029, %rax
// ASM-HAS: mov $255, %rax
// ASM-HAS: mov $-7, %rax
// ASM-NOT: imul
// ASM-NOT: idiv
// ASM-NOT: shl
// ASM-NOT: push %rax
int main() {
    print_int(2 * 3 + 4 * 6 - 10 / 2 + 7 % 4);
    print_int((1 << 10) | 5);
    print_int(~0 & 255);
    print_int(-(3 + 4));
    print_int(5 > 3 && 2 < 1);
    print_int(true + true);
    return 0;
}
