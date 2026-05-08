// circle.cl — float arithmetic; computes area and circumference of a circle.
int main() {
    float pi = 3.14159265;
    float r;
    read(r);
    float area = pi * r * r;
    float circumference = 2.0 * pi * r;
    print(area);
    print(circumference);
    return 0;
}
