#include <iostream>
#include <regex>
#include <string>

int main() {
    std::string test = "hello world";
    std::regex pattern("hello");
    
    if (std::regex_search(test, pattern)) {
        std::cout << "Regex works!" << std::endl;
        return 0;
    } else {
        std::cout << "Regex failed!" << std::endl;
        return 1;
    }
}