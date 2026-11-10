int classify(int x) {
    if (x < 0) { return -1; }
    else if (x == 0) { return 0; }
    else { return 1; }
}
int main() {
    print_int(classify(-5));
    print_int(classify(0));
    print_int(classify(7));
    return 0;
}
