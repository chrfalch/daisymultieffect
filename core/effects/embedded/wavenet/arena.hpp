/**
 * @file arena.hpp
 * @brief Static memory arena for WaveNet inference
 *
 * Modified from GuitarML/Mercury's arena.hpp to use static std::array backing
 * instead of dynamic std::vector. This eliminates heap allocation on embedded.
 *
 * Original: https://github.com/GuitarML/Mercury/blob/main/wavenet/arena.hpp
 * License: MIT
 */
#pragma once

#include <array>
#include <cassert>
#include <cstddef>

namespace wavenet
{

    /**
     * Returns the next pointer with a given byte alignment,
     * or the base pointer if it is already aligned.
     */
    template <typename Type, typename Integer_Type>
    Type *snap_pointer_to_alignment(Type *base_pointer,
                                    Integer_Type alignment_bytes) noexcept
    {
        return (Type *)((((size_t)base_pointer) + (alignment_bytes - 1)) & ~(alignment_bytes - 1));
    }

    static constexpr int default_byte_alignment = 16;

    /**
     * A simple memory arena backed by a static std::array.
     * Size is determined at compile time from template parameter.
     *
     * For sample-by-sample WaveNet processing (N=1), this is sufficient
     * and avoids any dynamic allocation.
     */
    template <size_t Size>
    class Static_Arena
    {
    public:
        Static_Arena() = default;

        Static_Arena(const Static_Arena &) = delete;
        Static_Arena &operator=(const Static_Arena &) = delete;

        /**
         * Moves the allocator "stack pointer" back to zero,
         * effectively "reclaiming" all allocated memory.
         */
        void clear() noexcept
        {
            bytes_used = 0;
        }

        /** Returns the number of bytes currently being used */
        [[nodiscard]] size_t get_bytes_used() const noexcept { return bytes_used; }

        /** Returns the total capacity in bytes */
        [[nodiscard]] static constexpr size_t capacity() noexcept { return Size; }

        /**
         * Allocates a given number of bytes.
         * The returned memory will be un-initialized, so be sure to clear it manually
         * if needed.
         */
        void *allocate_bytes(size_t num_bytes, size_t alignment = 1)
        {
            auto *pointer = snap_pointer_to_alignment(raw_data.data() + bytes_used, alignment);
            const auto bytes_increment = static_cast<size_t>(std::distance(raw_data.data() + bytes_used, pointer + num_bytes));

            if (bytes_used + bytes_increment > raw_data.size())
            {
                assert(false && "Static arena overflow!");
                return nullptr;
            }

            bytes_used += bytes_increment;
            return pointer;
        }

        /**
         * Allocates space for some number of objects of type T
         * The returned memory will be un-initialized, so be sure to clear it manually
         * if needed.
         */
        template <typename T, typename IntType>
        T *allocate(IntType num_Ts, size_t alignment = alignof(T))
        {
            return static_cast<T *>(allocate_bytes((size_t)num_Ts * sizeof(T), alignment));
        }

        /** Returns a pointer to the internal buffer with a given offset in bytes */
        template <typename T, typename IntType>
        T *data(IntType offset_bytes) noexcept
        {
            return reinterpret_cast<T *>(raw_data.data() + offset_bytes);
        }

        /**
         * Creates a "frame" for the allocator.
         * Once the frame goes out of scope, the allocator will be reset
         * to whatever its state was at the beginning of the frame.
         */
        struct Frame
        {
            Frame() = default;
            explicit Frame(Static_Arena &allocator)
                : alloc(&allocator), bytes_used_at_start(alloc->bytes_used) {}

            ~Frame()
            {
                if (alloc)
                    alloc->bytes_used = bytes_used_at_start;
            }

            Static_Arena *alloc = nullptr;
            size_t bytes_used_at_start = 0;
        };

        /** Creates a frame for this allocator */
        auto create_frame() { return Frame{*this}; }

        void reset_to_frame(const Frame &frame)
        {
            assert(frame.alloc == this);
            bytes_used = frame.bytes_used_at_start;
        }

    private:
        std::array<std::byte, Size> raw_data{};
        size_t bytes_used = 0;
    };

    /**
     * Compatibility alias for code that expects Memory_Arena<>.
     * For sample-by-sample processing (N=1), 512 bytes is more than enough
     * for Pico models (channels=2).
     */
    template <typename = void>
    using Memory_Arena = Static_Arena<512>;

} // namespace wavenet
