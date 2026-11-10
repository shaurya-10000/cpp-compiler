int is_odd(int n);
int is_even(int n) {
    if (n == 0) { return 1; }
    return is_odd(n - 1);
}
int is_odd(int n) {
    if (n == 0) { return 0; }
    return is_even(n - 1);
}
int main() {
    print_int(is_even(10));
    print_int(is_odd(7));
    print_int(is_even(7));
    return 0;
}
