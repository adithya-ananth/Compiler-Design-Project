int fact_tail(int n, int accumulator) {
    if (n <= 1) {
        return accumulator;
    }
    // Tail recursion: The function call is the ABSOLUTE LAST operation.
    // The optimizer will flag this, destroy the stack frame early, and use a 'jump' 
    // instead of a 'call', running in O(1) memory instead of O(n).
    return fact_tail(n - 1, n * accumulator);
}

int main() {
    int result = fact_tail(7, 1);
    printf("Tail-Recursive Factorial is %d\n", result);
    return 0;
}