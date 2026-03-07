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
            r = -1;
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
    return v1 + v2 + v3 + v4;
}

