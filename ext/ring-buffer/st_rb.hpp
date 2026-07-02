#pragma once

#include <cstddef>
#include <optional>

#include "rb_helpers.hpp"

namespace ring_buffer {

template <typename _T, size_t _Capacity, typename _PtrType = size_t,
          typename _Base = ring_buffer::stack_base<_T, _Capacity, _PtrType>>
  requires ring_buffer::is_buffer_base<_Base, _T, _Capacity, _PtrType>
class st_ring_buffer
    : public ring_buffer::access_helpers<_T, _Capacity, _PtrType, _Base> {
  _PtrType start_{0};
  _PtrType end_{0};

  using access = ring_buffer::access_helpers<_T, _Capacity, _PtrType, _Base>;

public:
  /**
   * pushes an item into the ring-buffer queue
   *
   * @arg{item} the item to push
   * @return true if pushed, false if capacity reached
   */
  bool push(_T item) {
    if (end_ - start_ == _Capacity)
      return false;

    if constexpr (std::is_move_assignable_v<_T> and
                  std::is_move_constructible_v<_T>) {
      access::set_mod(end_++, std::move(item));
    } else {
      access::set_mod(end_++, item);
    }

    return true;
  }

  [[nodiscard]]
  /**
   * pops an item from the ring-buffer queue
   *
   * @return item if non-empty, nullopt if empty
   */
  std::optional<_T> pop() {
    if (start_ == end_)
      return std::nullopt;

    return access::take_opt_mod(start_++);
  }

  [[nodiscard]]
  /**
   * gives the number of items currently in the queue
   *
   * @return number of items in the queue
   */
  inline size_t size() const {
    return size_t(end_ - start_);
  }

  inline bool is_empty() const { return start_ == end_; }
};

template <typename _T, size_t _Capacity, typename _PtrType = size_t>
using st_ring_buffer_heap =
    st_ring_buffer<_T, _Capacity, _PtrType,
                   ring_buffer::heap_base<_T, _Capacity, _PtrType>>;

template <typename _T, size_t _Capacity, typename _PtrType = size_t>
using st_ring_buffer_stack =
    st_ring_buffer<_T, _Capacity, _PtrType,
                   ring_buffer::stack_base<_T, _Capacity, _PtrType>>;

} // namespace ring_buffer