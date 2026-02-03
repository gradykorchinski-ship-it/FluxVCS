#include <gtest/gtest.h>
#include "flux/core/repository.hpp"
#include "flux/storage/object_store.hpp"
#include "flux/storage/packfile.hpp"
#include "flux/util/filesystem.hpp"
#include <vector>
#include <string>

#include <filesystem>

namespace flux {

class PackIntegrationTest : public ::testing::Test {
protected:
    void SetUp() override {
        test_dir_ = std::filesystem::current_path() / "test_repo_pack";
        if (std::filesystem::exists(test_dir_)) {
            std::filesystem::remove_all(test_dir_);
        }
        std::filesystem::create_directories(test_dir_);
        
        auto repo_result = Repository::init(test_dir_);
        ASSERT_TRUE(repo_result.has_value());
    }

    void TearDown() override {
        std::filesystem::remove_all(test_dir_);
    }

    Path test_dir_;
};

TEST_F(PackIntegrationTest, WritePackRead) {
    auto repo_result = Repository::open(test_dir_);
    ASSERT_TRUE(repo_result.has_value());
    auto& repo = **repo_result;
    
    // 1. Write an object
    std::string content = "test content for packing";
    std::vector<uint8_t> data(content.begin(), content.end());
    
    auto store = repo.object_store(); // Assuming access to store, or we construct one
    // Note: Repository might not expose mutable object_store easily, 
    // let's construct one manually for testing if needed, or use repo's internal store if accessible.
    // Looking at repository.hpp might be needed, but let's try direct ObjectStore usage for testing.
    
    ObjectStore raw_store(test_dir_ / ".flux" / "objects");
    
    auto id_result = raw_store.write(ObjectType::Blob, data, HashAlgorithm::SHA256);
    ASSERT_TRUE(id_result.has_value());
    ObjectId id = *id_result;
    
    // Verify readable loose
    auto read_loose = raw_store.read(id);
    ASSERT_TRUE(read_loose.has_value());
    
    // 2. Pack the object
    // We need to use PackFile::create_from_objects manually
    std::vector<ObjectId> objects = {id};
    Path pack_dir = test_dir_ / ".flux" / "objects" / "pack";
    Filesystem::create_directories(pack_dir);
    
    auto pack_result = PackFile::create_from_objects(pack_dir, objects, raw_store);
    ASSERT_TRUE(pack_result.has_value());
    
    // 3. Remove loose object
    auto remove_result = raw_store.remove(id);
    ASSERT_TRUE(remove_result.has_value());
    
    // Verify not exists loose (conceptually, though store.read handles fallback now)
    // We want to ensure raw access to loose fails? ObjectStore doesn't expose strict "read_loose".
    // But we can check filesystem
    // Actually, store.read() should now succeed because of the pack!
    
    // 4. Read back (should come from pack)
    // We need to re-initialize store to pick up new packs (since load_packs is called in ctor)
    ObjectStore new_store(test_dir_ / ".flux" / "objects");
    
    auto read_packed = new_store.read(id);
    ASSERT_TRUE(read_packed.has_value());
    
    std::string read_content(read_packed->begin(), read_packed->end());
    EXPECT_EQ(content, read_content);
}

} // namespace flux
