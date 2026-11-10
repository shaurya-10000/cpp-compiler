int main() {
    int is_prime[50];
    for (int i = 0; i < 50; i = i + 1) { is_prime[i] = 1; }
    is_prime[0] = 0;
    is_prime[1] = 0;
    for (int i = 2; i * i < 50; i = i + 1) {
        if (is_prime[i]) {
            for (int j = i * i; j < 50; j = j + i) { is_prime[j] = 0; }
        }
    }
    for (int i = 0; i < 50; i = i + 1) {
        if (is_prime[i]) { print_int(i); }
    }
    return 0;
}
