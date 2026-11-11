int add(int a, int b) { return a + b; }
int mul(int a, int b) { return a * b; }
int main() {
    int x = 3;
    int y = 4;
    print_int(add(x, y) * add(y, 1) + mul(x, 2) - add(1, mul(2, 3)));
    print_int((x + y) * (x - y) + (x * y) / (y - 1));
    print_int(x < y && y < 10 || x == 99);
    int a[4];
    a[0] = x * 2 + 1;
    a[1] = a[0] * y - 3;
    a[2] = a[1] + a[0] * 2;
    a[3] = a[a[0] - 6] + a[2 - 1];
    print_int(a[0]);
    print_int(a[1]);
    print_int(a[2]);
    print_int(a[3]);
    char buf[4] = "abc";
    char* p = buf;
    print_int(p[1] + *(p + 2) - p[0]);
    double d = 2.5;
    print_double(d * 2 + 1);
    print_double(1 + d * d);
    return 0;
}
