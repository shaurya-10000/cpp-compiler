struct Point { int x; int y; };
struct Pair { struct Point a; struct Point b; char tag; };

struct Point g;

struct Point make(int x, int y) {
    struct Point p;
    p.x = x;
    p.y = y;
    return p;
}

struct Point add(struct Point a, struct Point b) {
    struct Point r;
    r.x = a.x + b.x;
    r.y = a.y + b.y;
    return r;
}

void bump(struct Point p) {
    p.x = p.x + 100;
    print_int(p.x);
}

int sum(struct Point p) { return p.x + p.y; }

struct Point swap(struct Point p) {
    struct Point r;
    r.x = p.y;
    r.y = p.x;
    return r;
}

struct Point ident(struct Point p) { return p; }

struct Pair wrap(struct Point a, struct Point b) {
    struct Pair q;
    q.a = a;
    q.b = b;
    q.tag = 'Q';
    return q;
}

int many(int a, int b, int c, int d, int e, int f, struct Point p, int h) {
    return a + b + c + d + e + f + p.x + p.y + h;
}

int main() {
    struct Point a = make(1, 2);
    struct Point b = a;
    b.x = 10;
    print_int(a.x);
    print_int(b.x);
    struct Point c = add(a, b);
    print_int(c.x);
    print_int(c.y);
    bump(a);
    print_int(a.x);
    print_int(sum(make(5, 6)));
    print_int(add(make(1, 1), make(2, 2)).y);
    struct Pair q = wrap(a, b);
    print_int(q.a.x);
    print_int(q.b.x);
    print_char(q.tag);
    print_char(10);
    g = c;
    print_int(g.x + g.y);
    struct Point arr[2];
    arr[0] = a;
    arr[1] = make(7, 8);
    print_int(arr[1].x + arr[0].y);
    struct Point *pp = &arr[1];
    struct Point d = *pp;
    d.y = 80;
    print_int(arr[1].y);
    print_int(d.y);
    *pp = d;
    print_int(arr[1].y);
    a = b = c;
    print_int(a.x);
    print_int(many(1, 2, 3, 4, 5, 6, make(10, 20), 7));
    print_int((a = make(3, 4)).x);
    struct Point e = (a.x > 5) ? a : b;
    print_int(e.x);
    print_int(swap(make(1, 2)).x);
    print_int(ident(c).y);
    return 0;
}
