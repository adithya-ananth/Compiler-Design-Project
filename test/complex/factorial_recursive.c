int fact(int n) {
    if (n <= 1) {
        return 1;
    }

    return n * fact(n - 1);
}

int main() {
    int result = fact(7);
    printf("Recursive Factorial is %d\n", result);
    return 0;
}