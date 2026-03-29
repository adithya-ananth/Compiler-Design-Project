struct S { int a; char b; };
int main() {
    struct S x;
    x.a = 5;
    x.b = 'c';
    return x.a;
}
