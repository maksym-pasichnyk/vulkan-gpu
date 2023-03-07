//
// Created by Maksym Pasichnyk on 06.03.2023.
//

#pragma once

#include <variant>

template<typename... Types>
struct Enum : std::variant<Types...> {
    using std::variant<Types...>::variant;

    template<typename T>
    [[nodiscard]] auto is() const -> bool {
        return std::holds_alternative<T>(*this);
    }

    template<typename T>
    [[nodiscard]] auto as() const -> const T& {
        return std::get<T>(*this);
    }

    template<typename T>
    [[nodiscard]] auto as() -> T& {
        return std::get<T>(*this);
    }
};