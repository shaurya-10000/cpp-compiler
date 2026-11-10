void swap(int* a, int* b) {
    int t = *a;
    *a = *b;
    *b = t;
}
void set(int* p, int v) { *p = v; }
int main() {
    int x = 5;
    int y = 9;
    swap(&x, &y);
    print_int(x);
    print_int(y);
    int* p = &x;
    *p = *p + 100;
    print_int(x);
    set(&y, 42);
    print_int(y);
    int** pp = &p;
    **pp = 7;
    print_int(x);
    int* q = 0;
    print_int(q == 0);
    print_int(p != 0);
    print_int(p == &x);
    return 0;
}
