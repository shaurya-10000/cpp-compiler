// ERROR: struct values cannot be copied
struct P { int x; };
int main() { struct P a; struct P b; a = b; return 0; }
