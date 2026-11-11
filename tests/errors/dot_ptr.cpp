// ERROR: '.' requires a struct value
struct P { int x; };
int main() { struct P p; struct P* q = &p; return q.x; }
