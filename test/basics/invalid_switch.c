void g() {
    /* returns void */
}

int bad_switch(int x) {

    /* invalid: switch expression has type void */
    switch (g()) {

        /* valid constant, but duplicated later */
        case 1:
            x = 1;
            break;

        /* duplicate constant case label */
        case 1:
            x = 2;
            break;

        /* invalid: case label is not a constant expression */
        case x:
            x = 3;
            break;

        /* first default */
        default:
            x = 4;
            break;

        /* invalid: multiple default labels */
        default:
            x = 5;
            break;
    }

    return x;
}

