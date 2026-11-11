int main() {
    print_int(7 / 2);
    print_int(-7 / 2);
    print_int(7 % 3);
    print_int(-7 % 3);
    print_int(1 << 62);
    print_int(-1 >> 3);
    print_int(5 - 8);
    print_int(3 == 3);
    print_int('a' + 1);
    print_int((char)200);
    print_int((char)300 + 1);
    print_int((bool)5 + 1);
    print_double((double)7 / 2);
    int z = 0;
    print_int(0 && (10 / z));
    print_int(1 || (10 / z));
    print_int(1 ? 7 : 8);
    print_int(0 ? 7 : 8);
    print_int(!5);
    print_int(!0);
    return 0;
}
