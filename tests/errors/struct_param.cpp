// ERROR: struct parameters must be passed by pointer
struct P { int x; };
int f(struct P p) { return 0; }
int main() { return 0; }
