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

void print_result(const std::vector<std::vector<std::string>>& result) {
    std::cout << "Result size: " << result.size() << std::endl;
    for (size_t i = 0; i < result.size(); ++i) {
        std::cout << "Command " << i << " (size " << result[i].size() << "): ";
        for (size_t j = 0; j < result[i].size(); ++j) {
            std::cout << "\"" << result[i][j] << "\"";
            if (j < result[i].size() - 1) std::cout << ", ";
        }
        std::cout << std::endl;
    }
}

bool test_parse_empty() {
    bool ok;
    auto result = parse_command_line_regex("", ok);
    return ok && result.size() == 0;
}

bool test_parse_space() {
    bool ok;
    auto result = parse_command_line_regex(" ", ok);
    return ok && result.size() == 0;
}

bool test_parse_spaces() {
    bool ok;
    auto result = parse_command_line_regex(" \t\n;", ok);
    return ok && result.size() == 0;
}

bool test_parse_simplest() {
    bool ok;
    auto result = parse_command_line_regex("mkfs.so", ok);
    return ok && result.size() == 1 && result[0].size() == 2 && 
           result[0][0] == "mkfs.so" && result[0][1] == "";
}

bool test_parse_simplest_with_args() {
    bool ok;
    auto result = parse_command_line_regex("mkfs.so --blub      --blah", ok);
    return ok && result.size() == 1 && result[0].size() == 4 &&
           result[0][0] == "mkfs.so" && result[0][1] == "--blub" && 
           result[0][2] == "--blah" && result[0][3] == "";
}

bool test_parse_simplest_with_quotes() {
    bool ok;
    auto result = parse_command_line_regex("mkfs.so  \"--blub ;  --blah\"", ok);
    return ok && result.size() == 1 && result[0].size() == 3 &&
           result[0][0] == "mkfs.so" && result[0][1] == "--blub ;  --blah" && result[0][2] == "";
}

bool test_parse_simple_multiple() {
    bool ok;
    auto result = parse_command_line_regex("mkfs.so;cpiod.so   ;   haproxy.so;", ok);
    if (!ok || result.size() != 3) return false;
    
    std::vector<std::string> expected_cmds = {"mkfs.so", "cpiod.so", "haproxy.so"};
    for (size_t i = 0; i < result.size(); i++) {
        if (result[i].size() != 2) return false;
        if (result[i][0] != expected_cmds[i]) return false;
        if (result[i][1] != ";") return false;
    }
    return true;
}

bool test_parse_multiple_with_quotes() {
    bool ok;
    auto result = parse_command_line_regex("mkfs.so;cpiod.so  \" ;; --onx -fon;x \\t\" ;   haproxy.so", ok);
    if (!ok || result.size() != 3) return false;
    
    if (result[0].size() != 2 || result[1].size() != 3 || result[2].size() != 2) return false;
    
    std::vector<std::string> expected_cmds = {"mkfs.so", "cpiod.so", "haproxy.so"};
    for (size_t i = 0; i < result.size(); i++) {
        if (result[i][0] != expected_cmds[i]) return false;
    }
    
    // Check the quoted string with escape sequence
    if (result[1][1] != " ;; --onx -fon;x \t") return false;
    
    return true;
}

int main() {
    std::cout << "Testing regex-based command parser..." << std::endl;
    
    // Test basic cases
    std::cout << "\nTest empty: " << (test_parse_empty() ? "PASS" : "FAIL") << std::endl;
    std::cout << "Test space: " << (test_parse_space() ? "PASS" : "FAIL") << std::endl;
    std::cout << "Test spaces: " << (test_parse_spaces() ? "PASS" : "FAIL") << std::endl;
    std::cout << "Test simplest: " << (test_parse_simplest() ? "PASS" : "FAIL") << std::endl;
    std::cout << "Test simplest with args: " << (test_parse_simplest_with_args() ? "PASS" : "FAIL") << std::endl;
    std::cout << "Test with quotes: " << (test_parse_simplest_with_quotes() ? "PASS" : "FAIL") << std::endl;
    std::cout << "Test simple multiple: " << (test_parse_simple_multiple() ? "PASS" : "FAIL") << std::endl;
    std::cout << "Test multiple with quotes: " << (test_parse_multiple_with_quotes() ? "PASS" : "FAIL") << std::endl;
    
    // Debug output for failing tests
    std::cout << "\nDebug output for escape sequence test:" << std::endl;
    bool ok;
    auto result = parse_command_line_regex("mkfs.so;cpiod.so  \" ;; --onx -fon;x \\t\" ;   haproxy.so", ok);
    print_result(result);
    
    return 0;
}