// ERROR: cannot convert 'int' to 'struct A'
struct A { int x; };
struct A make() {
    return 5;
}
int main() { return 0; }
