int main() {
    char comp[60000];
    int total = 0;
    for (int rep = 0; rep < 100; rep++) {
        for (int i = 0; i < 60000; i++) { comp[i] = 0; }
        int count = 0;
        for (int i = 2; i < 60000; i++) {
            if (!comp[i]) {
                count++;
                for (int j = i * 2; j < 60000; j += i) { comp[j] = 1; }
            }
        }
        total += count;
    }
    print_int(total);
    return 0;
}