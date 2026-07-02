#pragma once
#include "pa_helpers.hpp"

#include <cstddef>

namespace allocation {

template <typename _T, size_t _Capacity,
          typename _Base = allocation::heap_base<_T, _Capacity>>
  requires allocation::is_allocator_base<_Base, _T, _Capacity>
class pool_allocator : public _Base {
  struct link_t {
    void *next;
  } free_;

public:
  using Type = _T;
  static_assert(_Capacity > 0, "_Capacity must be greater than zero.");

public:
  pool_allocator() : _Base() {
    free_.next = _Base::memory_;
    link_t *link = (link_t *)_Base::memory_;
    for (size_t i = 0; i < _Capacity - 1; ++i) {
      link->next = (void *)((size_t)link + _Base::chunk_size);
      link = (link_t *)link->next;
    }
    link->next = nullptr;
  }

  Type *alloc() {
    if (free_.next == nullptr)
      return nullptr;

    Type *ret = (Type *)((size_t)free_.next + sizeof(void *));
    link_t *link = (link_t *)free_.next;
    free_.next = link->next;
    return ret;
  }

  void free(Type *elem) {
    link_t *link = (link_t *)((size_t)elem - sizeof(void *));
    link->next = free_.next;
    free_.next = (void *)link;
  }
};

template <typename _T, size_t _Capacity>
using pool_allocator_heap =
    pool_allocator<_T, _Capacity, allocation::heap_base<_T, _Capacity>>;

template <typename _T, size_t _Capacity>
using pool_allocator_stack =
    pool_allocator<_T, _Capacity, allocation::stack_base<_T, _Capacity>>;

} // namespace allocation