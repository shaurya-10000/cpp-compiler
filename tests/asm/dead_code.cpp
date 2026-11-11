// ASM-NOT: mov $99, %rax
// ASM-NOT: mov $98, %rax
// ASM-NOT: mov $97, %rax
// ASM-NOT: mov $96, %rax
// FILE-NOT: never
int main() {
    if (1 > 2) { print_str("never\n"); }
    if (3 > 2) { print_int(1); } else { print_int(2); }
    while (0) { print_int(99); }
    for (int i = 0; 0; i++) { print_int(98); }
    int k = 0;
    while (1) {
        k = k + 1;
        if (k == 3) { break; print_int(97); }
    }
    print_int(k);
    return 0;
    print_int(96);
}
