#include <iostream>

namespace {
  class Foo {
  private: 
    int var;
  public:
    Foo(int var) : var(var) {}
    int returnSomething() { return var*2; }
  };
}

int main(int argc, char** argv) {
  Foo foo( argc );
  int i = foo.returnSomething();
  std::cout << i << std::endl;
  return 0;
}
