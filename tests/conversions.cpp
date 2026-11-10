int main() {
    int i = 7;
    double d = i;
    print_double(d / 2);
    int t = 9.99;
    print_int(t);
    print_int((int)2.75);
    print_int((int)-2.75);
    print_double((double)3);
    char c = 321;
    print_int(c);
    char big = 200;
    print_int(big);
    bool b = 5;
    print_int(b);
    bool z = 0.0;
    print_int(z);
    print_int(i + 'a');
    print_int(true + true);
    print_char('H');
    print_char('i');
    print_char('\n');
    return 0;
}
