// ERROR: case label must be an integer constant
int main() {
    int n = 1;
    switch (n) {
        case n: break;
    }
    return 0;
}
