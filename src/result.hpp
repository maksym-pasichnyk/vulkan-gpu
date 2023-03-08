//
// Created by Maksym Pasichnyk on 06.03.2023.
//

#pragma once

#include "enum.hpp"

#include <concepts>
#include <stdexcept>

template<typename T, std::derived_from<std::runtime_error> E>
struct Result : Enum<T, E> {
    using Enum = Enum<T, E>;
    using Enum::Enum;

    [[nodiscard]] auto has_value() const -> bool {
        switch (Enum::index()) {
            case 0: return true;
            case 1: return false;
            default: unreachable();
        }
    }

    [[nodiscard]] auto has_error() const -> bool {
        switch (Enum::index()) {
            case 0: return false;
            case 1: return true;
            default: unreachable();
        }
    }

    [[nodiscard]] auto value() const -> const T& {
        switch (Enum::index()) {
            case 0: return Enum::template as<T>();
            case 1: throw E(Enum::template as<E>());
            default: unreachable();
        }
    }

    [[nodiscard]] auto value() -> T& {
        switch (Enum::index()) {
            case 0: return Enum::template as<T>();
            case 1: throw E(Enum::template as<E>());
            default: unreachable();
        }
    }

    template<typename U>
    [[nodiscard]] auto value_or(U&& other) -> T {
        switch (Enum::index()) {
            case 0: return Enum::template as<T>();
            case 1: return std::forward<U>(other);
            default: unreachable();
        }
    }

    [[noreturn]] inline static void unreachable() {
        while (true) {}
    }
};