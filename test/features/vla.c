int main() {
    int n = 5;
    int a[n];
    int i;
    for (i = 0; i < n; i = i + 1) {
        a[i] = i * i;
    }
    int sum = 0;
    for (i = 0; i < n; i = i + 1) {
        sum = sum + a[i];
    }
    return sum; // Expected: 0+1+4+9+16 = 30
}
