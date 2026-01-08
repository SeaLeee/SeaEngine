#pragma once

#include <cstdint>
#include <memory>
#include <string>
#include <vector>
#include <unordered_map>
#include <functional>
#include <optional>
#include <variant>
#include <span>
#include <filesystem>

namespace Sea
{
    // 基础类型别名
    using u8 = uint8_t;
    using u16 = uint16_t;
    using u32 = uint32_t;
    using u64 = uint64_t;
    using i8 = int8_t;
    using i16 = int16_t;
    using i32 = int32_t;
    using i64 = int64_t;
    using f32 = float;
    using f64 = double;

    // 智能指针别名
    template<typename T>
    using Ref = std::shared_ptr<T>;

    template<typename T>
    using Scope = std::unique_ptr<T>;

    template<typename T>
    using WeakRef = std::weak_ptr<T>;

    template<typename T, typename... Args>
    constexpr Ref<T> MakeRef(Args&&... args)
    {
        return std::make_shared<T>(std::forward<Args>(args)...);
    }

    template<typename T, typename... Args>
    constexpr Scope<T> MakeScope(Args&&... args)
    {
        return std::make_unique<T>(std::forward<Args>(args)...);
    }

    // 资源句柄
    template<typename T>
    struct Handle
    {
        u32 index = 0;
        u32 generation = 0;

        bool IsValid() const { return generation != 0; }
        bool operator==(const Handle& other) const 
        { 
            return index == other.index && generation == other.generation; 
        }
    };

    // 结果类型
    template<typename T, typename E = std::string>
    using Result = std::variant<T, E>;

    template<typename T, typename E>
    bool IsOk(const Result<T, E>& result) { return std::holds_alternative<T>(result); }

    template<typename T, typename E>
    T& GetValue(Result<T, E>& result) { return std::get<T>(result); }

    template<typename T, typename E>
    const T& GetValue(const Result<T, E>& result) { return std::get<T>(result); }

    template<typename T, typename E>
    E& GetError(Result<T, E>& result) { return std::get<E>(result); }

    // 非拷贝基类
    class NonCopyable
    {
    protected:
        NonCopyable() = default;
        ~NonCopyable() = default;
        NonCopyable(const NonCopyable&) = delete;
        NonCopyable& operator=(const NonCopyable&) = delete;
    };

    // 枚举标志位操作
    template<typename E>
    struct EnableBitmaskOperators : std::false_type {};

    template<typename E>
    requires std::is_enum_v<E> && EnableBitmaskOperators<E>::value
    constexpr E operator|(E lhs, E rhs)
    {
        using underlying = std::underlying_type_t<E>;
        return static_cast<E>(static_cast<underlying>(lhs) | static_cast<underlying>(rhs));
    }

    template<typename E>
    requires std::is_enum_v<E> && EnableBitmaskOperators<E>::value
    constexpr E operator&(E lhs, E rhs)
    {
        using underlying = std::underlying_type_t<E>;
        return static_cast<E>(static_cast<underlying>(lhs) & static_cast<underlying>(rhs));
    }

    template<typename E>
    requires std::is_enum_v<E> && EnableBitmaskOperators<E>::value
    constexpr bool HasFlag(E value, E flag)
    {
        using underlying = std::underlying_type_t<E>;
        return (static_cast<underlying>(value) & static_cast<underlying>(flag)) != 0;
    }
}
