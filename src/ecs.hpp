//
// Created by Maksym Pasichnyk on 05.02.2023.
//

#pragma once

#include "iter.hpp"
#include "numeric.hpp"

#include <any>
#include <typeindex>
#include <unordered_map>

namespace ecs {
    namespace detail {
        template<typename T>
        struct E {};

        template<template<typename...> typename List, typename... L, typename... R>
        consteval auto join(const List<L...>&, const List<R...>&) {
            return List<L..., R...>{};
        }

        template<template<typename...> typename List>
        consteval auto filter(List<>, auto predicate) {
            return List<>();
        }

        template<template<typename...> typename List, typename T, typename... Ts>
        consteval auto filter(List<T, Ts...>, auto predicate) {
            if constexpr (predicate(E<T>())) {
                return join(List<T>(), filter(List<Ts...>(), predicate));
            } else {
                return filter(List<Ts...>(), predicate);
            }
        }

        template<template<typename...> typename T>
        consteval auto is_instance_of(auto) {
            return false;
        }

        template<template<typename...> typename T, typename... U>
        consteval auto is_instance_of(E<T<U...>>) {
            return true;
        }
    }

    template<typename...>
    struct TypeList {
        consteval auto filter(auto predicate) {
            return detail::filter(*this, predicate);
        }
    };

    struct Entity {
        u32 id;

        static constexpr auto null() -> Entity {
            return {std::numeric_limits<u32>::max()};
        }

        friend auto operator<=>(const Entity& lhs, const Entity& rhs) = default;
    };

    struct Storage {
        Entity first = Entity::null();
        Entity prev = Entity::null();
        Entity next = Entity::null();
        Entity parent = Entity::null();
        size_t children = 0;

        std::unordered_map<std::type_index, std::any> components;

        template<typename T>
        [[nodiscard]] auto has() const -> bool {
            return components.contains(std::type_index(typeid(T)));
        }

        template<typename T>
        [[nodiscard]] auto get() -> T& {
            return *std::any_cast<T>(&components.at(std::type_index(typeid(T))));
        }

        template<typename T>
        void insert(T&& component) {
            components.insert_or_assign(std::type_index(typeid(T)), std::forward<T>(component));
        }

        void clear() {
            components.clear();
        }

        template<typename... T>
        auto all_of() {
            return (has<T>() && ...);
        }

        template<typename... T>
        auto any_of() {
            return (has<T>() || ...);
        }
    };

    struct World {
        template<typename... T>
        auto spawn(T&& ... components) -> Entity {
            auto entity = create();
            (insert(entity, std::forward<T>(components)), ...);
            return entity;
        }

        void destroy(Entity entity) {
            storages_[entity.id].clear();
            entities_[entity.id] = deleted_;
            deleted_ = entity;
        }

        template<typename T>
        void insert(Entity entity, T&& component) {
            storages_[entity.id].insert(std::forward<T>(component));
        }

        template<typename... T>
        [[nodiscard]] auto all_of(Entity entity) -> bool {
            return storages_[entity.id].all_of<T...>();
        }

        template<typename... T>
        [[nodiscard]] auto any_of(Entity entity) -> bool {
            return storages_[entity.id].any_of<T...>();
        }

        template<typename T>
        [[nodiscard]] auto get(Entity entity) -> T& {
            return storages_[entity.id].get<T>();
        }

        [[nodiscard]] auto each() {
            return Iter{ranges::views::iota(0zu, entities_.size())}
                .map([this](size_t i) { return entities_[i]; })
                .where(std::bind_front(&ecs::World::alive, this));
        }

        [[nodiscard]] auto alive(Entity entity) -> bool {
            return entities_[entity.id] == entity;
        }

        [[nodiscard]] auto children(Entity entity) {
            return Iter{ranges::subrange(
                ChildIterator{storages_[entity.id].first, this},
                ChildIterator{Entity::null(), this},
                storages_[entity.id].children
            )};
        };

        [[nodiscard]] auto parent_of(Entity entity) -> Entity {
            return storages_[entity.id].parent;
        }

        void clear() {
            deleted_ = Entity::null();
            entities_.clear();
            storages_.clear();
        }

    private:
        [[nodiscard]] auto create() -> Entity {
            return deleted_ == Entity::null() ? allocate() : recycle();
        }

        [[nodiscard]] auto recycle() -> Entity {
            auto entity = deleted_;
            deleted_ = entities_[deleted_.id];
            entities_[entity.id] = entity;
            return entity;
        }

        [[nodiscard]] auto allocate() -> Entity {
            auto entity = Entity{static_cast<u32>(entities_.size())};
            entities_.emplace_back(entity);
            storages_.emplace_back(Storage{});
            return entity;
        }

    private:
        struct ChildIterator {
            using iterator_category = std::forward_iterator_tag;
            using value_type = Entity;
            using difference_type = std::ptrdiff_t;
            using pointer = Entity*;
            using reference = Entity&;

            Entity current;
            World* world;

            auto operator*() const -> Entity {
                return current;
            }

            auto operator++() -> ChildIterator& {
                current = world->storages_[current.id].next;
                return *this;
            }

            auto operator++(int) -> ChildIterator {
                auto copy = *this;
                ++(*this);
                return copy;
            }

            auto operator==(const ChildIterator& other) const -> bool {
                return current == other.current;
            }
        };

        Entity deleted_ = Entity::null();

        std::vector<Entity> entities_;
        std::vector<Storage> storages_;
    };

    template<typename>
    struct With {};

    template<typename>
    struct Without {};

    template<typename... T>
    struct Query {
        explicit Query(World& world) : world(&world) {}

        auto iter() {
            return build(
                TypeList<T...>()
                    .filter([](auto type) {
                        return !detail::is_instance_of<With>(type)
                            && !detail::is_instance_of<Without>(type);
                    }),
                TypeList<T...>().filter([](auto type) {
                    return detail::is_instance_of<With>(type);
                }),
                TypeList<T...>().filter([](auto type) {
                    return detail::is_instance_of<Without>(type);
                })
            );
        }

    private:
        template<typename... T1, typename... T2, typename... T3>
        auto build(TypeList<T1...>, TypeList<With<T2>...>, TypeList<Without<T3>...>) {
            return world->each()
                .where([*this](Entity entity) {
                    return !world->any_of<T3...>(entity)
                         && world->all_of<T2...>(entity)
                         && world->all_of<T1...>(entity);
                })
                .map([*this](Entity entity) {
                    return std::tuple<Entity, T1& ...>(entity, world->get<T1>(entity)...);
                });
        }

    private:
        World* world;
    };

    template<typename Context>
    struct System {
        explicit System() = default;
        virtual ~System() = default;

        virtual void run(Context&, World& world) = 0;
    };

    template<typename Context, typename T>
    struct SystemParamFetch;

    template<typename Context>
    struct SystemParamFetch<Context, Context> {
        static auto fetch(Context& context, World& world) -> Context& {
            return context;
        }
    };

    template<typename Context>
    struct SystemParamFetch<Context, World> {
        static auto fetch(Context& context, World& world) -> World& {
            return world;
        }
    };

    template<typename Context, typename... T>
    struct SystemParamFetch<Context, Query<T...>> {
        static auto fetch(Context& context, World& world) -> Query<T...> {
            return Query<T...>(world);
        }
    };

    template<typename Context, typename... Args>
    struct FunctionSystem : System<Context> {
        std::function<void(Args...)> action;

        template<typename Action>
        explicit FunctionSystem(Action&& action) : action(std::forward<Action>(action)) {}

        void run(Context& context, World& world) override {
            action(SystemParamFetch<Context, std::decay_t<Args>>::fetch(context, world)...);
        }
    };
}
