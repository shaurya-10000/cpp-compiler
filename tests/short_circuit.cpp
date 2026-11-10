int bump(int x) {
    print_int(x);
    return 1;
}
int main() {
    if (0 && bump(1)) { print_int(100); }
    if (1 || bump(2)) { print_int(9); }
    if (1 && bump(3)) { print_int(7); }
    print_int(1 && 0);
    print_int(0 || 5);
    return 0;
}
