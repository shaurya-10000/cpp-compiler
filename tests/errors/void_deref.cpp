// ERROR: cannot dereference 'void*'
int main() { int x = 1; void* p = &x; return *p; }
