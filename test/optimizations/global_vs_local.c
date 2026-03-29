int main() {
    int x = 10;
    int y = 20;
    int z;

    // Local
    // A local optimizer can see that z is 30.
    // It can fold (10 + 20) and propagate the constant.
    z = x + y; 

    if (z > 50) {
        // Global
        // A local optimizer only looks inside this block and sees "a = 100".
        // A GLOBAL optimizer sees that "z" is always 30, so "z > 50" is always FALSE.
        // It will delete this entire block as UNREACHABLE.
        int a = 100;
        printf("%d", a);
    }

    // A local optimizer won't delete "z = 30" because it doesn't know if 
    // z is used later in other blocks.
    // A GLOBAL optimizer (DCE) sees z is never used again and deletes it.
    return 0;
}