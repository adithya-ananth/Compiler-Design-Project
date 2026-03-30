int main() {
    int sum = 0;
    int i = 0;
    
    // We loop exactly 10 times. Unrolling by a factor of 2 means the loop will only evaluate the "i < 10" condition 5 times
    while (i < 10) {
        sum = sum + i;
        i = i + 1;
    }
    
    return sum;
}