double square(double v) { return v * v; }
double avg(double a, double b) { return (a + b) / 2; }
int half(int n) { return n / 2; }
double mix(int a, double b, char c, bool d, double e, int f, double g, double h) {
    return a + b + c + d + e + f + g + h;
}
int main() {
    print_double(square(3));
    print_double(avg(1, 2));
    print_int(half(7.9));
    print_double(mix(1, 0.5, 'a', true, 0.25, 2, 0.125, 0.125));
    double acc = 0.0;
    for (int i = 1; i <= 4; i = i + 1) { acc = acc + 0.5 * i; }
    print_double(acc);
    return 0;
}
