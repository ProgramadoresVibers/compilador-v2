void g() {
    return;
}
int principal() {
    int x = g();
    x = 1 + g();
    print(g());
    if (g()) x = 2;
    return g();
}
