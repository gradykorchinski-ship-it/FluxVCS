# FluxVCS

A modern distributed version control system built in C++ that addresses the architectural, usability, performance, and safety limitations of Git.

## Key Features

- **Safe by Default**: Transactional operations with write-ahead logging prevent data loss
- **Chunk-Based Storage**: Efficient handling of large files and binaries with content-defined chunking
- **Algorithm-Agile**: Pluggable hash algorithms for future-proof cryptographic agility
- **Fast Operations**: SQLite-based indexing for O(1) status checks
- **Semantic Awareness**: Foundation for AST-based diffs and merges (future phases)

## Current Status

**Phase 1: Core Engine & Foundation** (In Development)

- ✅ Repository initialization
- ✅ Chunk-based object storage
- ✅ Transactional operations
- ✅ Basic CLI commands (init, add, commit, branch, log, status)

## Building

### Requirements

- C++20 compatible compiler (GCC 10+, Clang 12+)
- CMake 3.20+
- SQLite3
- OpenSSL
- zstd

### Ubuntu/Debian

```bash
sudo apt-get install build-essential cmake libsqlite3-dev libssl-dev libzstd-dev pkg-config
```

### Build Instructions

```bash
mkdir build && cd build
cmake ..
make -j$(nproc)
```

### Running Tests

```bash
cd build
ctest --output-on-failure
```

## Quick Start

```bash
# Initialize a new repository
./flux init my-project
cd my-project

# Add files
echo "Hello FluxVCS" > README.md
./flux add README.md

# Commit changes
./flux commit -m "Initial commit"

# View history
./flux log

# Check status
./flux status
```

## Architecture

FluxVCS uses a layered architecture:

- **CLI Layer**: User-facing commands
- **Core Engine**: Repository logic and object model
- **Storage Backend**: Object store, references, and WAL
- **Index Manager**: SQLite-based metadata indexing

## Roadmap

- **Phase 1**: Core Engine & Foundation (Current)
- **Phase 2**: Safety & Transactional Operations
- **Phase 3**: Semantic Diff & Merge
- **Phase 4**: Performance & Scalability
- **Phase 5**: Network Protocol & Collaboration
- **Phase 6**: Security & Trust

## License

MIT License - See LICENSE file for details

## Contributing

This is currently in early development. Contributions welcome once Phase 1 is complete.
