int counter = 10;
int total;
double rate = 2.5;
char letter = 'A';
bool flag = true;
char* greeting = "hi there";
int table[5] = {1, 2, 3};
int sq[4];
int neg = -7;

void bump() { counter = counter + 1; }
int next_id() { total = total + 1; return total; }

int main() {
    print_int(counter);
    bump();
    bump();
    print_int(counter);
    print_int(next_id());
    print_int(next_id());
    print_double(rate * 2);
    print_char(letter);
    print_char('\n');
    print_int(flag);
    print_str(greeting);
    print_char('\n');
    print_int(table[0] + table[1] + table[2] + table[3] + table[4]);
    for (int i = 0; i < 4; i = i + 1) { sq[i] = i * i; }
    print_int(sq[3]);
    print_int(neg);
    letter = 'B';
    print_char(letter);
    print_char('\n');
    int* p = table;
    print_int(p[2]);
    return 0;
}
