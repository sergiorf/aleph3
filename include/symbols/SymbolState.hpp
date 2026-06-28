#pragma once

#include "expr/Expr.hpp"

#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

namespace aleph3::symbols {

enum class SymbolAttribute {
    hold_all,
    hold_first,
    hold_rest,
    listable,
    numeric_function,
    protected_symbol
};

enum class DefinitionOrigin {
    builtin,
    pack,
    user
};

enum class SymbolDefinitionKind {
    own_value,
    user_function,
    registered_handler,
    builtin_function,
    host_function,
    rewrite_rule
};

struct SymbolMetadata {
    std::string name;
    std::vector<SymbolAttribute> attributes;
    std::string documentation;
    DefinitionOrigin origin = DefinitionOrigin::user;
    std::string provider;
};

struct SymbolDefinitionRecord {
    SymbolDefinitionKind kind = SymbolDefinitionKind::own_value;
    DefinitionOrigin origin = DefinitionOrigin::user;
    std::string provider;
};

class SymbolValueTable {
public:
    using MapType = std::unordered_map<std::string, ExprPtr>;

    [[nodiscard]] bool contains(const std::string& name) const {
        return values_.find(name) != values_.end();
    }

    [[nodiscard]] ExprPtr* lookup(const std::string& name) {
        auto it = values_.find(name);
        return it == values_.end() ? nullptr : &it->second;
    }

    [[nodiscard]] const ExprPtr* lookup(const std::string& name) const {
        auto it = values_.find(name);
        return it == values_.end() ? nullptr : &it->second;
    }

    void set(std::string name, ExprPtr value) {
        values_[std::move(name)] = std::move(value);
    }

    [[nodiscard]] MapType& entries() {
        return values_;
    }

    [[nodiscard]] const MapType& entries() const {
        return values_;
    }

private:
    MapType values_;
};

class SymbolMetadataTable {
public:
    using MapType = std::unordered_map<std::string, SymbolMetadata>;

    [[nodiscard]] bool contains(const std::string& name) const {
        return metadata_.find(name) != metadata_.end();
    }

    [[nodiscard]] SymbolMetadata* lookup(const std::string& name) {
        auto it = metadata_.find(name);
        return it == metadata_.end() ? nullptr : &it->second;
    }

    [[nodiscard]] const SymbolMetadata* lookup(const std::string& name) const {
        auto it = metadata_.find(name);
        return it == metadata_.end() ? nullptr : &it->second;
    }

    void set(std::string name, SymbolMetadata metadata) {
        metadata_[std::move(name)] = std::move(metadata);
    }

    void ensure(std::string name, SymbolMetadata metadata) {
        metadata_.try_emplace(std::move(name), std::move(metadata));
    }

    [[nodiscard]] bool has_attribute(const std::string& name, SymbolAttribute attribute) const {
        const auto* metadata = lookup(name);
        if (metadata == nullptr) {
            return false;
        }
        for (const auto candidate : metadata->attributes) {
            if (candidate == attribute) {
                return true;
            }
        }
        return false;
    }

    [[nodiscard]] MapType& entries() {
        return metadata_;
    }

    [[nodiscard]] const MapType& entries() const {
        return metadata_;
    }

private:
    MapType metadata_;
};

class SymbolDefinitionTable {
public:
    using MapType = std::unordered_map<std::string, std::vector<SymbolDefinitionRecord>>;

    [[nodiscard]] bool contains(const std::string& name) const {
        return definitions_.find(name) != definitions_.end();
    }

    [[nodiscard]] std::vector<SymbolDefinitionRecord>* lookup(const std::string& name) {
        auto it = definitions_.find(name);
        return it == definitions_.end() ? nullptr : &it->second;
    }

    [[nodiscard]] const std::vector<SymbolDefinitionRecord>* lookup(const std::string& name) const {
        auto it = definitions_.find(name);
        return it == definitions_.end() ? nullptr : &it->second;
    }

    [[nodiscard]] bool contains(
        const std::string& name,
        SymbolDefinitionKind kind) const {
        return find(name, kind) != nullptr;
    }

    [[nodiscard]] SymbolDefinitionRecord* find(
        const std::string& name,
        SymbolDefinitionKind kind) {
        auto* records = lookup(name);
        if (records == nullptr) {
            return nullptr;
        }
        for (auto& record : *records) {
            if (record.kind == kind) {
                return &record;
            }
        }
        return nullptr;
    }

    [[nodiscard]] const SymbolDefinitionRecord* find(
        const std::string& name,
        SymbolDefinitionKind kind) const {
        const auto* records = lookup(name);
        if (records == nullptr) {
            return nullptr;
        }
        for (const auto& record : *records) {
            if (record.kind == kind) {
                return &record;
            }
        }
        return nullptr;
    }

    [[nodiscard]] bool contains(
        const std::string& name,
        SymbolDefinitionKind kind,
        DefinitionOrigin origin,
        std::string_view provider) const {
        const auto* records = lookup(name);
        if (records == nullptr) {
            return false;
        }
        for (const auto& record : *records) {
            if (record.kind == kind &&
                record.origin == origin &&
                record.provider == provider) {
                return true;
            }
        }
        return false;
    }

    void add(std::string name, SymbolDefinitionRecord record) {
        definitions_[std::move(name)].push_back(std::move(record));
    }

    void add_unique(std::string name, SymbolDefinitionRecord record) {
        if (contains(name, record.kind, record.origin, record.provider)) {
            return;
        }
        definitions_[std::move(name)].push_back(std::move(record));
    }

    [[nodiscard]] MapType& entries() {
        return definitions_;
    }

    [[nodiscard]] const MapType& entries() const {
        return definitions_;
    }

private:
    MapType definitions_;
};

class FunctionDefinitionTable {
public:
    using MapType = std::unordered_map<std::string, FunctionDefinition>;

    [[nodiscard]] bool contains(const std::string& name) const {
        return definitions_.find(name) != definitions_.end();
    }

    [[nodiscard]] FunctionDefinition* lookup(const std::string& name) {
        auto it = definitions_.find(name);
        return it == definitions_.end() ? nullptr : &it->second;
    }

    [[nodiscard]] const FunctionDefinition* lookup(const std::string& name) const {
        auto it = definitions_.find(name);
        return it == definitions_.end() ? nullptr : &it->second;
    }

    void set(std::string name, FunctionDefinition definition) {
        definitions_[std::move(name)] = std::move(definition);
    }

    [[nodiscard]] MapType& entries() {
        return definitions_;
    }

    [[nodiscard]] const MapType& entries() const {
        return definitions_;
    }

private:
    MapType definitions_;
};

}  // namespace aleph3::symbols
