int gcd(int a, int b) {
    while (b != 0) {
        int t = a % b;
        a = b;
        b = t;
    }
    return a;
}
int power(int base, int exp) {
    int result = 1;
    for (int i = 0; i < exp; i = i + 1) { result = result * base; }
    return result;
}
int main() {
    print_int(gcd(48, 18));
    print_int(gcd(17, 5));
    print_int(power(2, 10));
    print_int(power(3, 5));
    return 0;
}
