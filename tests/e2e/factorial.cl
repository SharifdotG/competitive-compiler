int fact(int n) {
    if (n <= 1) { return 1; }
    return n * fact(n - 1);
}
int main() {
    int n;
    read(n);
    print(fact(n));
    return 0;
}
