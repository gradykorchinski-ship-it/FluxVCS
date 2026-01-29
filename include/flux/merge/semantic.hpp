#pragma once

#include "flux/core/types.hpp"
#include "flux/core/object_id.hpp"
#include <vector>
#include <memory>

namespace flux {

/**
 * AST node for semantic diff/merge
 */
struct ASTNode {
    enum class Type {
        FUNCTION,
        CLASS,
        METHOD,
        VARIABLE,
        STATEMENT,
        EXPRESSION,
        UNKNOWN
    };
    
    Type type;
    std::string name;
    size_t start_line;
    size_t end_line;
    std::vector<std::unique_ptr<ASTNode>> children;
};

/**
 * Language parser interface
 */
class LanguageParser {
public:
    virtual ~LanguageParser() = default;
    
    virtual Result<std::unique_ptr<ASTNode>> parse(std::string_view source) = 0;
    virtual std::string language() const = 0;
};

/**
 * Semantic diff between two ASTs
 */
struct SemanticDiff {
    enum class ChangeType {
        ADDED,
        REMOVED,
        MODIFIED,
        MOVED,
        RENAMED
    };
    
    struct Change {
        ChangeType type;
        std::string path;  // e.g., "MyClass.myMethod"
        size_t old_line;
        size_t new_line;
        std::string description;
    };
    
    std::vector<Change> changes;
};

/**
 * Semantic diff engine
 */
class SemanticDiffer {
public:
    explicit SemanticDiffer(std::shared_ptr<LanguageParser> parser);
    
    Result<SemanticDiff> diff(
        std::string_view old_source,
        std::string_view new_source
    );
    
    SemanticDiff diff_trees(const ASTNode& old_tree, const ASTNode& new_tree);
    
private:
    std::shared_ptr<LanguageParser> parser_;
};

/**
 * Merge result
 */
struct MergeResult {
    bool has_conflicts;
    std::string merged_content;
    
    struct Conflict {
        size_t line_start;
        size_t line_end;
        std::string base_content;
        std::string ours_content;
        std::string theirs_content;
        std::string description;
    };
    
    std::vector<Conflict> conflicts;
};

/**
 * Semantic three-way merge
 */
class SemanticMerger {
public:
    explicit SemanticMerger(std::shared_ptr<LanguageParser> parser);
    
    Result<MergeResult> merge(
        std::string_view base,
        std::string_view ours,
        std::string_view theirs
    );
    
private:
    std::shared_ptr<LanguageParser> parser_;
};

} // namespace flux
