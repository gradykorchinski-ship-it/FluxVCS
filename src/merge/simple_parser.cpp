#include "flux/merge/semantic.hpp"
#include <regex>
#include <sstream>

namespace flux {

/**
 * Simple C++ parser using regex
 * Good enough for demonstration, can be replaced with tree-sitter later
 */
class SimpleParser : public LanguageParser {
public:
    Result<std::unique_ptr<ASTNode>> parse(std::string_view source) override {
        auto root = std::make_unique<ASTNode>();
        root->type = ASTNode::Type::UNKNOWN;
        root->name = "<root>";
        root->start_line = 0;
        root->end_line = 0;
        
        std::string src(source);
        size_t line_num = 1;
        
        // Parse functions
        parse_functions(src, *root, line_num);
        
        // Parse classes
        parse_classes(src, *root, line_num);
        
        return root;
    }
    
    std::string language() const override {
        return "C++";
    }
    
private:
    void parse_functions(const std::string& source, ASTNode& root, size_t& /*line_num*/) {
        // Regex for function definitions: return_type function_name(params) {
        std::regex func_regex(R"((\w+)\s+(\w+)\s*\([^)]*\)\s*\{)");
        
        std::sregex_iterator iter(source.begin(), source.end(), func_regex);
        std::sregex_iterator end;
        
        for (; iter != end; ++iter) {
            auto match = *iter;
            auto node = std::make_unique<ASTNode>();
            node->type = ASTNode::Type::FUNCTION;
            node->name = match[2].str();  // function name
            node->start_line = count_lines(source, 0, match.position());
            node->end_line = find_closing_brace(source, match.position() + match.length());
            
            root.children.push_back(std::move(node));
        }
    }
    
    void parse_classes(const std::string& source, ASTNode& root, size_t& /*line_num*/) {
        // Regex for class definitions: class ClassName {
        std::regex class_regex(R"(class\s+(\w+)\s*\{)");
        
        std::sregex_iterator iter(source.begin(), source.end(), class_regex);
        std::sregex_iterator end;
        
        for (; iter != end; ++iter) {
            auto match = *iter;
            auto node = std::make_unique<ASTNode>();
            node->type = ASTNode::Type::CLASS;
            node->name = match[1].str();  // class name
            node->start_line = count_lines(source, 0, match.position());
            node->end_line = find_closing_brace(source, match.position() + match.length());
            
            // Parse methods within class
            parse_methods(source, *node, match.position(), node->end_line);
            
            root.children.push_back(std::move(node));
        }
    }
    
    void parse_methods(const std::string& source, ASTNode& class_node, size_t start_pos, size_t end_line) {
        // Find methods within class body
        std::regex method_regex(R"((\w+)\s+(\w+)\s*\([^)]*\)\s*\{)");
        
        auto class_end = find_position_at_line(source, end_line);
        std::string class_body = source.substr(start_pos, class_end - start_pos);
        
        std::sregex_iterator iter(class_body.begin(), class_body.end(), method_regex);
        std::sregex_iterator end;
        
        for (; iter != end; ++iter) {
            auto match = *iter;
            auto node = std::make_unique<ASTNode>();
            node->type = ASTNode::Type::METHOD;
            node->name = match[2].str();
            node->start_line = count_lines(source, 0, start_pos + match.position());
            node->end_line = find_closing_brace(source, start_pos + match.position() + match.length());
            
            class_node.children.push_back(std::move(node));
        }
    }
    
    size_t count_lines(const std::string& str, size_t start, size_t end) {
        size_t lines = 1;
        for (size_t i = start; i < end && i < str.length(); ++i) {
            if (str[i] == '\n') lines++;
        }
        return lines;
    }
    
    size_t find_closing_brace(const std::string& source, size_t start) {
        int depth = 1;
        size_t pos = start;
        
        while (pos < source.length() && depth > 0) {
            if (source[pos] == '{') depth++;
            else if (source[pos] == '}') depth--;
            pos++;
        }
        
        return count_lines(source, 0, pos);
    }
    
    size_t find_position_at_line(const std::string& source, size_t line) {
        size_t current_line = 1;
        size_t pos = 0;
        
        while (pos < source.length() && current_line < line) {
            if (source[pos] == '\n') current_line++;
            pos++;
        }
        
        return pos;
    }
};

} // namespace flux

// Factory function (outside namespace for C linkage compatibility)
std::shared_ptr<flux::LanguageParser> create_cpp_parser() {
    return std::make_shared<flux::SimpleParser>();
}
