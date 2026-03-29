// A tail recursive function to print numbers
void fun(int n) {
    if (n > 0) {
        //printf("%d ", n);
        // The recursive call is the last statement
        fun(n - 1); 
    }
}

// Driver Code
int main() {
    int x = 3;
    fun(x); // Output: 3 2 1
    return 0;
}