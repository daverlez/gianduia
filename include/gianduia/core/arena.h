#pragma once
#include <memory>
#include <vector>
#include <algorithm>
#include <utility>
#include <new>

namespace gnd {

class MemoryArena {
public:
    explicit MemoryArena(std::size_t block_size = 1024 * 1024)
        : m_block_size(block_size) {
        allocate_new_block(m_block_size);
    }

    ~MemoryArena() {
        clear();
        for (char* ptr : m_used_blocks) {
            ::operator delete(ptr);
        }
        ::operator delete(m_current_block_start);
    }

    MemoryArena(const MemoryArena&) = delete;
    MemoryArena& operator=(const MemoryArena&) = delete;

    MemoryArena(MemoryArena&& other) noexcept
        : m_current_block_start(std::exchange(other.m_current_block_start, nullptr))
        , m_current_ptr(std::exchange(other.m_current_ptr, nullptr))
        , m_current_end(std::exchange(other.m_current_end, nullptr))
        , m_block_size(other.m_block_size)
        , m_used_blocks(std::move(other.m_used_blocks))
        , m_dtor_chain(std::exchange(other.m_dtor_chain, nullptr)) {}

    template <typename T, typename... Args>
    T* create(Args&&... args) {
        if constexpr (std::is_trivially_destructible_v<T>) {
            // Trivial destruction: avoid storing destruction node
            void* ptr = alloc_bytes(sizeof(T), alignof(T));
            return new (ptr) T(std::forward<Args>(args)...);
        } else {
            // Non-trivial destruction: store destruction node
            void* ptr = alloc_bytes(sizeof(T), alignof(T));
            T* obj = new (ptr) T(std::forward<Args>(args)...);

            auto dtor_func = [](void* p) { static_cast<T*>(p)->~T(); };
            void* node_mem = alloc_bytes(sizeof(DestructNode), alignof(DestructNode));

            m_dtor_chain = new (node_mem) DestructNode{dtor_func, m_dtor_chain, obj};

            return obj;
        }
    }

    void reset() {
        clear();

        for (char* ptr : m_used_blocks) {
            ::operator delete(ptr);
        }
        m_used_blocks.clear();
        m_current_ptr = m_current_block_start;
    }

private:
    struct DestructNode {
        void (*dtor)(void*);
        DestructNode* prev;
        void* obj;
    };

    char* m_current_block_start = nullptr;
    char* m_current_ptr = nullptr;
    char* m_current_end = nullptr;
    std::size_t m_block_size;

    std::vector<char*> m_used_blocks;
    DestructNode* m_dtor_chain = nullptr;

    void clear() {
        while (m_dtor_chain) {
            m_dtor_chain->dtor(m_dtor_chain->obj);
            m_dtor_chain = m_dtor_chain->prev;
        }
    }

    void* alloc_bytes(std::size_t size, std::size_t alignment) {
        std::size_t space = m_current_end - m_current_ptr;
        void* ptr = m_current_ptr;

        if (std::align(alignment, size, ptr, space)) {
            m_current_ptr = static_cast<char*>(ptr) + size;
            return ptr;
        }

        std::size_t new_size = std::max(m_block_size, size + alignment);
        m_used_blocks.push_back(m_current_block_start);

        allocate_new_block(new_size);

        space = m_current_end - m_current_ptr;
        ptr = m_current_ptr;
        if (!std::align(alignment, size, ptr, space)) {
            throw std::bad_alloc();
        }

        m_current_ptr = static_cast<char*>(ptr) + size;
        return ptr;
    }

    void allocate_new_block(std::size_t size) {
        m_current_block_start = static_cast<char*>(::operator new(size));
        m_current_ptr = m_current_block_start;
        m_current_end = m_current_block_start + size;
        m_block_size = size;
    }
};

}