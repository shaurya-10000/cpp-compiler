int g[] = {5, 6, 7};

int main() {
    int a[5] = {1, 2, 3};
    int sum = 0;
    for (int i = 0; i < 5; i++) { sum += a[i]; }
    print_int(sum);
    print_int(a[3]);
    print_int(a[4]);
    int b[] = {7, 8, 9, 10};
    print_int(b[3]);
    char s[8] = "hey";
    print_str(s);
    print_char('\n');
    print_int(s[3]);
    print_int(s[7]);
    char t[] = "abc!";
    print_str(t);
    print_char('\n');
    t[0] = 'X';
    print_str(t);
    print_char('\n');
    double d[3] = {1, 2.5};
    print_double(d[0] + d[1] + d[2]);
    char* words[] = {"one", "two", "three"};
    print_str(words[2]);
    print_char('\n');
    print_int(g[0] + g[1] + g[2]);
    return 0;
}
