int main() {
    int x = 10;
    void* v = &x;
    int* p = (int*)v;
    print_int(*p);
    char* names[3];
    names[0] = "ann";
    names[1] = "bob";
    names[2] = "cy";
    for (int i = 0; i < 3; i = i + 1) {
        print_str(names[i]);
        print_char('\n');
    }
    char** pp = names;
    print_str(*(pp + 2));
    print_char('\n');
    print_str(pp[1]);
    print_char('\n');
    int a[4];
    int* lo = &a[0];
    int* hi = &a[3];
    print_int(lo < hi);
    print_int(hi < lo);
    print_int(hi - lo);
    int n = (int)hi - (int)lo;
    print_int(n);
    print_int(!lo);
    int* z = 0;
    print_int(!z);
    if (z) { print_int(111); } else { print_int(222); }
    return 0;
}
