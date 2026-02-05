# FluxVCS

A modern distributed version control system built in **Zig** that addresses the architectural, usability, performance, and safety limitations of Git.

## Key Features

- **Built with Zig 0.15.2**: High-performance, memory-safe, and robust codebase
- **Safe by Default**: Transactional operations with write-ahead logging prevent data loss
- **Chunk-Based Storage**: Efficient handling of large files and binaries with content-defined chunking
- **Algorithm-Agile**: Pluggable hash algorithms (SHA-1, SHA-256, SHA3, BLAKE3)
- **Fast Operations**: SQLite-based indexing for O(1) status checks
- **Semantic Awareness**: Foundation for AST-based diffs and merges

## Current Status

**Phase 1: Core Engine & Foundation** (Migrated from C++ to Zig)

- ✅ Repository initialization
- ✅ Chunk-based object storage (CDC)
- ✅ Transactional operations (WAL)
- ✅ Algorithm-agile hashing
- ✅ Core CLI commands (init, add, commit, branch, log, status, diff, show, tag, reset)

## Building

### Requirements

- **Zig 0.15.2**
- **SQLite3** development headers
- **OpenSSL** development headers
- **zstd** development headers
- **CURL** development headers
- **zlib** development headers

### Ubuntu/Debian

```bash
sudo apt install zig sqlite3 libsqlite3-dev libssl-dev libzstd-dev libcurl4-openssl-dev zlib1g-dev
```

*Note: If Zig 0.15.2 is not in your package manager, download it from [ziglang.org](https://ziglang.org/download/).*

### Build Instructions

```bash
# Clone the repository
git clone https://github.com/gradykorchinski-ship-it/FluxVCS.git
cd FluxVCS

# Build the project
zig build

# The binary will be available at:
./zig-out/bin/flux
```

### Running Tests

```bash
zig build test
```

## Quick Start

```bash
# Initialize a new repository
./zig-out/bin/flux init my-project
cd my-project

# Add files
echo "Hello FluxVCS" > README.md
../zig-out/bin/flux add README.md

# Commit changes
../zig-out/bin/flux commit -m "Initial commit"

# View history
../zig-out/bin/flux log

# Check status
../zig-out/bin/flux status

# Show changes
../zig-out/bin/flux diff
```

## Architecture

FluxVCS uses a layered architecture:

- **CLI Layer**: User-facing commands in Zig
- **Core Engine**: Repository logic and object model
- **Storage Backend**: Object store, references, and WAL
- **Index Manager**: SQLite-based metadata indexing
- **Util**: CDC chunking, compression, and hashing

## Roadmap

- **Phase 1**: Core Engine & Foundation (Complete)
- **Phase 2**: Remote Operations (clone, push, pull, fetch) - In Progress
- **Phase 3**: Advanced Features (stash, revert, rebase)
- **Phase 4**: Semantic Diff & Merge
- **Phase 5**: Performance & Scalability

## License

MIT License - See LICENSE file for details

## Contributing

FluxVCS is in active development. Contributions are welcome!
