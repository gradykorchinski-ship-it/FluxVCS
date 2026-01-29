#include "flux/merge/semantic.hpp"
#include <algorithm>
#include <map>
#include <set>
#include <functional>

namespace flux {

// Helper function to describe AST nodes
static std::string get_node_description(const ASTNode& node) {
    switch (node.type) {
        case ASTNode::Type::FUNCTION:
            return "Function '" + node.name + "'";
        case ASTNode::Type::CLASS:
            return "Class '" + node.name + "'";
        case ASTNode::Type::METHOD:
            return "Method '" + node.name + "'";
        case ASTNode::Type::VARIABLE:
            return "Variable '" + node.name + "'";
        default:
            return "'" + node.name + "'";
    }
}

SemanticDiffer::SemanticDiffer(std::shared_ptr<LanguageParser> parser)
    : parser_(std::move(parser)) {}

Result<SemanticDiff> SemanticDiffer::diff(
    std::string_view old_source,
    std::string_view new_source
) {
    auto old_tree_result = parser_->parse(old_source);
    if (!old_tree_result) {
        return flux::unexpected(old_tree_result.error());
    }
    
    auto new_tree_result = parser_->parse(new_source);
    if (!new_tree_result) {
        return flux::unexpected(new_tree_result.error());
    }
    
    return diff_trees(**old_tree_result, **new_tree_result);
}

SemanticDiff SemanticDiffer::diff_trees(const ASTNode& old_tree, const ASTNode& new_tree) {
    SemanticDiff result;
    
    std::map<std::string, const ASTNode*> old_nodes;
    std::map<std::string, const ASTNode*> new_nodes;
    
    // Lambda to collect nodes
    std::function<void(const ASTNode&, const std::string&, std::map<std::string, const ASTNode*>&)> collect_nodes;
    collect_nodes = [&](const ASTNode& node, const std::string& path, std::map<std::string, const ASTNode*>& nodes) {
        std::string full_path = path.empty() ? node.name : path + "." + node.name;
        nodes[full_path] = &node;
        
        for (const auto& child : node.children) {
            collect_nodes(*child, full_path, nodes);
        }
    };
    
    collect_nodes(old_tree, "", old_nodes);
    collect_nodes(new_tree, "", new_nodes);
    
    // Find additions
    for (const auto& [path, node] : new_nodes) {
        if (old_nodes.find(path) == old_nodes.end()) {
            SemanticDiff::Change change;
            change.type = SemanticDiff::ChangeType::ADDED;
            change.path = path;
            change.new_line = node->start_line;
            change.description = get_node_description(*node) + " added";
            result.changes.push_back(change);
        }
    }
    
    // Find deletions and modifications
    for (const auto& [path, old_node] : old_nodes) {
        auto it = new_nodes.find(path);
        
        if (it == new_nodes.end()) {
            SemanticDiff::Change change;
            change.type = SemanticDiff::ChangeType::REMOVED;
            change.path = path;
            change.old_line = old_node->start_line;
            change.description = get_node_description(*old_node) + " removed";
            result.changes.push_back(change);
        } else {
            const ASTNode* new_node = it->second;
            if (old_node->start_line != new_node->start_line ||
                old_node->end_line != new_node->end_line) {
                SemanticDiff::Change change;
                change.type = SemanticDiff::ChangeType::MODIFIED;
                change.path = path;
                change.old_line = old_node->start_line;
                change.new_line = new_node->start_line;
                change.description = get_node_description(*old_node) + " modified";
                result.changes.push_back(change);
            }
        }
    }
    
    return result;
}

} // namespace flux
