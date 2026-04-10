int main() {
    int a[20];
    int n = 20;
    a[0] = 64; a[1] = 34; a[2] = 25; a[3] = 12; a[4] = 22;
    a[5] = 11; a[6] = 90; a[7] = 88; a[8] = 76; a[9] = 54;
    a[10] = 32; a[11] = 16; a[12] = 8; a[13] = 4; a[14] = 2;
    a[15] = 1; a[16] = 0; a[17] = -1; a[18] = -23; a[19] = -32;

    int i;
    int j;
    int temp;

    for (i = 0; i < n - 1; i = i + 1) {
        for (j = 0; j < n - i - 1; j = j + 1) {
            if (a[j] > a[j + 1]) {
                temp = a[j];
                a[j] = a[j + 1];
                a[j + 1] = temp;
            }
        }
    }

    printf("Sorted array: ");
    for (i = 0; i < n; i = i + 1) {
        printf("%d ", a[i]);
    }
    printf("\n");

    return 0;
}
