int main() {
    int i;
    i = 0;

    while (i < 5) {
        i = i + 1;
        if (i == 3) continue;
        i = i + 10;
    }

    int j;
    for (j = 0; j < 5; j = j + 1) {
        if (j == 2) continue;
        i = i + j;
    }

    return i + j;
}
