int length(char* s) {
    int n = 0;
    while (*s) { n = n + 1; s = s + 1; }
    return n;
}
void print_reverse(char* s) {
    int n = length(s);
    for (int i = n - 1; i >= 0; i = i - 1) { print_char(s[i]); }
    print_char('\n');
}
int count_char(char* s, char c) {
    int n = 0;
    for (int i = 0; s[i] != 0; i = i + 1) {
        if (s[i] == c) { n = n + 1; }
    }
    return n;
}
int main() {
    char* msg = "hello, world";
    print_str(msg);
    print_char('\n');
    print_int(length(msg));
    print_reverse("stressed");
    print_int(count_char(msg, 'l'));
    print_int(count_char("banana", 'a'));
    char* t = msg + 7;
    print_str(t);
    print_char('\n');
    print_int(t[0] == 'w');
    return 0;
}
