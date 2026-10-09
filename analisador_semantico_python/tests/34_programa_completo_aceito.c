int total;
int dobro(int n) {
    return n * 2;
}
void registra(int v[], int n) {
    int i;
    for (i = 0; i < n; i = i + 1) {
        if (v[i] < 0) continue;
        if (v[i] > 100) break;
        total = total + v[i];
    }
    print("total", total);
    return;
}
bool letra(char c) {
    return c >= 'a' && c <= 'z' || c == '_';
}
float media(float a, int b) {
    return (a + b) / 2;
}
int busca() {
    while (true) {
        read();
        return 1;
    }
}
int laco() {
    for (;;) {
        return 2;
    }
}
int principal() {
    int xs[3];
    int k;
    float m;
    bool ok;
    total = 0;
    xs[0] = dobro(1);
    xs[1] = -xs[0] % 3;
    xs[2] = xs[1] = 7;
    registra(xs, 3);
    m = media(1, 2);
    ok = letra('x') && !(m != 1.5) || false;
    if (ok) {
        int k;
        k = 1;
    } else {
        k = 2;
    }
    if (m > 0) return dobro(k);
    else return 0;
}
