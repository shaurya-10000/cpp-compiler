// ERROR: multiple 'default' labels
int main() {
    int n = 1;
    switch (n) {
        default: break;
        default: break;
    }
    return 0;
}
