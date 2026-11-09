int main() {
    print_str("Testing loops:\n");
    for (int i = 0; i < 5; i = i + 1) {
        if (i == 2) { continue; }
        if (i == 4) { break; }
        print_int(i);
    }
    return 0;
}