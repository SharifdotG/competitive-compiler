int main() {
    int sum = 0;
    for (int i = 1; i <= 10; i = i + 1) {
        sum += i;
    }
    print(sum);

    int x = 0;
    while (x < 5) {
        x = x + 1;
        if (x == 3) { continue; }
        if (x == 5) { break; }
        print(x);
    }
    return 0;
}
