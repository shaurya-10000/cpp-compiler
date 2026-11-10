int fact(int n) {
    if (n <= 1) { return 1; }
    return n * fact(n - 1);
}
int main() {
    print_int(fact(5));
    print_int(fact(10));
    return 0;
}
