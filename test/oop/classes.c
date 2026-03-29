class Base {
    public:
    int pub_x;
    virtual void func(class Base this) {
    }
    private:
    int priv_x;
};

class Derived : Base {
    public:
    int derived_x;
    void func(class Derived this) {
    }
};

int main() {
    class Derived d;
    d.pub_x = 10;
    d.derived_x = 20;
    // d.priv_x = 30; // Access violation if uncommented
    d.func(); // Should call method Derived_func
    return 0;
}