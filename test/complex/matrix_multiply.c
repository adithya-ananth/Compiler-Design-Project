int main() {
    int n; // size of the square arrays
    printf("Enter the size of the array: ");
    scanf("%d", &n);
    int A[n][n], B[n][n], C[n][n];
    int i, j, k;
    int temp_mul, temp_add;

    // --- Input for Matrix A ---
    printf("Enter elements for Matrix A (%dx%d):\n", n, n);
    for (i = 0; i < n; i++) {
        for (j = 0; j < n; j++) {
            printf("A[%d][%d]: ", i, j);
            int k;
            scanf("%d", &k);
            A[i][j] = k;
        }
    }

    // --- Input for Matrix B ---
    printf("\nEnter elements for Matrix B (%dx%d):\n", n, n);
    for (i = 0; i < n; i++) {
        for (j = 0; j < n; j++) {
            printf("B[%d][%d]: ", i, j);
            int k;
            scanf("%d", &k);
            B[i][j] = k;
        }
    }

    // --- Matrix Multiplication Logic ---
    for (i = 0; i < n; i++) {
        for (j = 0; j < n; j++) {
            C[i][j] = 0; // Initialize result cell
            for (k = 0; k < n; k++) {
                temp_mul = A[i][k] * B[k][j];
                temp_add = C[i][j] + temp_mul;
                C[i][j] = temp_add;
            }
        }
    }

    // --- Output the Result ---
    printf("\nResultant Matrix C:\n");
    for (i = 0; i < n; i++) {
        for (j = 0; j < n; j++) {
            int k = C[i][j];
            printf("%d\t", k);
        }
        printf("\n");
    }

    return 0;
}