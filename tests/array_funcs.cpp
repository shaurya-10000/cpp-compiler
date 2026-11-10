void fill(int* a, int n) {
    for (int i = 0; i < n; i = i + 1) { a[i] = (n - i) * 3 % 7; }
}
void sort(int* a, int n) {
    for (int i = 0; i < n; i = i + 1) {
        for (int j = 0; j + 1 < n - i; j = j + 1) {
            if (a[j] > a[j + 1]) {
                int t = a[j];
                a[j] = a[j + 1];
                a[j + 1] = t;
            }
        }
    }
}
int sum(int* a, int n) {
    int s = 0;
    for (int i = 0; i < n; i = i + 1) { s = s + a[i]; }
    return s;
}
int main() {
    int v[8];
    fill(v, 8);
    print_int(sum(v, 8));
    sort(v, 8);
    for (int i = 0; i < 8; i = i + 1) { print_int(v[i]); }
    double d[3];
    d[0] = 1.5;
    d[1] = d[0] * 2;
    d[2] = d[0] + d[1];
    print_double(d[2]);
    print_double(*(d + 1));
    return 0;
}
