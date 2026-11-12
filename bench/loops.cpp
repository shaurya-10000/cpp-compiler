int main() {
    int s = 0;
    for (int i = 0; i < 20000000; i++) {
        s += (i * 3 + 1) % 7;
        if ((i & 1023) == 0) { s ^= i >> 3; }
    }
    print_int(s);
    return 0;
}