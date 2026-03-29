int main() {
    int n = 7;
    int result = 1;
    int i = 1;

    while (i <= n) {
        result = result * i;
        i = i + 1;
    }

    printf("Iterative Factorial of %d is %d\n", n, result);
    return 0;
}