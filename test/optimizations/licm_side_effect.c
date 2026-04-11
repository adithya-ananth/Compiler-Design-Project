int main() {
    int a[10];
    int i = 0;
    int sum = 0;
    while (i < 10) {
        sum = sum + a[0];
        a[i] = 1; 
        i = i + 1;
    }
    return sum;
}
