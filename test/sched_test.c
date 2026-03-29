int main() {
    int a[10];
    int b, c, d, e, f, g;
    a[0] = 1;
    a[1] = 2;
    b = a[0]; // Load 1
    c = a[1]; // Load 2
    d = 10 + 20;
    e = 30 + 40;
    f = d + e;
    g = b + c;
    return f + g;
}
