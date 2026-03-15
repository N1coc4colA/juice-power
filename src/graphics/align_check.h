#ifndef ALIGN_CHECK_H
#define ALIGN_CHECK_H

#include <cstdint>
#include <type_traits>

#include <boost/pfr.hpp>
#include <boost/pfr/core.hpp>
#include <glm/glm.hpp>

#define GPU_EXPOSED(Type, Name) alignas(GPUChecks::std430_compliant<Type>()) Type Name

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
constexpr size_t padding_needed()
{
    constexpr size_t remainder = CurrentOffset % RequiredAlign;
    return remainder == 0 ? 0 : (RequiredAlign - remainder);
}

namespace details {

template<typename T, typename... Ts>
struct is_one_of : std::false_type
{};

template<typename T, typename First, typename... Rest>
struct is_one_of<T, First, Rest...> : std::conditional_t<std::is_same_v<T, First>, std::true_type, is_one_of<T, Rest...>>
{};

// Helper variable template
template<typename T, typename... Ts>
inline constexpr bool is_one_of_v = is_one_of<T, Ts...>::value;

} // namespace details

template<typename S, size_t Offset, typename T, size_t I>
constexpr bool is_std430_compliant()
{
    static_assert(false, "The type has not been matched as std430 compliant. Most likely because it is not registered, or not compatible.");
    return false;
}

template<Padding, size_t Offset, typename T, size_t I>
constexpr bool is_std430_compliant()
{
    return true;
}

template<typename S, size_t Offset, typename T, size_t I>
    requires details::is_one_of_v<S, int8_t, uint8_t>
constexpr bool is_std430_compliant()
{
    static_assert((Offset % 1) == 0, "The alignment of your type does not match std430.");
    return true;
}

template<typename S, size_t Offset, typename T, size_t I>
    requires details::is_one_of_v<S, int16_t, uint16_t>
constexpr bool is_std430_compliant()
{
    static_assert((Offset % 2) == 0, "The alignment of your type does not match std430.");
    return true;
}

template<typename S, size_t Offset, typename T, size_t I>
    requires details::is_one_of_v<S, int, float, uint, bool>
constexpr bool is_std430_compliant()
{
    static_assert((Offset % 4) == 0, "The alignment of your type does not match std430.");
    return true;
}

template<typename S, size_t Offset, typename T, size_t I>
    requires details::is_one_of_v<S, glm::vec2, glm::ivec2, glm::uvec2, glm::bvec2, VkDeviceAddress>
constexpr bool is_std430_compliant()
{
    static_assert((Offset % 8) == 0, "The alignment of your type does not match std430.");
    return true;
}

template<typename S, size_t Offset, typename T, size_t I>
    requires details::is_one_of_v<S, glm::vec3, glm::ivec3, glm::uvec3, glm::bvec3>
constexpr bool is_std430_compliant()
{
    static_assert((Offset % 16) == 0, "The alignment of your type does not match std430.");
    return true;
}

template<typename S, size_t Offset, typename T, size_t I>
    requires details::is_one_of_v<S, glm::vec4, glm::ivec4, glm::uvec4, glm::bvec4>
constexpr bool is_std430_compliant()
{
    static_assert((Offset % 16) == 0, "The alignment of your type does not match std430.");
    return true;
}

template<typename S, size_t Offset, typename T, size_t I>
    requires details::is_one_of_v<S, glm::mat2>
constexpr bool is_std430_compliant()
{
    static_assert((Offset % 8) == 0, "The alignment of your type does not match std430.");
    return true;
}

template<typename S, size_t Offset, typename T, size_t I>
    requires details::is_one_of_v<S, glm::mat3>
constexpr bool is_std430_compliant()
{
    static_assert((Offset % 16) == 0, "The alignment of your type does not match std430.");
    return true;
}

template<typename S, size_t Offset, typename T, size_t I>
    requires details::is_one_of_v<S, glm::mat4>
constexpr bool is_std430_compliant()
{
    static_assert((Offset % 16) == 0, "The alignment of your type does not match std430.");
    return true;
}

template<typename S>
constexpr size_t std430_compliant()
{
    static_assert(false, "The type has not been matched as std430 compliant. Most likely because it is not registered, or not compatible.");
    return false;
}

template<Padding>
constexpr size_t std430_compliant()
{
    return true;
}

template<typename S>
    requires details::is_one_of_v<S, int8_t, uint8_t>
constexpr size_t std430_compliant()
{
    return 1;
}

template<typename S>
    requires details::is_one_of_v<S, int16_t, uint16_t>
constexpr size_t std430_compliant()
{
    return 2;
}

template<typename S>
    requires details::is_one_of_v<S, int, float, uint, bool>
constexpr size_t std430_compliant()
{
    return 4;
}

template<typename S>
    requires details::is_one_of_v<S, glm::vec2, glm::ivec2, glm::uvec2, glm::bvec2, VkDeviceAddress>
constexpr size_t std430_compliant()
{
    return 8;
}

template<typename S>
    requires details::is_one_of_v<S, glm::vec3, glm::ivec3, glm::uvec3, glm::bvec3>
constexpr size_t std430_compliant()
{
    return 16;
}

template<typename S>
    requires details::is_one_of_v<S, glm::vec4, glm::ivec4, glm::uvec4, glm::bvec4>
constexpr size_t std430_compliant()
{
    return 16;
}

template<typename S>
    requires details::is_one_of_v<S, glm::mat2>
constexpr size_t std430_compliant()
{
    return 8;
}

template<typename S>
    requires details::is_one_of_v<S, glm::mat3>
constexpr size_t std430_compliant()
{
    return 16;
}

template<typename S>
    requires details::is_one_of_v<S, glm::mat4>
constexpr size_t std430_compliant()
{
    return 16;
}

template<typename S, size_t Offset>
constexpr bool validate_struct_members()
{
    bool valid = true;
    size_t current_offset = Offset;
    boost::pfr::for_each_field(S{}, [&]<size_t I>(auto& field) -> void {
        using FieldType = std::remove_cvref_t<decltype(field)>;
        // align current_offset, then validate
        current_offset += padding_needed<current_offset, std430_compliant<FieldType>()>();
        valid &= is_std430_compliant<FieldType, current_offset, S, I>();
        current_offset += sizeof(FieldType);
    });
    return valid;
}

} // namespace GPUChecks

#endif // ALIGN_CHECK_H
