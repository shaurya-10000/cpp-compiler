struct Point { int x; int y; };
struct Rect { struct Point min; struct Point max; char tag; };
struct Node { int value; struct Node* next; };

int area(struct Rect* r) {
    return (r->max.x - r->min.x) * (r->max.y - r->min.y);
}
void move(struct Point* p, int dx, int dy) {
    p->x += dx;
    p->y += dy;
}

int main() {
    struct Point p;
    p.x = 3;
    p.y = 4;
    print_int(p.x * p.x + p.y * p.y);
    move(&p, 10, 20);
    print_int(p.x);
    print_int(p.y);

    struct Rect r;
    r.min.x = 1; r.min.y = 2;
    r.max.x = 6; r.max.y = 9;
    r.tag = 'R';
    print_int(area(&r));
    print_char(r.tag);
    print_char('\n');
    print_int(sizeof(struct Point));
    print_int(sizeof(struct Rect));

    struct Node a;
    struct Node b;
    struct Node c;
    a.value = 1; b.value = 2; c.value = 3;
    a.next = &b; b.next = &c; c.next = 0;
    int sum = 0;
    struct Node* n = &a;
    while (n) { sum += n->value; n = n->next; }
    print_int(sum);

    struct Point pts[3];
    for (int i = 0; i < 3; i++) { pts[i].x = i; pts[i].y = i * 10; }
    print_int(pts[2].x + pts[2].y);
    struct Point* pp = &pts[1];
    print_int(pp->y);
    pp = pp + 1;
    print_int(pp->y);
    print_int(pp - pts);
    return 0;
}
