#ifndef ALIGN_CHECK_H
#define ALIGN_CHECK_H

#include <cstdint>
#include <type_traits>

#include <boost/pfr.hpp>
#include <boost/pfr/core.hpp>
#include <glm/glm.hpp>

#define GPU_EXPOSED(Type, Name) alignas(::GPUChecks::std430Compliant<Type>()) Type Name

// Vulkan
using VkDeviceAddress = uint64_t;

namespace GPUChecks {

template<size_t S>
struct Padding
{
private:
    std::byte __reserved[S];
} __attribute__((packed));

template<size_t CurrentOffset, size_t RequiredAlign>
constexpr auto paddingNeeded() -> size_t
{
    constexpr size_t remainder = CurrentOffset % RequiredAlign;
    return remainder == 0 ? 0 : (RequiredAlign - remainder);
}

namespace details {

template<typename T, typename... Ts>
struct IsOneOf : std::false_type
{};

template<typename T, typename First, typename... Rest>
struct IsOneOf<T, First, Rest...> : std::conditional_t<std::is_same_v<T, First>, std::true_type, IsOneOf<T, Rest...>>
{};

// Helper variable template
template<typename T, typename... Ts>
inline constexpr bool is_one_of_v = IsOneOf<T, Ts...>::value;

} // namespace details

template<Padding>
constexpr auto std430Compliant() -> size_t
{
    return true;
}

template<typename S>
    requires details::is_one_of_v<S, int8_t, uint8_t>
constexpr auto std430Compliant() -> size_t
{
    return 1;
}

template<typename S>
    requires details::is_one_of_v<S, int16_t, uint16_t>
constexpr auto std430Compliant() -> size_t
{
    return 2;
}

template<typename S>
    requires details::is_one_of_v<S, int, float, uint, bool>
constexpr auto std430Compliant() -> size_t
{
    return 4;
}

template<typename S>
    requires details::is_one_of_v<S, glm::vec2, glm::ivec2, glm::uvec2, glm::bvec2, VkDeviceAddress>
constexpr auto std430Compliant() -> size_t
{
    return 8;
}

template<typename S>
    requires details::is_one_of_v<S, glm::vec3, glm::ivec3, glm::uvec3, glm::bvec3>
constexpr auto std430Compliant() -> size_t
{
    return 16;
}

template<typename S>
    requires details::is_one_of_v<S, glm::vec4, glm::ivec4, glm::uvec4, glm::bvec4>
constexpr auto std430Compliant() -> size_t
{
    return 16;
}

template<typename S>
    requires details::is_one_of_v<S, glm::mat2>
constexpr auto std430Compliant() -> size_t
{
    return 8;
}

template<typename S>
    requires details::is_one_of_v<S, glm::mat3>
constexpr auto std430Compliant() -> size_t
{
    return 16;
}

template<typename S>
    requires details::is_one_of_v<S, glm::mat4>
constexpr auto std430Compliant() -> size_t
{
    return 16;
}

template<typename S, size_t Offset, typename T, size_t I>
constexpr auto isStd430Compliant()
{
    static_assert(false, "The type has not been matched as std430 compliant. Most likely because it is not registered, or not compatible.");
    return false;
}

template<Padding, size_t Offset, typename T, size_t I>
constexpr auto isStd430Compliant()
{
    return true;
}

template<typename S, size_t Offset, typename T, size_t I>
    requires details::is_one_of_v<S, int8_t, uint8_t>
constexpr auto isStd430Compliant()
{
    static_assert((Offset % std430Compliant<S>()) == 0, "The alignment of your type does not match std430.");
    return true;
}

template<typename S, size_t Offset, typename T, size_t I>
    requires details::is_one_of_v<S, int16_t, uint16_t>
constexpr auto isStd430Compliant()
{
    static_assert((Offset % std430Compliant<S>()) == 0, "The alignment of your type does not match std430.");
    return true;
}

template<typename S, size_t Offset, typename T, size_t I>
    requires details::is_one_of_v<S, int, float, uint, bool>
constexpr auto isStd430Compliant()
{
    static_assert((Offset % std430Compliant<S>()) == 0, "The alignment of your type does not match std430.");
    return true;
}

template<typename S, size_t Offset, typename T, size_t I>
    requires details::is_one_of_v<S, glm::vec2, glm::ivec2, glm::uvec2, glm::bvec2, VkDeviceAddress>
constexpr auto isStd430Compliant()
{
    static_assert((Offset % std430Compliant<S>()) == 0, "The alignment of your type does not match std430.");
    return true;
}

template<typename S, size_t Offset, typename T, size_t I>
    requires details::is_one_of_v<S, glm::vec3, glm::ivec3, glm::uvec3, glm::bvec3>
constexpr auto isStd430Compliant()
{
    static_assert((Offset % std430Compliant<S>()) == 0, "The alignment of your type does not match std430.");
    return true;
}

template<typename S, size_t Offset, typename T, size_t I>
    requires details::is_one_of_v<S, glm::vec4, glm::ivec4, glm::uvec4, glm::bvec4>
constexpr auto isStd430Compliant()
{
    static_assert((Offset % std430Compliant<S>()) == 0, "The alignment of your type does not match std430.");
    return true;
}

template<typename S, size_t Offset, typename T, size_t I>
    requires details::is_one_of_v<S, glm::mat2>
constexpr auto isStd430Compliant()
{
    static_assert((Offset % std430Compliant<S>()) == 0, "The alignment of your type does not match std430.");
    return true;
}

template<typename S, size_t Offset, typename T, size_t I>
    requires details::is_one_of_v<S, glm::mat3>
constexpr auto isStd430Compliant()
{
    static_assert((Offset % std430Compliant<S>()) == 0, "The alignment of your type does not match std430.");
    return true;
}

template<typename S, size_t Offset, typename T, size_t I>
    requires details::is_one_of_v<S, glm::mat4>
constexpr auto isStd430Compliant()
{
    static_assert((Offset % std430Compliant<S>()) == 0, "The alignment of your type does not match std430.");
    return true;
}

template<typename S>
constexpr auto std430Compliant() -> size_t
{
    static_assert(false, "The type has not been matched as std430 compliant. Most likely because it is not registered, or not compatible.");
    return false;
}

template<typename S, size_t Offset>
constexpr auto validateStructMembers()
{
    bool valid = true;

    size_t current_offset = Offset;
    boost::pfr::for_each_field(S{}, [&]<size_t I>(auto& field) -> void {
        using FieldType = std::remove_cvref_t<decltype(field)>;
        // align current_offset, then validate
        current_offset += paddingNeeded<current_offset, std430Compliant<FieldType>()>();
        valid &= isStd430Compliant<FieldType, current_offset, S, I>();
        current_offset += sizeof(FieldType);
    });

    return valid;
}

} // namespace GPUChecks

#endif // ALIGN_CHECK_H
