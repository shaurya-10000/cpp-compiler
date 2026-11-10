int fib(int n) {
    if (n < 2) { return n; }
    return fib(n - 1) + fib(n - 2);
}
int main() {
    for (int i = 0; i < 10; i = i + 1) { print_int(fib(i)); }
    print_int(fib(20));
    return 0;
}
