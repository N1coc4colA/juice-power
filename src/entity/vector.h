#ifndef JP_ENTITY_VECTOR_H
#define JP_ENTITY_VECTOR_H

#include <cstddef>
#include <iterator>
#include <utility>
#include <vector>

#include "src/defines.h"
#include "src/keywords.h"

namespace Entity {

template<typename V, typename... Types>
concept Visitor = requires(V& v, const V& cv, Types&... args, const Types&... cargs) {
    { v.visit(args...) };
    { cv.visit(cargs...) };
};

template<typename T, typename... Types>
consteval auto type_index_v_impl() -> std::size_t
{
    std::size_t i = 0;
    bool found = false;
    ((found || (std::is_same_v<T, Types> ? (found = true) : (++i, false))), ...);
    return found ? i : sizeof...(Types);
    // sizeof...(Types) = not-found sentinel
}

template<typename T, typename... Types>
consteval auto checked_type_index_v() -> std::size_t
{
    constexpr std::size_t idx = type_index_v_impl<T, Types...>();
    static_assert(idx < sizeof...(Types), "Type not found in Vector's type list");
    return idx;
}

template<typename T, typename... Types>
inline constexpr std::size_t type_index_v = checked_type_index_v<T, Types...>();

template<typename T>
struct is_types_set : std::false_type
{};
template<typename... Ts>
struct is_types_set<TypesSet<Ts...>> : std::true_type
{};

// Trait to detect ReferencesSet (std::tuple<Ts&...>) and extract Ts...
template<typename T>
struct is_references_set : std::false_type
{};
template<typename... Ts>
struct is_references_set<std::tuple<Ts&...>> : std::true_type
{};

// Trait to extract argument types from V::visit member function
template<typename F>
struct visit_args;

template<typename C, typename... Args>
struct visit_args<void (C::*)(Args...)>
{
    using types = std::tuple<std::remove_cvref_t<Args>...>;
};

template<typename C, typename... Args>
struct visit_args<void (C::*)(Args...) const>
{
    using types = std::tuple<std::remove_cvref_t<Args>...>;
};

template<typename V>
using visit_arg_types_t = typename visit_args<decltype(&V::visit)>::types;

template<typename StoragePtr, std::size_t... Is>
class ZipIterator
{
public:
    using difference_type = std::ptrdiff_t;
    using iterator_category = std::random_access_iterator_tag;
    using value_type = std::remove_cvref_t<decltype(std::declval<StoragePtr>()->at_impl(0, std::index_sequence<Is...>{}))>;
    using reference = decltype(std::declval<StoragePtr>()->at_impl(0, std::index_sequence<Is...>{}));
    using pointer = void;

    ZipIterator() = default;
    ZipIterator(StoragePtr storage, const std::size_t idx)
        : m_storage(storage)
        , m_idx(idx)
    {}

    auto operator*() const { return m_storage->at_impl(m_idx, std::index_sequence<Is...>{}); }

    auto operator++() -> ZipIterator&
    {
        ++m_idx;
        return *this;
    }
    auto operator++(int)
    {
        auto tmp = *this;
        ++m_idx;
        return tmp;
    }
    auto operator--() -> ZipIterator&
    {
        --m_idx;
        return *this;
    }
    auto operator--(int)
    {
        auto tmp = *this;
        --m_idx;
        return tmp;
    }

    auto operator+=(const difference_type n) -> ZipIterator&
    {
        m_idx += n;
        return *this;
    }
    auto operator-=(const difference_type n) -> ZipIterator&
    {
        m_idx -= n;
        return *this;
    }
    auto operator+(const difference_type n) const { return ZipIterator(m_storage, m_idx + n); }
    auto operator-(const difference_type n) const { return ZipIterator(m_storage, m_idx - n); }
    auto operator-(const ZipIterator& o) const { return static_cast<difference_type>(m_idx) - static_cast<difference_type>(o.m_idx); }

    auto operator[](const difference_type n) const { return m_storage->at_impl(m_idx + n, std::index_sequence<Is...>{}); }

    auto operator==(const ZipIterator& o) const -> bool { return m_idx == o.m_idx; }
    auto operator!=(const ZipIterator& o) const -> bool { return m_idx != o.m_idx; }
    auto operator<(const ZipIterator& o) const -> bool { return m_idx < o.m_idx; }
    auto operator<=(const ZipIterator& o) const -> bool { return m_idx <= o.m_idx; }
    auto operator>(const ZipIterator& o) const -> bool { return m_idx > o.m_idx; }
    auto operator>=(const ZipIterator& o) const -> bool { return m_idx >= o.m_idx; }

private:
    StoragePtr m_storage = nullptr;
    std::size_t m_idx = 0;
};

template<typename StoragePtr, std::size_t... Is>
struct ZipRange
{
    using iterator = ZipIterator<StoragePtr, Is...>;

    ZipRange(StoragePtr s, const std::size_t sz)
        : m_storage(s)
        , m_size(sz)
    {}

    auto begin() { return iterator{m_storage, 0}; } // ← add this
    _nodiscard auto begin() const { return iterator{m_storage, 0}; }
    auto end() { return iterator{m_storage, m_size}; } // ← add this
    _nodiscard auto end() const { return iterator{m_storage, m_size}; }
    _nodiscard auto size() const { return m_size; }

private:
    StoragePtr m_storage;
    const std::size_t m_size;
};
} // namespace Entity

// Otherwise it fails, it needs to be in std::ranges or ::, not in ::*.
template<typename StoragePtr, std::size_t... Is>
inline constexpr bool std::ranges::enable_borrowed_range<Entity::ZipRange<StoragePtr, Is...>> = true;

namespace Entity {

template<typename... Types>
class Vector
{
public:
    using value_type = void;
    using size_type = std::size_t;
    using difference_type = std::ptrdiff_t;
    using reference = void;
    using const_reference = void;
    using pointer = void;
    using const_pointer = void;
    // iterator / const_iterator are not aliased here because their type
    // encodes the column index sequence, which cannot be named at this point.
    // Use auto or decltype(vec.begin()) at call sites if needed.

    Vector() = default;

    explicit Vector(const Vector& other)
        : m_data(other.m_data)
    {}
    explicit Vector(Vector&& other) noexcept
        : m_data(std::move(other.m_data))
    {}

    ~Vector() = default;

    auto operator=(const Vector& other) -> Vector& = default;
    auto operator=(Vector&& other) noexcept -> Vector&
    {
        if (this != &other)
            m_data = std::move(other.m_data);
        return *this;
    }

    /* Element access */

    auto at(const size_type idx) -> std::tuple<Types&...> { return at_impl(idx, std::index_sequence_for<Types...>{}); }
    _nodiscard auto at(const size_type idx) const -> std::tuple<const Types&...> { return at_impl(idx, std::index_sequence_for<Types...>{}); }

    // By single type — returns T&
    template<typename T>
        requires(!is_types_set<T>::value && !is_references_set<T>::value)
    auto at(const size_type idx) -> decltype(auto)
    {
        return std::get<type_index_v<T, Types...>>(m_data)[idx];
    }
    template<typename T>
        requires(!is_types_set<T>::value && !is_references_set<T>::value)
    _nodiscard auto at(const size_type idx) const -> decltype(auto)
    {
        return std::get<type_index_v<T, Types...>>(m_data)[idx];
    }

    // By multiple types — returns std::tuple<T1&, T2&, ...>
    template<typename T1, typename T2, typename... Rest>
    auto at(const size_type idx)
    {
        return at_by_typesset_impl(idx, TypesSet<T1, T2, Rest...>{});
    }
    template<typename T1, typename T2, typename... Rest>
    auto at(const size_type idx) const
    {
        return at_by_typesset_impl(idx, TypesSet<T1, T2, Rest...>{});
    }

    // By TypesSet — returns std::tuple<Ts&...>
    template<typename TSet>
        requires is_types_set<TSet>::value
    auto at(const size_type idx)
    {
        return at_by_typesset_impl(idx, TSet{});
    }
    template<typename TSet>
        requires is_types_set<TSet>::value
    auto at(const size_type idx) const
    {
        return at_by_typesset_impl(idx, TSet{});
    }

    // By ReferencesSet — returns std::tuple<Ts&...> with const preserved
    template<typename RefSet>
        requires is_references_set<RefSet>::value
    auto at(const size_type idx) -> RefSet
    {
        return at_by_refset_impl<RefSet>(idx);
    }
    // Note: no const overload — binding non-const refs from a const Vector is ill-formed

    auto operator[](const size_type idx) { return at(idx); }
    auto operator[](const size_type idx) const { return at(idx); }

    /* Capacity */

    _nodiscard auto size() const noexcept -> size_type
    {
        if constexpr (sizeof...(Types) == 0)
            return 0;
        return std::get<0>(m_data).size();
    }

    _nodiscard auto empty() const noexcept -> bool { return size() == 0; }

    _nodiscard auto capacity() const noexcept -> size_type
    {
        if constexpr (sizeof...(Types) == 0)
            return 0;
        return std::get<0>(m_data).capacity();
    }

    void reserve(const size_type new_cap)
    {
        for_each([new_cap](auto& v) -> auto { v.reserve(new_cap); });
    }

    void shrink_to_fit()
    {
        for_each([](auto& v) -> auto { v.shrink_to_fit(); });
    }

    /* Modifiers */

    template<typename... Args>
    void emplace_back(Args&&... args)
    {
        [&]<std::size_t... Is>(std::index_sequence<Is...>) -> auto {
            (std::get<Is>(m_data).emplace_back(std::forward<Args>(args)), ...);
        }(std::index_sequence_for<Types...>{});
    }

    void push_back(const Types&... values)
    {
        [&]<std::size_t... Is>(std::index_sequence<Is...>) -> auto {
            (std::get<Is>(m_data).push_back(values), ...);
        }(std::index_sequence_for<Types...>{});
    }
    void push_back(Types&&... values)
    {
        [&]<std::size_t... Is>(std::index_sequence<Is...>) -> auto {
            (std::get<Is>(m_data).push_back(std::move(values)), ...);
        }(std::index_sequence_for<Types...>{});
    }

    void pop_back()
    {
        if (!empty())
            for_each([](auto& v) -> auto { v.pop_back(); });
    }

    void clear() noexcept
    {
        for_each([](auto& v) -> auto { v.clear(); });
    }

    void resize(const size_type count)
    {
        for_each([count](auto& v) -> auto { v.resize(count); });
    }
    void resize(const size_type count, const Types&... values)
    {
        [&]<std::size_t... Is>(std::index_sequence<Is...>) -> auto {
            (std::get<Is>(m_data).resize(count, values), ...);
        }(std::index_sequence_for<Types...>{});
    }

    /* Iterators — all columns in declaration order */

    auto begin() { return make_zip_begin(this, std::index_sequence_for<Types...>{}); }
    auto end() { return make_zip_end(this, std::index_sequence_for<Types...>{}); }
    _nodiscard auto begin() const { return make_zip_begin(this, std::index_sequence_for<Types...>{}); }
    _nodiscard auto end() const { return make_zip_end(this, std::index_sequence_for<Types...>{}); }
    _nodiscard auto cbegin() const { return begin(); }
    _nodiscard auto cend() const { return end(); }

    /* Subset range: vec.range<B, A>() iterates columns B and A (in that order) */

    template<typename... SubTypes>
    auto range() &
    {
        return make_zip_range(this, std::index_sequence<type_index_v<SubTypes, Types...>...>{});
    }
    template<typename... SubTypes>
    _nodiscard auto range() const&
    {
        return make_zip_range(this, std::index_sequence<type_index_v<SubTypes, Types...>...>{});
    }

    /* Single-column view */

    template<std::size_t I>
    auto get() &
    {
        return std::get<I>(m_data);
    }
    template<std::size_t I>
    auto get() const&
    {
        return std::get<I>(m_data);
    }
    template<std::size_t I>
    auto get() &&
    {
        return std::get<I>(std::move(m_data));
    }

    /* visit() — calls visitor.visit(colA[i], colB[i], ...) for each row */

    template<typename V>
    void visit(V& visitor)
    {
        visit_impl(visitor);
    }
    template<typename V>
    void visit(V& visitor) const
    {
        visit_impl(visitor);
    }
    template<typename V>
    void visit(const V& visitor)
    {
        visit_impl(visitor);
    }
    template<typename V>
    void visit(const V& visitor) const
    {
        // A const visitor does not imply const data; remove data constness
        // by forwarding to the non-const this. Only valid if the visitor
        // itself is const (i.e. it won't modify the Vector).
        const_cast<Vector*>(this)->visit_impl(visitor);
    }

private:
    std::tuple<std::vector<Types>...> m_data{};

    /* Internal helpers */

    template<typename F>
    void for_each(F&& f)
    {
        std::apply([&](auto&... vecs) -> auto { (f(vecs), ...); }, m_data);
    }
    template<typename F>
    void for_each(F&& f) const
    {
        std::apply([&](const auto&... vecs) -> auto { (f(vecs), ...); }, m_data);
    }

    template<typename Ptr, std::size_t... Is>
    static auto make_zip_begin(Ptr* ptr, std::index_sequence<Is...>)
    {
        return ZipIterator<Ptr*, Is...>{ptr, 0};
    }
    template<typename Ptr, std::size_t... Is>
    static auto make_zip_end(Ptr* ptr, std::index_sequence<Is...>)
    {
        return ZipIterator<Ptr*, Is...>{ptr, ptr->size()};
    }
    template<typename Ptr, std::size_t... Is>
    static auto make_zip_range(Ptr* ptr, std::index_sequence<Is...>)
    {
        return ZipRange<Ptr*, Is...>{ptr, ptr->size()};
    }

    // Dispatch visit() using the argument types of V::visit,
    // reordering columns to match the visitor's expected order.
    template<typename V>
    void visit_impl(V& visitor)
    {
        visit_by_types(visitor, (visit_arg_types_t<V>*) nullptr, std::make_index_sequence<std::tuple_size_v<visit_arg_types_t<V>>>{});
    }
    template<typename V>
    void visit_impl(V& visitor) const
    {
        visit_by_types(visitor, (visit_arg_types_t<V>*) nullptr, std::make_index_sequence<std::tuple_size_v<visit_arg_types_t<V>>>{});
    }

    template<typename V, typename... VisitTypes, std::size_t... Js>
    void visit_by_types(V& visitor, std::tuple<VisitTypes...>*, std::index_sequence<Js...>)
    {
        for (size_type i = 0; i < size(); ++i) {
            visitor.visit(std::get<type_index_v<std::tuple_element_t<Js, std::tuple<VisitTypes...>>, Types...>>(m_data)[i]...);
        }
    }
    template<typename V, typename... VisitTypes, std::size_t... Js>
    void visit_by_types(V& visitor, std::tuple<VisitTypes...>*, std::index_sequence<Js...>) const
    {
        for (size_type i = 0; i < size(); ++i) {
            visitor.visit(std::get<type_index_v<std::tuple_element_t<Js, std::tuple<VisitTypes...>>, Types...>>(m_data)[i]...);
        }
    }

    template<std::size_t... Is>
    _nodiscard auto at_impl(const size_type idx, std::index_sequence<Is...>)
    {
        return std::tie(std::get<Is>(m_data)[idx]...);
    }
    template<std::size_t... Is>
    _nodiscard auto at_impl(const size_type idx, std::index_sequence<Is...>) const
    {
        return std::tie(std::get<Is>(m_data)[idx]...);
    }

    template<typename... Ts>
    auto at_by_typesset_impl(const size_type idx, TypesSet<Ts...>)
    {
        return std::tie(std::get<type_index_v<Ts, Types...>>(m_data)[idx]...);
    }
    template<typename... Ts>
    auto at_by_typesset_impl(const size_type idx, TypesSet<Ts...>) const
    {
        return std::tie(std::get<type_index_v<Ts, Types...>>(m_data)[idx]...);
    }

    template<typename RefSet, typename... Ts>
    auto at_by_refset_impl(const size_type idx) -> RefSet
    {
        // Unpacks RefSet = std::tuple<Ts&...>, strips const for index lookup
        // but return type preserves const on each reference
        return [&]<typename... Us>(std::tuple<Us&...>*) -> RefSet {
            return RefSet{std::get<type_index_v<std::remove_const_t<Us>, Types...>>(m_data)[idx]...};
        }(static_cast<RefSet*>(nullptr));
    }

    template<typename Ptr, std::size_t... Is>
    friend class ZipIterator;
};

template<typename T>
class VectorTypesImpl;

template<typename... Ts>
class VectorTypesImpl<TypesSet<Ts...>>
{
public:
    using type = Vector<Ts...>;
};

template<typename T>
using VectorTypes = typename VectorTypesImpl<T>::type;

} // namespace Entity

#endif // JP_ENTITY_VECTOR_H
