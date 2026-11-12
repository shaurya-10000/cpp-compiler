enum Color { RED, GREEN = 5, BLUE };
enum { A = 1 << 3, B = A | 2, C = B * 2 - A, D = -1 };

enum Color pick(int n) {
    if (n == 0) { return RED; }
    return BLUE;
}

int main() {
    print_int(RED);
    print_int(GREEN);
    print_int(BLUE);
    print_int(A);
    print_int(B);
    print_int(C);
    print_int(D);
    enum Color c = pick(1);
    print_int(c == BLUE);
    enum Local { X = 100, Y };
    print_int(Y);
    int total = 0;
    for (int i = RED; i <= GREEN; i++) { total += i; }
    print_int(total);
    return 0;
}
