int main() {
    int i = 5;
    print_int(i++);
    print_int(i);
    print_int(++i);
    print_int(i--);
    print_int(--i);

    int x = 10;
    x += 5; print_int(x);
    x -= 3; print_int(x);
    x *= 2; print_int(x);
    x /= 5; print_int(x);
    x %= 3; print_int(x);
    x <<= 4; print_int(x);
    x >>= 2; print_int(x);
    x |= 9; print_int(x);
    x &= 6; print_int(x);
    x ^= 15; print_int(x);

    print_int(12 & 10);
    print_int(12 | 3);
    print_int(12 ^ 5);
    print_int(1 << 10);
    print_int(-16 >> 2);
    print_int(~0);

    print_int(7 > 3 ? 100 : 200);
    print_int(2 > 3 ? 100 : 200);
    int m = 5;
    print_int(m > 3 ? (m > 4 ? 1 : 2) : 3);
    double d = 1 ? 2 : 3.5;
    print_double(d);

    int a[3] = {10, 20, 30};
    int j = 0;
    a[j++] += 5;
    print_int(a[0]);
    print_int(j);
    int* p = a;
    p += 2;
    print_int(*p);
    p--;
    print_int(*p);
    ++*p;
    print_int(a[1]);

    double dd = 1.5;
    dd += 2;
    print_double(dd);
    dd++;
    print_double(dd);
    char c = 'a';
    c++;
    print_char(c);
    print_char('\n');

    int k = 0;
    do { k += 3; } while (k < 10);
    print_int(k);
    int cnt = 0;
    do {
        cnt++;
        if (cnt == 2) { continue; }
        if (cnt == 4) { break; }
    } while (cnt < 10);
    print_int(cnt);
    return 0;
}
