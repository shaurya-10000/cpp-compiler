// ERROR: incompatible types in conditional expression
int main() { int* p = 0; int x = 1 ? p : 2.5; return 0; }
