int helper(int a, int b) {
    int i = 0, sum = 0;

    for (i = 0; i < a + b; i = i + 1) {
        if (i % 2 == 0) {
            sum = sum + i;
        } else {
            sum = sum - 1;
        }
    }

    while (sum > 10) {
        sum = sum - 3;
    }

    return sum;
}

int main() {
    int x = 3, y = 4, z = 0;

    {
        int t = x * y + (x - y) % 2;
        z = t;
    }

    for (int j = 0; j < 5; j = j + 1) {
        x = x + j;
    }

    if (x >= y) {
        x = x - y;
    } else {
        x = x + y;
    }

    return x + z;
}
