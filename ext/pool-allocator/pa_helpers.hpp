#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <cstdlib>

namespace allocation {

template <typename _T> class allocator_checker {
public:
  static auto check(_T &obj) -> decltype(obj.memory_) { return obj.memory_; }
};

template <typename _T, size_t _Capacity> class base {
public:
  using element_type = _T;
  static constexpr const size_t chunk_size = sizeof(void *) + sizeof(_T);
  static constexpr const size_t capacity = _Capacity;
  static constexpr const size_t mem_size = chunk_size * capacity;
};

template <typename _T, size_t _Capacity>
class stack_base : public base<_T, _Capacity> {
protected:
  std::array<uint8_t, base<_T, _Capacity>::mem_size> buffer_;
  void *memory_ = &buffer_[0];

  // cannot be instantiated (protected ctor)
  stack_base() {}

  ~stack_base() {}

  template <typename _T2> friend class allocator_checker;
};

template <typename _T, size_t _Capacity>
class heap_base : public base<_T, _Capacity> {
protected:
  void *memory_;

  // cannot be instantiated (protected ctor)
  heap_base() : memory_(std::malloc(base<_T, _Capacity>::mem_size)) {}

  ~heap_base() { std::free(memory_); }

  template <typename _T2> friend class allocator_checker;
};

template <typename _Base, typename _T, size_t _Capacity>
concept is_allocator_base = requires(_Base &base) {
  { allocator_checker<_Base>::check(base) } -> std::same_as<void *>;
  std::is_same_v<typename _Base::element_type, _T>;
  _Base::capacity == _Capacity;
};

} // namespace allocation