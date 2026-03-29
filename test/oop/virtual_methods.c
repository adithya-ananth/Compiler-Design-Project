struct S {
    int x;
    virtual void func(struct S this) {
        // dummy
    }
};

int main() {
    struct S s;
    return 0;
}