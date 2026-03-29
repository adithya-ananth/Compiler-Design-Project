class Base {
    private:
    int secret;
    public:
    void set(int s) {
        secret = s;
    }
};

int main() {
    Base b;
    b.set(10);
    // This should fail:
    b.secret = 20;
    return 0;
}
