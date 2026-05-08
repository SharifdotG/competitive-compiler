int main() {
    int n;
    read(n);
    int a[100];
    for (int i = 0; i < n; i = i + 1) {
        read(a[i]);
    }
    sort(a, n);
    int q;
    read(q);
    int idx = binary_search(a, n, q);
    if (idx >= 0) {
        print("found");
        print(idx);
    } else {
        print("missing");
    }
    return 0;
}
