int main() {
    int x = 10 + 20; // Constant folding: x = 30
    int y = x + 5;   // Constant propagation: y = 30 + 5 -> 35
    int z = y * 1;   // Peephole: z = y -> 35
    int a = z + 0;   // Peephole: a = z -> 35
    int b = a * 2;   // Peephole: b = 70
    return b;
}
