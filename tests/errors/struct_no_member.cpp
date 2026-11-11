// ERROR: has no member 'y'
struct P { int x; };
int main() { struct P p; p.y = 1; return 0; }
