int main(){

int A[2][2];
int B[2][2];
int C[2][2];
int i;
int j;
int k;
int temp_mul;
int temp_add;

for (i = 0; i < 2; i=i+1) {
    for (j = 0; j < 2; j=j+1) {
        C[i][j] = 0;
        for (k = 0; k < 2; k=k+1) {
            temp_mul = A[i][k] * B[k][j];
            temp_add = C[i][j] + temp_mul;
            C[i][j] = temp_add;
        }
    }
}

return 0;
}