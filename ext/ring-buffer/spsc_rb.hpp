#pragma once

#include <atomic>
#include <cstddef>
#include <optional>

#include "rb_helpers.hpp"

namespace concurrency {

template <typename _T, size_t _Capacity, typename _PtrType = size_t,
          typename _Base = ring_buffer::stack_base<_T, _Capacity, _PtrType>>
  requires ring_buffer::is_buffer_base<_Base, _T, _Capacity, _PtrType>
class spsc_ring_buffer
    : public ring_buffer::access_helpers<_T, _Capacity, _PtrType, _Base> {
  std::atomic<_PtrType> start_{0};
  _Base::cache_pad pad1_;
  std::atomic<_PtrType> end_{0};

  using access = ring_buffer::access_helpers<_T, _Capacity, _PtrType, _Base>;

public:
  /**
   * pushes an item into the ring-buffer queue
   *
   * @arg{item} the item to push
   * @return true if pushed, false if capacity reached
   */
  bool push(_T item) {
    auto s = start_.load(std::memory_order::relaxed);
    auto e = end_.load(std::memory_order::relaxed);

    if (e - s == _Capacity)
      return false;

    if constexpr (std::is_move_assignable_v<_T> and
                  std::is_move_constructible_v<_T>) {
      access::set_mod(e, std::move(item));
    } else {
      access::set_mod(e, item);
    }

    end_.store(e + 1, std::memory_order::release);
    return true;
  }

  [[nodiscard]]
  /**
   * pops an item from the ring-buffer queue
   *
   * @return item if non-empty, nullopt if empty
   */
  std::optional<_T> pop() {
    auto s = start_.load(std::memory_order::relaxed);
    auto e = end_.load(std::memory_order::relaxed);

    if (s == e)
      return std::nullopt;

    auto item = access::take_opt_mod(s);

    // do not let the item memory load move beyond this point
    start_.store(s + 1, std::memory_order::release);
    // we expect copy-elision here so we do not move
    return item;
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
using spsc_ring_buffer_heap =
    spsc_ring_buffer<_T, _Capacity, _PtrType,
                     ring_buffer::heap_base<_T, _Capacity, _PtrType>>;

template <typename _T, size_t _Capacity, typename _PtrType = size_t>
using spsc_ring_buffer_stack =
    spsc_ring_buffer<_T, _Capacity, _PtrType,
                     ring_buffer::stack_base<_T, _Capacity, _PtrType>>;

} // namespace concurrency