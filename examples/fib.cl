// fib.cl — recursive Fibonacci, exercises if/else + recursion + I/O
int fib(int n) {
    if (n < 2) {
        return n;
    }
    return fib(n - 1) + fib(n - 2);
}

int main() {
    int n;
    read(n);
    print(fib(n));
    return 0;
}
