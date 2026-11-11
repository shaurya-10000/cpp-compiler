struct Item { int id; double price; };
struct Item inventory[3];
struct Item best;
int count = 0;

void add(int id, double price) {
    inventory[count].id = id;
    inventory[count].price = price;
    count++;
}

int main() {
    add(1, 9.5);
    add(2, 3.25);
    add(3, 7.0);
    best.id = 0;
    best.price = 0.0;
    for (int i = 0; i < count; i++) {
        if (inventory[i].price > best.price) {
            best.id = inventory[i].id;
            best.price = inventory[i].price;
        }
    }
    print_int(best.id);
    print_double(best.price);
    print_int(count);
    struct Item* p = &best;
    p->price *= 2;
    print_double(best.price);
    return 0;
}
