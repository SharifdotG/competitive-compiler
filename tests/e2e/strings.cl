int main() {
    string s = "hello";
    string t = "world";
    print(s);
    print(t);
    string u = s + " " + t;
    print(u);
    print(len(u));
    if (s == "hello") {
        print("eq");
    }
    if (s != t) {
        print("neq");
    }
    return 0;
}
