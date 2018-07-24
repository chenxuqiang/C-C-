#include <iostream>

int main(void)
{
    int n = 0;
    int m = 0;
    int i = 1;
    int j = 1;
    n = ++i + i++;
    m = ++j + j;

    std::cout << m << std::endl;
    std::cout << n << std::endl;

    return 0;
}