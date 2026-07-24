#include "dotline/include/dotline.hpp"
#include <iostream>

int main()
{
    int x = 0;
    auto ps = dotl::prompt("-> ").read_string();
    auto res = ps.parse<int>();
    if (res.error()) std::cout << "Warning: integer parsing failed!\n";
    else {
        x = res.value();
    }

    std::cout << "input: '" << ps.string() << "'\n";
    std::cout << "value: '" << x << "'\n";
}
