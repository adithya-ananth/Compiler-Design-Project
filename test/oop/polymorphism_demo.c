class Base {
public:
    virtual void show() {
        printf("Base class\n");
    }
};

class Derived : Base {
public:
    void show(){
        printf("Derived class\n");
    }
};

int main() {
    Base* b;
    Derived d;

    b = &d;      // base pointer pointing to derived object
    b->show();   // runtime polymorphism

    return 0;
}