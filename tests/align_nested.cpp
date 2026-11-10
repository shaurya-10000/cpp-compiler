int id(int x) { return x; }
int add3(int a, int b, int c) { return a + b + c; }
int main() {
    print_int(1 + id(2));
    print_int(1 + (2 + (3 + id(4))));
    print_int(add3(id(1), id(2) + id(3), id(4) * id(5)));
    return 0;
}
