// ERROR: invalid cast from 'double' to 'int*'
int main() { double d = 1.5; int* p = (int*)d; return 0; }
