#pragma once

#include "expr/Expr.hpp"

#include <string>
#include <unordered_map>

namespace aleph3::symbols {

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
