int main() {
    int a[10];
    int x = 5;
    int i = 0;
    while (i < 10) {
        a[i] = x + 2; // x + 2 is invariant
        i = i + 1;
    }
    return a[0];
}
