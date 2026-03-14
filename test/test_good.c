
char foo(int x, int y) {
    while (x < y) {
        x = x + 1;
    }
    int i;
    for(i=0; i< 5; i = i+1)
    {
	    int j;
	    j = i*2;
        if (j > 5) {
            break;
        }
    }
    char z;
    return z;
}

int main() {
    int a;
    a = 10;
    if (a > 5) {
        return a;
    } else {
	foo(4, a);
	//foo(4);
	//foo(4,5,6);
	//foo('a','b');
        return 0;
    }
}

