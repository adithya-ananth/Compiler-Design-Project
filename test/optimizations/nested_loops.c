int main() {
    int i;
    int j;
    int k;
    int sum;
    sum = 0;

    for (i = 0; i < 3; i = i + 1) {
        j = 0;
        while (j < 3) {
            j = j + 1;
            if (i == j) {
                continue;
            }
            for (k = 0; k < 2; k = k + 1) {
                if (k == 1) {
                    break;
                }
                sum = sum + i + j + k;
            }
        }
    }
    return sum;
}
