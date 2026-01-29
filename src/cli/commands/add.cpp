#include "flux/core/repository.hpp"
#include "flux/core/blob.hpp"
#include "flux/util/filesystem.hpp"
#include <CLI/CLI.hpp>
#include <fmt/format.h>
#include <iostream>

int cmd_add(int argc, char** argv) {
    CLI::App app{"Add files to the staging area"};
    
    std::vector<std::string> paths;
    app.add_option("paths", paths, "Files to add")->required();
    
    try {
        app.parse(argc, argv);
    } catch (const CLI::ParseError& e) {
        return app.exit(e);
    }
    
    // Open repository
    auto repo_result = flux::Repository::open(".");
    if (!repo_result) {
        fmt::print(stderr, "Error: {}\n", repo_result.error());
        return 1;
    }
    
    auto& repo = **repo_result;
    
    // Add each file
    for (const auto& path_str : paths) {
        flux::Path path(path_str);
        
        // Read file content
        auto content_result = flux::Filesystem::read_file(path);
        if (!content_result) {
            fmt::print(stderr, "Error reading {}: {}\n", path_str, content_result.error());
            continue;
        }
        
        // Create blob from content
        flux::Blob blob = flux::Blob::from_data(*content_result, repo.hash_algorithm());
        
        // Write blob chunks to object store
        for (const auto& chunk : blob.chunks()) {
            auto chunk_id_result = repo.objects().write(
                flux::ObjectType::Blob,
                chunk.data,
                repo.hash_algorithm()
            );
            
            if (!chunk_id_result) {
                fmt::print(stderr, "Error writing chunk: {}\n", chunk_id_result.error());
                continue;
            }
        }
        
        // Write blob metadata
        flux::Bytes blob_data = blob.serialize();
        auto blob_id_result = repo.objects().write(
            flux::ObjectType::Blob,
            blob_data,
            repo.hash_algorithm()
        );
        
        if (!blob_id_result) {
            fmt::print(stderr, "Error writing blob: {}\n", blob_id_result.error());
            continue;
        }
        
        // Stage file
        auto stage_result = repo.index().stage_file(
            path,
            *blob_id_result,
            flux::FileMode::Regular
        );
        
        if (!stage_result) {
            fmt::print(stderr, "Error staging {}: {}\n", path_str, stage_result.error());
            continue;
        }
        
        fmt::print("Added {}\n", path_str);
    }
    
    return 0;
}
