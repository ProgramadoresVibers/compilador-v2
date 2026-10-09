void g() {
    return;
}
void h() {
    return g();
}
void k() {
    return 1;
}
int principal() {
    h();
    return 0;
}
