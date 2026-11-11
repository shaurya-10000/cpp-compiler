// ERROR: functions cannot return a struct by value
struct P { int x; };
struct P f() { struct P p; return p; }
int main() { return 0; }
