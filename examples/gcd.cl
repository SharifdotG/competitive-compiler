// gcd.cl — read two ints, print their gcd and lcm.
int main() {
    int a;
    int b;
    read(a);
    read(b);
    print(gcd(a, b));
    print(lcm(a, b));
    return 0;
}
