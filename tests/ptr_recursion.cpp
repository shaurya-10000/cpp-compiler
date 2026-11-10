int sum_arr(int* a, int n) {
    if (n == 0) { return 0; }
    return a[0] + sum_arr(a + 1, n - 1);
}
int max_arr(int* a, int n) {
    int m = a[0];
    for (int i = 1; i < n; i = i + 1) { if (a[i] > m) { m = a[i]; } }
    return m;
}
void reverse(int* a, int n) {
    int* lo = a;
    int* hi = a + n - 1;
    while (lo < hi) {
        int t = *lo;
        *lo = *hi;
        *hi = t;
        lo = lo + 1;
        hi = hi - 1;
    }
}
int main() {
    int a[6];
    for (int i = 0; i < 6; i = i + 1) { a[i] = i * 3 + 1; }
    print_int(sum_arr(a, 6));
    print_int(max_arr(a, 6));
    reverse(a, 6);
    for (int i = 0; i < 6; i = i + 1) { print_int(a[i]); }
    return 0;
}
