// strings.cl — exercises string variables, ==, +, len, read.
int main() {
    string greeting = "Hello, ";
    string who;
    print("enter your name:");
    read(who);
    string full = greeting + who + "!";
    print(full);
    print(len(full));
    if (who == "world") {
        print("classic");
    } else {
        print("nice to meet you");
    }
    return 0;
}
