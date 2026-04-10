int main() {
    int x = 10;
    int y = 5;
    
    printf("Testing increment operators:\n");
    printf("x = %d\n", x);
    printf("++x = %d\n", ++x); // should be 11
    printf("x++ = %d\n", x++); // should be 11
    printf("x = %d\n", x);     // should be 12
    
    printf("\nTesting decrement operators:\n");
    printf("y = %d\n", y);
    printf("--y = %d\n", --y); // should be 4
    printf("y-- = %d\n", y--); // should be 4
    printf("y = %d\n", y);     // should be 3

    int i = 5;

    printf("\nTesting for loop with declaration:\n");
    for (int i = 0; i < 5; i++) {
        printf("i = %d\n", i);
    }
    
    // Check if i is out of scope (uncommenting should cause semantic error)
    // printf("i after loop = %d\n", i); 

    printf("\nTesting nested for loops with same variable names:\n");
    for (int i = 0; i < 2; i++) {
        for (int i = 0; i < 3; i++) {
            printf("inner i = %d\n", i);
        }
        printf("outer i = %d\n", i);
    }

    printf("\nTesting complex for loop condition:\n");
    int limit = 2;
    for (int i = 0; i < 2 + limit; i++) {
        printf("complex i = %d\n", i);
    }

    printf("Global i = %d\n", i);
    return 0;
}
