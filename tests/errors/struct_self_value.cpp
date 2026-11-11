// ERROR: has incomplete type
struct N { struct N inner; };
int main() { return 0; }
