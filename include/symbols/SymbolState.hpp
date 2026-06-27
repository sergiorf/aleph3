#pragma once

#include "expr/Expr.hpp"

#include <string>
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

    void add(std::string name, SymbolDefinitionRecord record) {
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
