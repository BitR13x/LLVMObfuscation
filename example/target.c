// A simple example program to test the MBA Obfuscation Pass
#include <stdio.h>

int calculate_sum(int a, int b) {
    return a + b;
}

int calculate_diff(int a, int b) {
    return a - b;
}

int reminder(int a, int b) {
    return a % b;
}

int main() {
    int x = 15;
    int y = 27;

    int sum = calculate_sum(x, y);
    int diff = calculate_diff(x, y);
    int rem = reminder(y, x);


    printf("Sum: %d\n", sum);
    printf("Diff: %d\n", diff);
    printf("Reminder: %d\n", rem);

    return 0;
}
