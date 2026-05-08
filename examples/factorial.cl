// factorial.cl — recursive + iterative factorial, exercises recursion and loops.
int fact_rec(int n) {
    if (n <= 1) {
        return 1;
    }
    return n * fact_rec(n - 1);
}

int fact_iter(int n) {
    int p = 1;
    for (int i = 2; i <= n; i = i + 1) {
        p = p * i;
    }
    return p;
}

int main() {
    int n;
    read(n);
    print(fact_rec(n));
    print(fact_iter(n));
    return 0;
}
