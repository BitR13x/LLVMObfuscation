// target_str.c
#include <stdio.h>

const char* global_message = "This is a global message!";

int main() {
    const char* local_msg = "Hello, String Obfuscation!";
    printf("Msg 1: %s\n", local_msg);
    printf("Msg 2: %s\n", global_message);
    printf("Msg 3: %s\n", "Inline string literal");
    return 0;
}
