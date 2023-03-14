#pragma once

#include <range/v3/all.hpp>

template<typename T>
struct Iter {
    T range_;

    template<typename Fn>
    static constexpr auto wrap(Fn&& fn) {
        return [fn = std::forward<Fn>(fn)](auto&& args) {
            if constexpr (std::is_member_object_pointer_v<Fn>) {
                return (std::forward<decltype(args)>(args).*fn);
            } else if constexpr (std::is_member_function_pointer_v<Fn>) {
                return (std::forward<decltype(args)>(args).*fn)();
            } else if constexpr (std::is_invocable_v<Fn,decltype(args)>) {
                return fn(std::forward<decltype(args)>(args));
            } else {
                return std::apply(fn, std::forward<decltype(args)>(args));
            }
        };
    }

    template<typename Fn>
    constexpr auto any(Fn&& fn) {
        return ranges::any_of(range_, wrap(std::forward<Fn>(fn)));
    }

    template<typename Fn>
    constexpr auto all(Fn&& fn) {
        return ranges::all_of(range_, wrap(std::forward<Fn>(fn)));
    }

    template<typename Fn>
    constexpr auto where(Fn&& fn) {
        return ::Iter{ranges::views::filter(range_, wrap(std::forward<Fn>(fn)))};
    }

    template<typename Fn>
    constexpr auto map(Fn&& fn) {
        return ::Iter{ranges::views::transform(range_, wrap(std::forward<Fn>(fn)))};
    }

    template<typename Fn>
    constexpr auto flatmap(Fn&& fn) {
        return map(std::forward<Fn>(fn)).join();
    }

    template<typename R, typename Fn>
    constexpr auto reduce(R&& init, Fn&& fn) -> R {
        return ranges::accumulate(
            range_,
            std::forward<R>(init),
            std::forward<Fn>(fn)
        );
    }

//    template<typename Fn>
//    constexpr auto sorted(Fn&& fn) {
//        ranges::sort(collect(), std::forward<Fn>(fn));
//    }

    template<typename Fn>
    constexpr auto group_by(Fn&& fn) {
        return ::Iter{ranges::views::chunk_by(range_, std::forward<Fn>(fn))};
    }

    template<typename Fn>
    constexpr void for_each(Fn&& fn) {
        ranges::for_each(range_, wrap(std::forward<Fn>(fn)));
    }

    constexpr auto zip(auto&& ... iter) {
        return ::Iter{ranges::views::zip(range_, std::forward<decltype(iter)>(iter)...)};
    }

    constexpr auto join() {
        return ::Iter{ranges::views::join(range_)};
    }

    template<typename Tp>
    constexpr auto to() -> Tp {
        return ranges::to<Tp>(range_);
    }

    template<template<typename...> typename Tp>
    constexpr auto to() {
        return ranges::to<Tp>(range_);
    }

    template<typename U>
    constexpr auto split(U&& val) {
        return ::Iter{ranges::views::split(range_, std::forward<U>(val))};
    }

    template<typename Fn>
    constexpr auto count_if(Fn&& fn) {
        return ranges::count_if(range_, wrap(std::forward<Fn>(fn)));
    }

    constexpr auto first() {
        return ranges::front(range_);
    }

    constexpr auto collect() {
        return ranges::to<std::vector>(range_);
    }

    constexpr auto begin() noexcept(noexcept(ranges::begin(range_))) {
        return ranges::begin(range_);
    }

    constexpr auto end() noexcept(noexcept(ranges::end(range_))) {
        return ranges::end(range_);
    }
};

template<typename T>
Iter(T&) -> Iter<T&>;

template<typename T>
Iter(T&&) -> Iter<std::decay_t<T>>;
