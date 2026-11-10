int main() {
    char buf[16];
    char* src = "abc";
    int i = 0;
    while (src[i] != 0) {
        buf[i] = src[i] - 32;
        i = i + 1;
    }
    buf[i] = 0;
    print_str(buf);
    print_char('\n');
    print_int(sizeof(char));
    print_int(sizeof(int));
    print_int(sizeof(double));
    print_int(sizeof(char*));
    print_int(sizeof(int**));
    return 0;
}
