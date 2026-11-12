// ERROR: cannot convert 'int' to 'struct A'
struct A { int x; };
int take(struct A a) { return a.x; }
int main() {
    return take(5);
}
