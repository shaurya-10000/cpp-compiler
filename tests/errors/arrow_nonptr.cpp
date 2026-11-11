// ERROR: '->' requires a pointer to struct
struct P { int x; };
int main() { struct P p; return p->x; }
