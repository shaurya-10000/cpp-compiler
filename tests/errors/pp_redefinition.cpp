// ERROR: redefinition of macro 'X'
#define X 1
#define X 2
int main() { return X; }
