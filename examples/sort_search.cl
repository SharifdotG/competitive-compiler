// sort_search.cl — read N ints, sort, then look up keys with binary_search.
int main() {
    int n;
    read(n);
    int a[100];
    for (int i = 0; i < n; i = i + 1) {
        read(a[i]);
    }
    sort(a, n);
    print("sorted:");
    for (int i = 0; i < n; i = i + 1) {
        print(a[i]);
    }
    int q;
    read(q);
    int idx = binary_search(a, n, q);
    if (idx >= 0) {
        print("found at index");
        print(idx);
    } else {
        print("not found");
    }
    return 0;
}
