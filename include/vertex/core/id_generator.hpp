#pragma once

#include <atomic>
#include <cstdint>
#include <type_traits>
#include "vertex/core/strong_id.hpp"

namespace vertex::core
{
    template<typename>
    struct is_strong_id : std::false_type {}; //default, for all types is_strong_id = false
    
    template<typename Tag>
    struct is_strong_id<StrongId<Tag>> : std::true_type{}; //for StrongId<Tag> types is_strong_id = true
    


    template <typename T>
    class IdGenerator
    {
        static_assert(
            is_strong_id<T>::value, 
            "T must be StrongId");   //check of template typename

    private:
        std::atomic<std::uint64_t> counter{0};

    public:
        IdGenerator() = default;
        T next();
    };

    template <typename T>
    T IdGenerator<T>::next()
    {
        auto value = counter.fetch_add(1, std::memory_order_relaxed) + 1;
        return T{value};
    }

} // namespace vertex::core
