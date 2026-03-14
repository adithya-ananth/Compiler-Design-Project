struct S { int a; };
int main() {
    struct S s;
    struct S *p;
    p = &s;
    p->a = 7;
    return p->a;
}
