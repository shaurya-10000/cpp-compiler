int popcount(int n) {
    int c = 0;
    while (n) { n &= n - 1; c++; }
    return c;
}
int main() {
    print_int(popcount(255));
    print_int(popcount(1024));
    print_int(popcount(0));
    print_int(popcount(1000));
    print_int((1 << 3) | (1 << 5));
    print_int(40 >> 3 & 1);
    print_int(40 >> 4 & 1);
    print_int(5 & 3 == 3);
    return 0;
}
