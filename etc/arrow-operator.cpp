#include <vector>

struct test {
    int x;
};

void foo() {
    std::vector<test>::iterator i;
    i->x += 1;
}
