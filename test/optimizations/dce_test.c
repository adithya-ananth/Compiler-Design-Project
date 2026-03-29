int main() {
    int x = 10;
    int y = 20;
    int unused = 30; // Dead code
    if (0) {         // Unreachable
        unused = 40;
        x = 5;
    } else {
        y = 25;
    }
    return x + y; // Should return 35
}
