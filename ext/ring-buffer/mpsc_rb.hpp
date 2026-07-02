#pragma once

#include <atomic>
#include <cstddef>
#include <optional>

#include "rb_helpers.hpp"

namespace concurrency {

template <typename _T, size_t _Capacity, typename _PtrType = size_t,
          typename _Base = ring_buffer::stack_base<_T, _Capacity, _PtrType>>
class mpsc_ring_buffer
    : public ring_buffer::access_helpers<_T, _Capacity, _PtrType, _Base> {
  std::atomic<_PtrType> start_{0};
  _Base::cache_pad pad1_;
  std::atomic<_PtrType> end_{0};
  // if pend is changed, changing and reading start and pend should not cause
  // local CPU cache eviction
  _Base::cache_pad pad2_;
  // producer end that can grow faster than end
  std::atomic<_PtrType> pend_{0};

  using access = ring_buffer::access_helpers<_T, _Capacity, _PtrType, _Base>;

public:
  /**
   * pushes an item into the ring-buffer queue
   *
   * @arg{item} the item to push
   * @return true if pushed, false if capacity reached
   */
  bool push(_T item) {
    auto p = pend_.load(std::memory_order::relaxed);
    do {
      // p is loaded the first time, then it gets updated in CAS
      if (p - start_.load(std::memory_order::relaxed) == _Capacity)
        return false;
      // we acquire a slot using pend, but we fail if capacity reached
      // we use weak compare-exchange, because false failures are not an issue
      // do not let the item memory store move before this point
    } while (not pend_.compare_exchange_weak(
        p, p + 1, std::memory_order::acquire, std::memory_order::relaxed));
    // we want to work with movable types, but we copy non-movable
    if constexpr (std::is_move_assignable_v<_T> and
                  std::is_move_constructible_v<_T>) {
      access::set_mod(p, std::move(item));
    } else {
      access::set_mod(p, item);
    }
    // make sure we increase end in the right order
    const auto b = p;
    // make sure we don't repeat increment unnecessarily, p1 does not need to
    // change
    const auto p1 = p + 1;
    // wait for end to grow to p (previous items all constructed)
    while (not end_.compare_exchange_weak(p, p1, std::memory_order::release,
                                          std::memory_order::relaxed)) {
      // p changes to end, reset to our b
      p = b;
    }
    // do not let the item memory store move beyond this point
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
using mpsc_ring_buffer_heap =
    mpsc_ring_buffer<_T, _Capacity, _PtrType,
                     ring_buffer::heap_base<_T, _Capacity, _PtrType>>;

template <typename _T, size_t _Capacity, typename _PtrType = size_t>
using mpsc_ring_buffer_stack =
    mpsc_ring_buffer<_T, _Capacity, _PtrType,
                     ring_buffer::stack_base<_T, _Capacity, _PtrType>>;

} // namespace concurrency