class Base {
public:
    virtual void show() {
        //cout << "Base class\n";
    }
};

class Derived : Base {
public:
    void show(){
        //cout << "Derived class\n";
    }
};

int main() {
    Base* b;
    Derived d;

    b = &d;      // base pointer pointing to derived object
    b->show();   // runtime polymorphism

    return 0;
}