// ERROR: cannot convert 'struct B' to 'struct A'
struct A { int x; };
struct B { int y; };
int main() {
    struct A a;
    struct B b;
    a = b;
    return 0;
}
