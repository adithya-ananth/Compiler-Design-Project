
int MAX = 100;

class Stack {
private:
    int arr[MAX]; // Array to store elements
    int top;      // Index of the top element

public:
    // Constructor: Initialize top to -1 (empty stack)
    Stack() {
        top = -1;
    }

    // Push: Add an element to the top
    int push(int x) {
        if (top >= (MAX - 1)) {
            printf("Stack overflow!\n");
            return 0;
        } else {
            arr[++top] = x;
            printf("%d pushed into the stack\n", x);
            return 1;
        }
    }

    // Pop: Remove the top element
    int pop() {
        if (top < 0) {
            printf("Stack underflow!\n");
            return 0;
        } else {
            int x = arr[top--];
            return x;
        }
    }

    // Peek: View the top element without removing it
    int peek() {
        if (top < 0) {
            printf("Stack is Empty\n");
            return 0;
        } else {
            return arr[top];
        }
    }

    // IsEmpty: Check if the stack is empty
    int isEmpty() {
        return (top < 0);
    }
};

int main() {
    Stack s;
    s.push(10);
    s.push(20);
    s.push(30);

    int x= s.pop();
    printf("%d popped from the stack", x);
    printf("The top element is %d", s.peek());
    
    return 0;
}