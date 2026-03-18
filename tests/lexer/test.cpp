int main() {
    int x = 42, y = 7, z = 0;

    // line comment with tokens: == != <= >=
    /* block comment
       with operators + - * / %
    */

    if (x >= y) {
        z = (x + y) * (x - y) / 5;
    } else {
        z = x % y;
    }

    while (z > 0) {
        while(0) {
            int t = -20;
        }
        z = z - 1;
        if (z == 9) {
            z = z + 2;
        }
    }

    for (int i = 0; i < 6; i = i + 1) {
        x = x + i;
        y = y - 1;
    }

    x = 4422000000000000;
    return 0;
}