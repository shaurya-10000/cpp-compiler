// ERROR: duplicate case value 1
int main() {
    int n = 1;
    switch (n) {
        case 1: break;
        case 1: break;
    }
    return 0;
}
