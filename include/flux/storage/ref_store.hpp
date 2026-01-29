#pragma once

#include "flux/core/types.hpp"
#include "flux/core/object_id.hpp"
#include <optional>

namespace flux {

/**
 * RefStore - Manages references (branches, tags, HEAD)
 * 
 * References are stored in .flux/refs/
 */
class RefStore {
public:
    explicit RefStore(Path refs_dir);
    
    // Read reference value
    Result<ObjectId> read(const std::string& name);
    
    // Write reference value
    Result<void> write(const std::string& name, const ObjectId& id);
    
    // Delete reference
    Result<void> remove(const std::string& name);
    
    // Check if reference exists
    bool exists(const std::string& name);
    
    // List all references
    Result<std::vector<std::string>> list();
    
    // Read symbolic reference (e.g., HEAD)
    Result<std::string> read_symbolic(const std::string& name);
    
    // Write symbolic reference
    Result<void> write_symbolic(const std::string& name, const std::string& target);
    
private:
    Path get_ref_path(const std::string& name) const;
    
    Path refs_dir_;
};

} // namespace flux
