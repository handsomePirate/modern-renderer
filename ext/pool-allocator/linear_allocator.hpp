#pragma once
#include "pa_helpers.hpp"

#include <cstddef>

namespace allocation {

template <size_t _Capacity,
          typename _Base = allocation::heap_base<uint8_t, _Capacity>>
  requires allocation::is_allocator_base<_Base, uint8_t, _Capacity>
class linear_allocator : public _Base {
  size_t ptr_ = 0;

public:
  static_assert(_Capacity > 0, "_Capacity must be greater than zero.");

public:
  linear_allocator() : _Base() {}

  void *alloc(size_t size, size_t alignment = 1) {
    if (size == 0 || alignment == 0)
      return nullptr;
    size_t aligned = (ptr_ + alignment - 1) & ~(alignment - 1);
    if (aligned + size > _Capacity)
      return nullptr;
    void *mem = (void *)((size_t)_Base::memory_ + aligned);
    ptr_ = aligned + size;
    return mem;
  }

  void reset() { ptr_ = 0; }

  size_t size() { return ptr_; }
};

template <size_t _Capacity>
using linear_allocator_heap =
    linear_allocator<_Capacity,
                     allocation::heap_base<unsigned char, _Capacity>>;

template <size_t _Capacity>
using linear_allocator_stack =
    linear_allocator<_Capacity,
                     allocation::stack_base<unsigned char, _Capacity>>;

} // namespace allocation