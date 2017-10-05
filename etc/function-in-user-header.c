#include "function-in-user-header.h"

int main() {
    int a;
    foo(a);
    return a;
}
