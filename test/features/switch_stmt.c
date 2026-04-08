int classify_int(int x) {
    int y = 0;

    switch (x) {
        case 0:
            y = 100;
            break;

        case 1:
            y = 200;
            /* fall through on purpose */
        case 2:
            y = y + 1;
            break;

        default:
            y = -1;
            break;
    }

    return y;
}

int classify_char(char c) {
    int r = 0;

    switch (c) {
        case 'a':
            r = 1;
            break;

        case 'b':
        case 'c':
            r = 2;
            break;

        default:
            r = -2;
            break;
    }

    return r;
}

int main() {
    int v1 = classify_int(1);
    int v2 = classify_int(2);
    int v3 = classify_char('a');
    int v4 = classify_char('z');

    /* Just combine to make the value depend on all switches */
    int v5 = v1 + v2 + v3 + v4;
    printf("v1 = %d\n", v1); // expected: 200
    printf("v2 = %d\n", v2); // expected: 201 
    printf("v3 = %d\n", v3); // expected: 1
    printf("v4 = %d\n", v4); // expected: -1
    printf("v5 = %d\n", v5); // expected: 202 (200 + 1 + 1 + -1)
    return 0;
}

