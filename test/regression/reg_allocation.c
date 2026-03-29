int helper(int x) {
    return x + 1;
}

int main() {
    int i = 0;
    int sum = 0;
    for (i = 0; i < 10; i = i + 1) {
        sum = sum + helper(i);
    }
    return sum; // 1+2+3+4+5+6+7+8+9+10 = 55
}
