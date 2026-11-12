#define N 5
#define SQUARE(x) ((x) * (x))
#define ADD(a, b) ((a) + (b))
#define MAX(a, b) ((a) > (b) ? (a) : (b))
#define GREETING "hello\n"
#define TWICE(f) f f
#define NEST ADD(N, SQUARE(2))

#ifdef N
int n_defined() { return 1; }
#else
int n_defined() { return 0; }
#endif

#ifndef UNDEFINED_THING
#define FALLBACK 42
#endif

#if N > 3 && defined(FALLBACK)
#define BRANCH 1
#elif N > 100
#define BRANCH 2
#else
#define BRANCH 3
#endif

#define LONG_SUM(a, b, c) \
    ((a) + \
     (b) + \
     (c))

#undef N
#define N 7

/* a block comment mentioning N and
#define SHOULD_NOT_EXIST 1
still inside the comment */

int main() {
    print_int(N);
    print_int(SQUARE(3));
    print_int(SQUARE(1 + 2));
    print_int(ADD(1, SQUARE(3)));
    print_int(MAX(3, 8));
    print_int(MAX(SQUARE(2), ADD(1, 2)));
    print_str(GREETING);
    print_int(n_defined());
    print_int(FALLBACK);
    print_int(BRANCH);
    print_int(LONG_SUM(1, 2, 3));
    print_int(NEST);
    TWICE(print_int(1);)
    int l1 = __LINE__;
    int l2 = __LINE__;
    print_int(l2 - l1);
    print_str("N is N\n");
    return 0;
}
