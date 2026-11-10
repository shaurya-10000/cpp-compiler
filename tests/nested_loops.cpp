int main() {
    for (int i = 0; i < 3; i = i + 1) {
        for (int j = 0; j < 3; j = j + 1) {
            if (j == 1) { break; }
            print_int(i * 10 + j);
        }
    }
    int k = 0;
    while (k < 5) {
        k = k + 1;
        if (k % 2 == 0) { continue; }
        print_int(k);
    }
    return 0;
}
