int f(int n) {
    if (n > 0) { return 1; } else { return 2; }
    print_int(999);
    return 3;
}
int g(int n) {
    for (int i = 0; i < n; i++) {
        if (i == 2) { continue; print_int(111); }
        if (i == 4) { break; }
        print_int(i);
    }
    return n;
}
int main() {
    print_int(f(5));
    print_int(f(-5));
    g(6);
    int a = 3;
    a;
    a + 1;
    5;
    do { a++; } while (0);
    print_int(a);
    while (0) { a = 100; }
    if (0) { a = 200; } else { a = a + 10; }
    print_int(a);
    for (int j = 0; j < 3; j++) { }
    return 0;
}
