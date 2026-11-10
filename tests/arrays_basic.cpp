int main() {
    int a[5];
    for (int i = 0; i < 5; i = i + 1) { a[i] = i * i; }
    int sum = 0;
    for (int i = 0; i < 5; i = i + 1) { sum = sum + a[i]; }
    print_int(sum);
    print_int(a[4]);
    a[2] = a[1] + a[3];
    print_int(a[2]);
    int* p = a;
    print_int(*p);
    print_int(*(p + 3));
    print_int(p[4]);
    int* e = &a[4];
    print_int(e - p);
    print_int(*(e - 1));
    return 0;
}
