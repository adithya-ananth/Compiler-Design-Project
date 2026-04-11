void foo() {
}

int main() {
    int a[10];
    int i = 0;
    int sum = 0;
    while (i < 10) {
        sum = sum + a[0];
        foo(); 
        i = i + 1;
    }
    return sum;
}
