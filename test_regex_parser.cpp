#include <iostream>
#include <vector>
#include <string>
#include <regex>

// Copy the parser functions from commands.cc for testing
std::string process_escape_sequences(const std::string& input) {
    std::string result;
    result.reserve(input.size());
    
    for (size_t i = 0; i < input.size(); ++i) {
        if (input[i] == '\\' && i + 1 < input.size()) {
            switch (input[i + 1]) {
                case 'a': result += '\a'; i++; break;
                case 'b': result += '\b'; i++; break;
                case 'f': result += '\f'; i++; break;
                case 'n': result += '\n'; i++; break;
                case 'r': result += '\r'; i++; break;
                case 't': result += '\t'; i++; break;
                case 'v': result += '\v'; i++; break;
                case '\\': result += '\\'; i++; break;
                case '\'': result += '\''; i++; break;
                case '"': result += '"'; i++; break;
                default: result += input[i]; break; // Keep backslash if not recognized
            }
        } else {
            result += input[i];
        }
    }
    return result;
}

std::vector<std::vector<std::string>>
parse_command_line_regex(const std::string& line, bool& ok) {
    std::vector<std::vector<std::string>> result;
    ok = true;
    
    // Lines with only {blank char or ;} are ignored.
    if (std::string::npos == line.find_first_not_of(" \f\n\r\t\v;")) {
        return result;
    }
    
    // Regex pattern to match: quoted strings, unquoted strings, and delimiters
    std::regex token_regex(R"(("([^"\\]|\\.)*")|([^\s;&!]+)|(&!|;|&|!))");
    
    std::vector<std::string> current_command;
    std::sregex_iterator iter(line.begin(), line.end(), token_regex);
    std::sregex_iterator end;
    
    for (; iter != end; ++iter) {
        const std::smatch& match = *iter;
        
        if (match[1].matched) {
            // Quoted string - remove quotes and process escape sequences
            std::string quoted = match[1].str();
            std::string content = quoted.substr(1, quoted.length() - 2); // Remove quotes
            current_command.push_back(process_escape_sequences(content));
        } else if (match[3].matched) {
            // Unquoted string - process escape sequences
            current_command.push_back(process_escape_sequences(match[3].str()));
        } else if (match[4].matched) {
            // Command delimiter - add it to current command and finish the command
            if (!current_command.empty()) {
                current_command.push_back(match[4].str());
                result.push_back(current_command);
                current_command.clear();
            }
        }
    }
    
    // Add the last command if it exists
    if (!current_command.empty()) {
        // If no delimiter was found, add empty string as terminator
        current_command.push_back("");
        result.push_back(current_command);
    }
    
    return result;
}

int main() {
    bool ok;
    
    // Test 1: Simple command
    auto result = parse_command_line_regex("mkfs.so", ok);
    std::cout << "Test 1 - 'mkfs.so': ";
    if (ok && result.size() == 1 && result[0].size() == 2 && 
        result[0][0] == "mkfs.so" && result[0][1] == "") {
        std::cout << "PASS" << std::endl;
    } else {
        std::cout << "FAIL" << std::endl;
    }
    
    // Test 2: Multiple commands with semicolons
    result = parse_command_line_regex("mkfs.so;cpiod.so;", ok);
    std::cout << "Test 2 - 'mkfs.so;cpiod.so;': ";
    if (ok && result.size() == 2 && 
        result[0].size() == 2 && result[0][0] == "mkfs.so" && result[0][1] == ";" &&
        result[1].size() == 2 && result[1][0] == "cpiod.so" && result[1][1] == ";") {
        std::cout << "PASS" << std::endl;
    } else {
        std::cout << "FAIL" << std::endl;
    }
    
    return 0;
}