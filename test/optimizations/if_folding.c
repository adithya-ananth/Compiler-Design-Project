int main() {
    int x = 1;
    int y = 2;
    int res;

    if (x < y) {
        res = 100;
    } else {
        res = 200;
    }

    if (10 == 10) {
        res = res + 50;
    }

    if (0) {
        res = 999;
    }

    return res;
}