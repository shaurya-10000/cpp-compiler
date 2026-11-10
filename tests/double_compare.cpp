int main() {
    double x = 1.5;
    double y = 2.5;
    print_int(x < y);
    print_int(x > y);
    print_int(x <= 1.5);
    print_int(x >= y);
    print_int(x == 1.5);
    print_int(x != 1.5);
    print_int(x < 2);
    if (x) { print_int(11); }
    double zero = 0.0;
    if (zero) { print_int(22); } else { print_int(33); }
    print_int(!zero);
    print_int(x && y);
    print_int(zero || 0);
    return 0;
}
