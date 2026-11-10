int sum8(int a, int b, int c, int d, int e, int f, int g, int h) {
    return a + b * 2 + c * 3 + d * 4 + e * 5 + f * 6 + g * 7 + h * 8;
}
int f7(int a, int b, int c, int d, int e, int f, int g) {
    return a - b + c - d + e - f + g;
}
int pick(int a, int b, int c, int d, int e, int f, int g, int h, int i) {
    int t = g * 100 + h * 10 + i;
    return t + a;
}
int main() {
    print_int(sum8(1, 2, 3, 4, 5, 6, 7, 8));
    print_int(f7(10, 1, 20, 2, 30, 3, 40));
    print_int(pick(1, 2, 3, 4, 5, 6, 7, 8, 9));
    return 0;
}
