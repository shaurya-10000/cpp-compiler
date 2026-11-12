// ERROR: 'continue' outside of loop
int main() {
    int n = 1;
    switch (n) {
        case 1: continue;
    }
    return 0;
}
