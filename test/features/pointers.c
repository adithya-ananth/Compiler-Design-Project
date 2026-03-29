int main(){
    int i;
    int a[11];
    int x;
    int *p;
    int y;
    x = 2;
    p = &x;  // assigning address to pointer
    y = *p;  // dereferencing pointer (R-value)
    *p = 5;  // dereferencing pointer (L-value)
    p = p + 1;  // pointer arithmetic
for(i = 1; i != 10; i=i+1)
 {
  a[i] = x * 5;                                       
 } 
 return 0;
}