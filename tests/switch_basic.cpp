enum Op { ADD, SUB, MUL };

int classify(int n) {
    switch (n) {
        case 0: return 100;
        case 1:
        case 2: return 200;
        case -3: return 300;
        default: return 999;
    }
    return -1;
}

int fall(int n) {
    int r = 0;
    switch (n) {
        case 1: r += 1;
        case 2: r += 10;
        case 3: r += 100; break;
        case 4: r += 1000; break;
    }
    return r;
}

int default_middle(int n) {
    int r = 0;
    switch (n) {
        case 1: r = 1; break;
        default: r = 50;
        case 2: r = r + 2; break;
    }
    return r;
}

int apply(int op, int a, int b) {
    switch (op) {
        case ADD: return a + b;
        case SUB: return a - b;
        case MUL: return a * b;
    }
    return 0;
}

int vowel(char c) {
    switch (c) {
        case 'a': case 'e': case 'i': case 'o': case 'u': return 1;
    }
    return 0;
}

int loop_test() {
    int sum = 0;
    for (int i = 0; i < 6; i++) {
        switch (i % 3) {
            case 0: continue;
            case 1: sum += 10; break;
            default: sum += 1;
        }
        sum += 100;
    }
    return sum;
}

int nested(int a, int b) {
    int r = 0;
    switch (a) {
        case 1:
            switch (b) {
                case 1: r = 11; break;
                case 2: r = 12; break;
            }
            r += 1000;
            break;
        case 2: r = 2; break;
    }
    return r;
}

int braces(int n) {
    switch (n) {
        case 5: { int t = n * 2; return t; }
        case 6: { int u = n + 100; return u; }
    }
    return -1;
}

int big(int n) {
    switch (n) {
        case 5000000000: return 1;
        case -5000000000: return 2;
        case 0: return 3;
    }
    return 0;
}

int boolsw(int x) {
    switch (x < 3) {
        case 0: return 10;
        case 1: return 20;
    }
    return 0;
}

int main() {
    print_int(classify(0));
    print_int(classify(1));
    print_int(classify(2));
    print_int(classify(-3));
    print_int(classify(7));
    print_int(fall(1));
    print_int(fall(2));
    print_int(fall(3));
    print_int(fall(4));
    print_int(fall(5));
    print_int(default_middle(1));
    print_int(default_middle(2));
    print_int(default_middle(9));
    print_int(apply(ADD, 7, 3));
    print_int(apply(SUB, 7, 3));
    print_int(apply(MUL, 7, 3));
    print_int(vowel('e'));
    print_int(vowel('z'));
    print_int(loop_test());
    print_int(nested(1, 1));
    print_int(nested(1, 2));
    print_int(nested(1, 3));
    print_int(nested(2, 0));
    print_int(nested(3, 3));
    print_int(braces(5));
    print_int(braces(6));
    print_int(braces(7));
    print_int(big(5000000000));
    print_int(big(-5000000000));
    print_int(big(0));
    print_int(big(1));
    print_int(boolsw(5));
    print_int(boolsw(1));
    return 0;
}
