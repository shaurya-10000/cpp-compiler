typedef int Int;
typedef char* String;
typedef struct Point { int x; int y; } Point;
typedef struct { int w; int h; } Size;
typedef struct Node Node;
struct Node { int value; Node* next; };
typedef Node* NodePtr;
typedef enum { LOW, HIGH = 10 } Level;
typedef Point* PointPtr;

int area(Size* s) { return s->w * s->h; }

Int add(Int a, Int b) { return a + b; }

int main() {
    Int x = add(2, 3);
    print_int(x);
    String s = "hi\n";
    print_str(s);
    Point p;
    p.x = 3;
    p.y = 4;
    PointPtr pp = &p;
    print_int(pp->x * pp->y);
    Size sz;
    sz.w = 6;
    sz.h = 7;
    print_int(area(&sz));
    Node a;
    Node b;
    a.value = 1;
    a.next = &b;
    b.value = 2;
    b.next = 0;
    NodePtr cur = &a;
    int sum = 0;
    while (cur != 0) {
        sum += cur->value;
        cur = cur->next;
    }
    print_int(sum);
    Level lv = HIGH;
    print_int(lv);
    print_int(sizeof(Point));
    print_int(sizeof(String));
    print_int((Int)3.9);
    typedef Int Counter;
    Counter k = 7;
    print_int(k);
    return 0;
}
