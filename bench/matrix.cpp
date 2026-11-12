int main() {
    int n = 64;
    int a[4096];
    int b[4096];
    int c[4096];
    for (int i = 0; i < n * n; i++) {
        a[i] = i % 13;
        b[i] = i % 7;
    }
    int checksum = 0;
    for (int rep = 0; rep < 20; rep++) {
        for (int i = 0; i < n; i++) {
            for (int j = 0; j < n; j++) {
                int acc = 0;
                for (int k = 0; k < n; k++) {
                    acc += a[i * n + k] * b[k * n + j];
                }
                c[i * n + j] = acc;
            }
        }
        for (int i = 0; i < n * n; i++) { checksum += c[i]; }
    }
    print_int(checksum);
    return 0;
}