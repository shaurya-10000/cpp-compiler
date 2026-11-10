#pragma once

// Library functions written in minicc itself. Parsed and prepended to every program.
inline const char* const kPrelude = R"MINICC(
void __print_digits(int n) {
    if (n >= 10) { __print_digits(n / 10); }
    print_char(48 + n % 10);
}

void print_double(double x) {
    if (x < 0.0) { print_char(45); x = -x; }
    int ip = x;
    __print_digits(ip);
    print_char(46);
    double frac = x - ip;
    for (int i = 0; i < 6; i = i + 1) {
        frac = frac * 10.0;
        int d = frac;
        print_char(48 + d);
        frac = frac - d;
    }
    print_char(10);
}
)MINICC";