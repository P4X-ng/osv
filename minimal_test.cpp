#include <iostream>
#include <regex>
#include <string>

int main() {
    try {
        std::string test = "hello world";
        std::regex pattern(R"(("([^"\\]|\\.)*")|([^\s;&!]+)|(&!|;|&|!))");
        
        std::sregex_iterator iter(test.begin(), test.end(), pattern);
        std::sregex_iterator end;
        
        std::cout << "Regex compilation and iteration: SUCCESS" << std::endl;
        
        for (; iter != end; ++iter) {
            const std::smatch& match = *iter;
            std::cout << "Match: " << match.str() << std::endl;
        }
        
        return 0;
    } catch (const std::exception& e) {
        std::cout << "Error: " << e.what() << std::endl;
        return 1;
    }
}