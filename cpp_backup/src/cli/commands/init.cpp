#include "flux/core/repository.hpp"
#include <CLI/CLI.hpp>
#include <fmt/format.h>
#include <iostream>

int cmd_init(int argc, char** argv) {
    CLI::App app{"Initialize a new FluxVCS repository"};
    
    std::string path = ".";
    std::string hash_algo = "sha256";
    app.add_option("path", path, "Path to initialize repository")->default_val(".");
    app.add_option("--hash", hash_algo, "Hash algorithm (sha1, sha256)")
        ->check(CLI::IsMember({"sha1", "sha256"}));
    
    try {
        app.parse(argc, argv);
    } catch (const CLI::ParseError& e) {
        return app.exit(e);
    }
    
    flux::HashAlgorithm algo = (hash_algo == "sha1") ? 
        flux::HashAlgorithm::SHA1 : flux::HashAlgorithm::SHA256;
    
    auto result = flux::Repository::init(path, algo);
    if (!result) {
        fmt::print(stderr, "{}\n", result.error());
        return 1;
    }
    
    fmt::print("Initialized empty FluxVCS repository ({}) in {}/.flux/\n", 
        hash_algo, std::filesystem::absolute(path).string());
    
    return 0;
}
