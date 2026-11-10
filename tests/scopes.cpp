int main() {
    int x = 1;
    { int x = 2; print_int(x); }
    print_int(x);
    for (int i = 0; i < 2; i = i + 1) { int y = i * 10; print_int(y); }
    return 0;
}
