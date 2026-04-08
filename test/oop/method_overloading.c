class Calculator {
    private:
    int x;
    public:
    Calculator(){
        printf("Calculator created\n");
        x = 8;
    }
    int add(int x, int y) {
        return x + y;
    }
    int add(int x) {
        return x + 1;
    }
};

int main() {
    Calculator c;
    int a = c.add(10, 20);
    int b = c.add(5);
    printf("a = %d\n", a);
    printf("b = %d\n", b);
    return 0;
}