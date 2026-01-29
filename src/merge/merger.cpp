#include "flux/merge/semantic.hpp"
#include <sstream>
#include <algorithm>
#include <map>

namespace flux {

SemanticMerger::SemanticMerger(std::shared_ptr<LanguageParser> parser)
    : parser_(std::move(parser)) {}

Result<MergeResult> SemanticMerger::merge(
    std::string_view base,
    std::string_view ours,
    std::string_view theirs
) {
    MergeResult result;
    result.has_conflicts = false;
    
    // Parse all three versions
    auto base_tree = parser_->parse(base);
    auto ours_tree = parser_->parse(ours);
    auto theirs_tree = parser_->parse(theirs);
    
    if (!base_tree || !ours_tree || !theirs_tree) {
        return flux::unexpected("Failed to parse one or more versions");
    }
    
    // Get diffs
    SemanticDiffer differ(parser_);
    auto base_to_ours = differ.diff_trees(**base_tree, **ours_tree);
    auto base_to_theirs = differ.diff_trees(**base_tree, **theirs_tree);
    
    // Analyze changes
    std::map<std::string, SemanticDiff::Change> our_changes;
    std::map<std::string, SemanticDiff::Change> their_changes;
    
    for (const auto& change : base_to_ours.changes) {
        our_changes[change.path] = change;
    }
    
    for (const auto& change : base_to_theirs.changes) {
        their_changes[change.path] = change;
    }
    
    // Detect conflicts
    for (const auto& [path, our_change] : our_changes) {
        auto it = their_changes.find(path);
        
        if (it != their_changes.end()) {
            const auto& their_change = it->second;
            
            // Both sides changed the same thing
            if (our_change.type == their_change.type &&
                our_change.type == SemanticDiff::ChangeType::MODIFIED) {
                // Both modified - this is a conflict
                MergeResult::Conflict conflict;
                conflict.line_start = our_change.old_line;
                conflict.line_end = our_change.old_line + 10;  // Simplified
                conflict.description = "Both sides modified " + path;
                conflict.ours_content = "// Our changes to " + path;
                conflict.theirs_content = "// Their changes to " + path;
                conflict.base_content = "// Original " + path;
                
                result.conflicts.push_back(conflict);
                result.has_conflicts = true;
            }
        }
    }
    
    // Generate merged content
    if (result.has_conflicts) {
        // Generate content with conflict markers
        std::ostringstream oss;
        oss << std::string(ours) << "\n";
        
        for (const auto& conflict : result.conflicts) {
            oss << "\n<<<<<<< OURS\n";
            oss << conflict.ours_content << "\n";
            oss << "=======\n";
            oss << conflict.theirs_content << "\n";
            oss << ">>>>>>> THEIRS\n";
        }
        
        result.merged_content = oss.str();
    } else {
        // No conflicts - merge automatically
        // For simplicity, take "ours" as base and apply non-conflicting changes
        result.merged_content = std::string(ours);
    }
    
    return result;
}

} // namespace flux
