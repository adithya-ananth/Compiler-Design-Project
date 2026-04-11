int fact(int n) {
    if (n <= 1) {
        return 1;
    }

    return n * fact(n - 1);
}

int main() {
    printf("Enter a positive integer: ");
    int n;
    scanf("%d", &n);
    int result = fact(n);
    printf("Recursive Factorial is %d\n", result);
    return 0;
}