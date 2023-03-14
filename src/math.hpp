//
// Created by Maksym Pasichnyk on 12.03.2023.
//

#pragma once

#include "numeric.hpp"

template<typename T, size_t N>
struct Vec;

template<typename T>
struct Vec<T, 2> {
    T x, y;

    constexpr auto operator[](size_t i) -> T& {
        return *(&x + i);
    }

    constexpr auto operator[](size_t i) const -> const T& {
        return *(&x + i);
    }

    friend constexpr auto operator<=>(const Vec& lhs, const Vec& rhs) = default;
};

template<typename T>
struct Vec<T, 3> {
    T x, y, z;

    constexpr auto operator[](size_t i) -> T& {
        return *(&x + i);
    }

    constexpr auto operator[](size_t i) const -> const T& {
        return *(&x + i);
    }

    friend constexpr auto operator<=>(const Vec& lhs, const Vec& rhs) = default;
};

template<typename T>
struct Vec<T, 4> {
    T x, y, z, w;

    constexpr auto operator[](size_t i) -> T& {
        return *(&x + i);
    }

    constexpr auto operator[](size_t i) const -> const T& {
        return *(&x + i);
    }

    friend constexpr auto operator<=>(const Vec& lhs, const Vec& rhs) = default;
};

template<typename T, size_t C, size_t R>
struct Mat {
    std::array<Vec<T, C>, R> rows;

    constexpr auto operator[](size_t row) -> Vec<T, C>& {
        return rows[row];
    }

    constexpr auto operator[](size_t row) const -> const Vec<T, C>& {
        return rows[row];
    }

    constexpr auto operator[](size_t row, size_t col) -> T& {
        return rows[row][col];
    }

    constexpr auto operator[](size_t row, size_t col) const -> const T& {
        return rows[row][col];
    }

    friend constexpr auto operator<=>(const Mat& lhs, const Mat& rhs) = default;
};

template<typename T> using Vec2 = Vec<T, 2>;
template<typename T> using Vec3 = Vec<T, 3>;
template<typename T> using Vec4 = Vec<T, 4>;

using Vec2i = Vec<i32, 2>;
using Vec3i = Vec<i32, 3>;
using Vec4i = Vec<i32, 4>;

using Vec2u = Vec<u32, 2>;
using Vec3u = Vec<u32, 3>;
using Vec4u = Vec<u32, 4>;

using Vec2f = Vec<f32, 2>;
using Vec3f = Vec<f32, 3>;
using Vec4f = Vec<f32, 4>;

template<typename T> using Mat2x2 = Mat<T, 2, 2>;
template<typename T> using Mat2x3 = Mat<T, 2, 3>;
template<typename T> using Mat2x4 = Mat<T, 2, 4>;

template<typename T> using Mat3x2 = Mat<T, 3, 2>;
template<typename T> using Mat3x3 = Mat<T, 3, 3>;
template<typename T> using Mat3x4 = Mat<T, 3, 4>;

template<typename T> using Mat4x2 = Mat<T, 4, 2>;
template<typename T> using Mat4x3 = Mat<T, 4, 3>;
template<typename T> using Mat4x4 = Mat<T, 4, 4>;

template<typename T, size_t N>
constexpr auto operator+(const Vec<T, N>& lhs, const Vec<T, N>& rhs) {
    Vec<T, N> result;
    for (size_t i = 0; i < N; ++i) {
        result[i] = lhs[i] + rhs[i];
    }
    return result;
}

template<typename T, size_t N>
constexpr auto operator-(const Vec<T, N>& lhs, const Vec<T, N>& rhs) {
    Vec<T, N> result;
    for (size_t i = 0; i < N; ++i) {
        result[i] = lhs[i] - rhs[i];
    }
    return result;
}

template<typename T, size_t N>
constexpr auto operator*(const Vec<T, N>& lhs, const Vec<T, N>& rhs) {
    Vec<T, N> result;
    for (size_t i = 0; i < N; ++i) {
        result[i] = lhs[i] * rhs[i];
    }
    return result;
}

template<typename T, size_t N>
constexpr auto operator/(const Vec<T, N>& lhs, const Vec<T, N>& rhs) {
    Vec<T, N> result;
    for (size_t i = 0; i < N; ++i) {
        result[i] = lhs[i] / rhs[i];
    }
    return result;
}

template<typename T, size_t N>
constexpr auto operator+(const Vec<T, N>& lhs, T rhs) {
    Vec<T, N> result;
    for (size_t i = 0; i < N; ++i) {
        result[i] = lhs[i] + rhs;
    }
    return result;
}

template<typename T, size_t N>
constexpr auto operator-(const Vec<T, N>& lhs, T rhs) {
    Vec<T, N> result;
    for (size_t i = 0; i < N; ++i) {
        result[i] = lhs[i] - rhs;
    }
    return result;
}

template<typename T, size_t N>
constexpr auto operator*(const Vec<T, N>& lhs, T rhs) {
    Vec<T, N> result;
    for (size_t i = 0; i < N; ++i) {
        result[i] = lhs[i] * rhs;
    }
    return result;
}

template<typename T, size_t N>
constexpr auto operator/(const Vec<T, N>& lhs, T rhs) {
    Vec<T, N> result;
    for (size_t i = 0; i < N; ++i) {
        result[i] = lhs[i] / rhs;
    }
    return result;
}

template<typename T, size_t N>
constexpr auto operator+(T lhs, const Vec<T, N>& rhs) {
    Vec<T, N> result;
    for (size_t i = 0; i < N; ++i) {
        result[i] = lhs + rhs[i];
    }
    return result;
}

template<typename T, size_t N>
constexpr auto operator-(T lhs, const Vec<T, N>& rhs) {
    Vec<T, N> result;
    for (size_t i = 0; i < N; ++i) {
        result[i] = lhs - rhs[i];
    }
    return result;
}

template<typename T, size_t N>
constexpr auto operator*(T lhs, const Vec<T, N>& rhs) {
    Vec<T, N> result;
    for (size_t i = 0; i < N; ++i) {
        result[i] = lhs * rhs[i];
    }
    return result;
}

template<typename T, size_t N>
constexpr auto operator/(T lhs, const Vec<T, N>& rhs) {
    Vec<T, N> result;
    for (size_t i = 0; i < N; ++i) {
        result[i] = lhs / rhs[i];
    }
    return result;
}

template<typename T, size_t N>
constexpr auto operator+=(Vec<T, N>& lhs, const Vec<T, N>& rhs) {
    for (size_t i = 0; i < N; ++i) {
        lhs[i] += rhs[i];
    }
    return lhs;
}

template<typename T, size_t N>
constexpr auto operator-=(Vec<T, N>& lhs, const Vec<T, N>& rhs) {
    for (size_t i = 0; i < N; ++i) {
        lhs[i] -= rhs[i];
    }
    return lhs;
}

template<typename T, size_t N>
constexpr auto operator*=(Vec<T, N>& lhs, const Vec<T, N>& rhs) {
    for (size_t i = 0; i < N; ++i) {
        lhs[i] *= rhs[i];
    }
    return lhs;
}

template<typename T, size_t N>
constexpr auto operator/=(Vec<T, N>& lhs, const Vec<T, N>& rhs) {
    for (size_t i = 0; i < N; ++i) {
        lhs[i] /= rhs[i];
    }
    return lhs;
}

template<typename T, size_t N>
constexpr auto operator+=(Vec<T, N>& lhs, T rhs) {
    for (size_t i = 0; i < N; ++i) {
        lhs[i] += rhs;
    }
    return lhs;
}

template<typename T, size_t N>
constexpr auto operator-=(Vec<T, N>& lhs, T rhs) {
    for (size_t i = 0; i < N; ++i) {
        lhs[i] -= rhs;
    }
    return lhs;
}

template<typename T, size_t N>
constexpr auto operator*=(Vec<T, N>& lhs, T rhs) {
    for (size_t i = 0; i < N; ++i) {
        lhs[i] *= rhs;
    }
    return lhs;
}

template<typename T, size_t N>
constexpr auto operator/=(Vec<T, N>& lhs, T rhs) {
    for (size_t i = 0; i < N; ++i) {
        lhs[i] /= rhs;
    }
    return lhs;
}