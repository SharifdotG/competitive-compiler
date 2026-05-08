// optimization_demo.cl — illustrates the optimizer phase.
//   - 2*3+4 folds to 10
//   - x*1+0 reduces to x
//   - x-x folds to 0
//   - q*0 folds to 0
//   - some intermediate temps die.
int main() {
    int x = 2 * 3 + 4;
    int y = x * 1 + 0;
    int z = x - x;
    int q;
    read(q);
    int r = q * 0;
    print(x);
    print(y);
    print(z);
    print(r);
    return 0;
}
