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
concept Visitor = requires(V &v, const V &cv, Types &...args, const Types &...cargs) {
    { v.visit(args...) };
    { cv.visit(cargs...) };
};

template<typename T, typename... Types>
consteval auto typeIndexVImpl() -> std::size_t
{
    std::size_t i = 0;
    bool found = false;
    ((found || (std::is_same_v<T, Types> ? (found = true) : (++i, false))), ...);
    return found ? i : sizeof...(Types);
    // sizeof...(Types) = not-found sentinel
}

template<typename T, typename... Types>
consteval auto checkedTypeIndexV() -> std::size_t
{
    constexpr std::size_t idx = typeIndexVImpl<T, Types...>();
    static_assert(idx < sizeof...(Types), "Type not found in Vector's type list");
    return idx;
}

template<typename T, typename... Types>
inline constexpr std::size_t typeIndexV = checkedTypeIndexV<T, Types...>();

template<typename T>
struct IsTypesSet : std::false_type
{};
template<typename... Ts>
struct IsTypesSet<TypesSet<Ts...>> : std::true_type
{};

// Trait to detect ReferencesSet (std::tuple<Ts&...>) and extract Ts...
template<typename T>
struct IsReferencesSet : std::false_type
{};
template<typename... Ts>
struct IsReferencesSet<std::tuple<Ts &...>> : std::true_type
{};

// Trait to extract argument types from V::visit member function
template<typename F>
struct VisitArgs;

template<typename C, typename... Args>
struct VisitArgs<void (C::*)(Args...)>
{
    using types = std::tuple<std::remove_cvref_t<Args>...>;
};

template<typename C, typename... Args>
struct VisitArgs<void (C::*)(Args...) const>
{
    using types = std::tuple<std::remove_cvref_t<Args>...>;
};

template<typename V>
using VisitArgTypes_t = typename VisitArgs<decltype(&V::visit)>::types;

template<typename StoragePtr, std::size_t... Is>
class ZipIterator
{
public:
    using difference_type = std::ptrdiff_t;
    using iterator_category = std::random_access_iterator_tag;
    using value_type = std::remove_cvref_t<decltype(std::declval<StoragePtr>()->atImpl(0, std::index_sequence<Is...>{}))>;
    using reference = decltype(std::declval<StoragePtr>()->atImpl(0, std::index_sequence<Is...>{}));
    using pointer = void;

    ZipIterator() = default;
    ZipIterator(StoragePtr storage, const std::size_t idx)
        : m_storage(storage)
        , m_idx(idx)
    {}

    auto operator*() const { return m_storage->atImpl(m_idx, std::index_sequence<Is...>{}); }

    auto operator++() -> ZipIterator &
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
    auto operator--() -> ZipIterator &
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

    auto operator+=(const difference_type n) -> ZipIterator &
    {
        m_idx += n;
        return *this;
    }
    auto operator-=(const difference_type n) -> ZipIterator &
    {
        m_idx -= n;
        return *this;
    }
    auto operator+(const difference_type n) const { return ZipIterator(m_storage, m_idx + n); }
    auto operator-(const difference_type n) const { return ZipIterator(m_storage, m_idx - n); }
    auto operator-(const ZipIterator &o) const { return static_cast<difference_type>(m_idx) - static_cast<difference_type>(o.m_idx); }

    auto operator[](const difference_type n) const { return m_storage->atImpl(m_idx + n, std::index_sequence<Is...>{}); }

    auto operator==(const ZipIterator &o) const -> bool { return m_idx == o.m_idx; }
    auto operator!=(const ZipIterator &o) const -> bool { return m_idx != o.m_idx; }
    auto operator<(const ZipIterator &o) const -> bool { return m_idx < o.m_idx; }
    auto operator<=(const ZipIterator &o) const -> bool { return m_idx <= o.m_idx; }
    auto operator>(const ZipIterator &o) const -> bool { return m_idx > o.m_idx; }
    auto operator>=(const ZipIterator &o) const -> bool { return m_idx >= o.m_idx; }

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

    auto begin() { return iterator{m_storage, 0}; }
    _nodiscard auto begin() const { return iterator{m_storage, 0}; }
    auto end() { return iterator{m_storage, m_size}; }
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

    Vector() = default;

    explicit Vector(const Vector &other)
        : m_data(other.m_data)
    {}
    explicit Vector(Vector &&other) noexcept
        : m_data(std::move(other.m_data))
    {}

    ~Vector() = default;

    auto operator=(const Vector &other) -> Vector & = default;
    auto operator=(Vector &&other) noexcept -> Vector &
    {
        if (this != &other)
            m_data = std::move(other.m_data);
        return *this;
    }

    /* Element access */

    auto at(const size_type idx) -> std::tuple<Types &...> { return atImpl(idx, std::index_sequence_for<Types...>{}); }
    _nodiscard auto at(const size_type idx) const -> std::tuple<const Types &...> { return atImpl(idx, std::index_sequence_for<Types...>{}); }

    // By single type — returns T&
    template<typename T>
        requires(!IsTypesSet<T>::value && !IsReferencesSet<T>::value)
    auto at(const size_type idx) -> decltype(auto)
    {
        return std::get<typeIndexV<T, Types...>>(m_data)[idx];
    }
    template<typename T>
        requires(!IsTypesSet<T>::value && !IsReferencesSet<T>::value)
    _nodiscard auto at(const size_type idx) const -> decltype(auto)
    {
        return std::get<typeIndexV<T, Types...>>(m_data)[idx];
    }

    // By multiple types — returns std::tuple<T1&, T2&, ...>
    template<typename T1, typename T2, typename... Rest>
    auto at(const size_type idx)
    {
        return atByTypesSetImpl(idx, TypesSet<T1, T2, Rest...>{});
    }
    template<typename T1, typename T2, typename... Rest>
    auto at(const size_type idx) const
    {
        return atByTypesSetImpl(idx, TypesSet<T1, T2, Rest...>{});
    }

    // By TypesSet — returns std::tuple<Ts&...>
    template<typename TSet>
        requires IsTypesSet<TSet>::value
    auto at(const size_type idx)
    {
        return atByTypesSetImpl(idx, TSet{});
    }
    template<typename TSet>
        requires IsTypesSet<TSet>::value
    auto at(const size_type idx) const
    {
        return atByTypesSetImpl(idx, TSet{});
    }

    // By ReferencesSet — returns std::tuple<Ts&...> with const preserved
    template<typename RefSet>
        requires IsReferencesSet<RefSet>::value
    auto at(const size_type idx) -> RefSet
    {
        return atByRefSetImpl<RefSet>(idx);
    }

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

    void reserve(const size_type newCap)
    {
        forEach([newCap](auto &v) -> auto { v.reserve(newCap); });
    }

    void shrinkToFit()
    {
        forEach([](auto &v) -> auto { v.shrink_to_fit(); });
    }

    void shrink_to_fit()
    {
        shrinkToFit();
    }

    /* Modifiers */

    template<typename... Args>
    void emplaceBack(Args &&...args)
    {
        [&]<std::size_t... Is>(std::index_sequence<Is...>) -> auto {
            (std::get<Is>(m_data).emplace_back(std::forward<Args>(args)), ...);
        }(std::index_sequence_for<Types...>{});
    }

    template<typename... Args>
    void emplace_back(Args &&...args)
    {
        emplaceBack(std::forward<Args>(args)...);
    }

    void pushBack(const Types &...values)
    {
        [&]<std::size_t... Is>(std::index_sequence<Is...>) -> auto {
            (std::get<Is>(m_data).push_back(values), ...);
        }(std::index_sequence_for<Types...>{});
    }
    void pushBack(Types &&...values)
    {
        [&]<std::size_t... Is>(std::index_sequence<Is...>) -> auto {
            (std::get<Is>(m_data).push_back(std::move(values)), ...);
        }(std::index_sequence_for<Types...>{});
    }

    void push_back(const Types &...values)
    {
        pushBack(values...);
    }
    void push_back(Types &&...values)
    {
        pushBack(std::move(values)...);
    }

    void popBack()
    {
        if (!empty())
            forEach([](auto &v) -> auto { v.pop_back(); });
    }

    void pop_back()
    {
        popBack();
    }

    void clear() noexcept
    {
        forEach([](auto &v) -> auto { v.clear(); });
    }

    void resize(const size_type count)
    {
        forEach([count](auto &v) -> auto { v.resize(count); });
    }
    void resize(const size_type count, const Types &...values)
    {
        [&]<std::size_t... Is>(std::index_sequence<Is...>) -> auto {
            (std::get<Is>(m_data).resize(count, values), ...);
        }(std::index_sequence_for<Types...>{});
    }

    /* Iterators — all columns in declaration order */

    auto begin() { return makeZipBegin(this, std::index_sequence_for<Types...>{}); }
    auto end() { return makeZipEnd(this, std::index_sequence_for<Types...>{}); }
    _nodiscard auto begin() const { return makeZipBegin(this, std::index_sequence_for<Types...>{}); }
    _nodiscard auto end() const { return makeZipEnd(this, std::index_sequence_for<Types...>{}); }
    _nodiscard auto cbegin() const { return begin(); }
    _nodiscard auto cend() const { return end(); }

    /* Subset range: vec.range<B, A>() iterates columns B and A (in that order) */

    template<typename... SubTypes>
    auto range() &
    {
        return makeZipRange(this, std::index_sequence<typeIndexV<SubTypes, Types...>...>{});
    }
    template<typename... SubTypes>
    _nodiscard auto range() const &
    {
        return makeZipRange(this, std::index_sequence<typeIndexV<SubTypes, Types...>...>{});
    }

    /* Single-column view */

    template<std::size_t I>
    auto get() &
    {
        return std::get<I>(m_data);
    }
    template<std::size_t I>
    auto get() const &
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
    void visit(V &visitor)
    {
        visitImpl(visitor);
    }
    template<typename V>
    void visit(V &visitor) const
    {
        visitImpl(visitor);
    }
    template<typename V>
    void visit(const V &visitor)
    {
        visitImpl(visitor);
    }
    template<typename V>
    void visit(const V &visitor) const
    {
        // A const visitor does not imply const data; remove data constness
        // by forwarding to the non-const this. Only valid if the visitor
        // itself is const (i.e. it won't modify the Vector).
        const_cast<Vector *>(this)->visitImpl(visitor);
    }

private:
    std::tuple<std::vector<Types>...> m_data{};

    /* Internal helpers */

    template<typename F>
    void forEach(F &&f)
    {
        std::apply([&](auto &...vecs) -> auto { (f(vecs), ...); }, m_data);
    }
    template<typename F>
    void forEach(F &&f) const
    {
        std::apply([&](const auto &...vecs) -> auto { (f(vecs), ...); }, m_data);
    }

    template<typename Ptr, std::size_t... Is>
    static auto makeZipBegin(Ptr *ptr, std::index_sequence<Is...>)
    {
        return ZipIterator<Ptr *, Is...>{ptr, 0};
    }
    template<typename Ptr, std::size_t... Is>
    static auto makeZipEnd(Ptr *ptr, std::index_sequence<Is...>)
    {
        return ZipIterator<Ptr *, Is...>{ptr, ptr->size()};
    }
    template<typename Ptr, std::size_t... Is>
    static auto makeZipRange(Ptr *ptr, std::index_sequence<Is...>)
    {
        return ZipRange<Ptr *, Is...>{ptr, ptr->size()};
    }

    template<typename V>
    void visitImpl(V &visitor)
    {
        visitByTypes(visitor, (VisitArgTypes_t<V> *) nullptr, std::make_index_sequence<std::tuple_size_v<VisitArgTypes_t<V>>>{});
    }
    template<typename V>
    void visitImpl(V &visitor) const
    {
        visitByTypes(visitor, (VisitArgTypes_t<V> *) nullptr, std::make_index_sequence<std::tuple_size_v<VisitArgTypes_t<V>>>{});
    }

    template<typename V, typename... VisitTypes, std::size_t... Js>
    void visitByTypes(V &visitor, std::tuple<VisitTypes...> *, std::index_sequence<Js...>)
    {
        for (size_type i = 0; i < size(); ++i) {
            visitor.visit(std::get<typeIndexV<std::tuple_element_t<Js, std::tuple<VisitTypes...>>, Types...>>(m_data)[i]...);
        }
    }
    template<typename V, typename... VisitTypes, std::size_t... Js>
    void visitByTypes(V &visitor, std::tuple<VisitTypes...> *, std::index_sequence<Js...>) const
    {
        for (size_type i = 0; i < size(); ++i) {
            visitor.visit(std::get<typeIndexV<std::tuple_element_t<Js, std::tuple<VisitTypes...>>, Types...>>(m_data)[i]...);
        }
    }

    template<std::size_t... Is>
    _nodiscard auto atImpl(const size_type idx, std::index_sequence<Is...>)
    {
        return std::tie(std::get<Is>(m_data)[idx]...);
    }
    template<std::size_t... Is>
    _nodiscard auto atImpl(const size_type idx, std::index_sequence<Is...>) const
    {
        return std::tie(std::get<Is>(m_data)[idx]...);
    }

    template<typename... Ts>
    auto atByTypesSetImpl(const size_type idx, TypesSet<Ts...>)
    {
        return std::tie(std::get<typeIndexV<Ts, Types...>>(m_data)[idx]...);
    }
    template<typename... Ts>
    auto atByTypesSetImpl(const size_type idx, TypesSet<Ts...>) const
    {
        return std::tie(std::get<typeIndexV<Ts, Types...>>(m_data)[idx]...);
    }

    template<typename RefSet, typename... Ts>
    auto atByRefSetImpl(const size_type idx) -> RefSet
    {
        return [&]<typename... Us>(std::tuple<Us &...> *) -> RefSet {
            return RefSet{std::get<typeIndexV<std::remove_const_t<Us>, Types...>>(m_data)[idx]...};
        }(static_cast<RefSet *>(nullptr));
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
