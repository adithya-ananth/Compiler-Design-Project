int main() {
    int n;
    // get size of array
    printf("Enter size of array: ");
    scanf("%d", &n);
    int a[n];
    int i;
    for (i = 0; i < n; i=i+1) {
        a[i] = (i+1) * (i+1);
    }
    printf("Array contents:\n");
    for (i = 0; i < n; i=i+1) {
        printf("%d ", a[i]);
    }
    printf("\n");
    // the sum of the array
    int sum = 0;
    for (i = 0; i < n; i=i+1) {
        sum = sum + a[i];
    }
    printf("Sum of array: %d\n", sum);
    return 0;
}
