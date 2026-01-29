#pragma once

#include <cstdint>
#include <string>
#include <string_view>
#include <vector>
#include <optional>
#include <filesystem>
#include <variant>
#include <stdexcept>

namespace flux {

// Helper to create error results
template<typename E>
struct ErrorTag {
    E error;
    explicit ErrorTag(E e) : error(std::move(e)) {}
};

template<typename E>
class Unexpected {
public:
    explicit Unexpected(E error) : error_(std::move(error)) {}
    const E& error() const { return error_; }
    
private:
    E error_;
};

// Helper function to create Unexpected - overload for const char* to convert to std::string
inline Unexpected<std::string> unexpected(const char* error) {
    return Unexpected<std::string>(std::string(error));
}

template<typename E>
Unexpected<E> unexpected(E error) {
    return Unexpected<E>(std::move(error));
}

// Simple Result type for C++20 (std::expected is C++23)
template<typename T, typename E = std::string>
class Result {
public:
    // Constructors
    Result(T value) : data_(std::move(value)) {}
    Result(Unexpected<E> error) : data_(ErrorTag<E>{error.error()}) {}
    
    // Allow implicit conversion from string literals when E is std::string
    template<typename U = E>
    requires std::is_same_v<U, std::string>
    Result(const char* error) : data_(ErrorTag<E>{std::string(error)}) {}
    
    // Check if contains value or error
    bool has_value() const { return std::holds_alternative<T>(data_); }
    bool has_error() const { return std::holds_alternative<ErrorTag<E>>(data_); }
    explicit operator bool() const { return has_value(); }
    
    // Access value (throws if error)
    T& value() & { 
        if (!has_value()) throw std::runtime_error("Result contains error");
        return std::get<T>(data_); 
    }
    const T& value() const & { 
        if (!has_value()) throw std::runtime_error("Result contains error");
        return std::get<T>(data_); 
    }
    T&& value() && { 
        if (!has_value()) throw std::runtime_error("Result contains error");
        return std::get<T>(std::move(data_)); 
    }
    
    // Access error (throws if value)
    const E& error() const { 
        if (!has_error()) throw std::runtime_error("Result contains value");
        return std::get<ErrorTag<E>>(data_).error; 
    }
    
    // Dereference operators
    T& operator*() & { return value(); }
    const T& operator*() const & { return value(); }
    T&& operator*() && { return std::move(*this).value(); }
    
    T* operator->() { return &value(); }
    const T* operator->() const { return &value(); }
    
private:
    std::variant<T, ErrorTag<E>> data_;
};

// Specialization for void
template<typename E>
class Result<void, E> {
public:
    Result() : error_(std::nullopt) {}
    Result(Unexpected<E> error) : error_(error.error()) {}
    
    // Allow implicit conversion from string literals when E is std::string
    template<typename U = E>
    requires std::is_same_v<U, std::string>
    Result(const char* error) : error_(std::string(error)) {}
    
    bool has_value() const { return !error_.has_value(); }
    bool has_error() const { return error_.has_value(); }
    explicit operator bool() const { return has_value(); }
    
    void value() const {
        if (has_error()) throw std::runtime_error("Result contains error");
    }
    
    const E& error() const {
        if (!has_error()) throw std::runtime_error("Result contains value");
        return *error_;
    }
    
private:
    std::optional<E> error_;
};

// Common type aliases
using Bytes = std::vector<uint8_t>;
using Path = std::filesystem::path;

// Hash algorithm enumeration
enum class HashAlgorithm {
    SHA1,    // Git compatibility
    SHA256,  // Default for FluxVCS
    BLAKE3,  // Future
    SHA3_256 // Future
};

// Object types
enum class ObjectType : uint8_t {
    Blob = 1,
    Tree = 2,
    Commit = 3,
    Tag = 4
};

// File mode (similar to Git)
enum class FileMode : uint32_t {
    Regular = 0100644,      // Regular file
    Executable = 0100755,   // Executable file
    Symlink = 0120000,      // Symbolic link
    Directory = 0040000     // Directory (tree)
};

// Error types
struct Error {
    std::string message;
    std::string suggestion;
    std::optional<std::string> recovery_command;
    
    Error(std::string msg) : message(std::move(msg)) {}
    
    Error(std::string msg, std::string sug) 
        : message(std::move(msg)), suggestion(std::move(sug)) {}
    
    Error(std::string msg, std::string sug, std::string recovery)
        : message(std::move(msg)), 
          suggestion(std::move(sug)),
          recovery_command(std::move(recovery)) {}
    
    std::string format() const;
};

// Convert enum to string
std::string to_string(HashAlgorithm algo);
std::string_view to_string(ObjectType type);
std::string_view to_string(FileMode mode);

// Parse string to enum
std::optional<HashAlgorithm> parse_hash_algorithm(std::string_view str);
std::optional<ObjectType> parse_object_type(std::string_view str);

} // namespace flux
