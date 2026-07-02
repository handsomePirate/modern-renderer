#pragma once

#include <array>
#include <cstddef>
#include <optional>

namespace ring_buffer {

template <typename _T> class buffer_checker {
public:
  static auto check(_T &obj) -> decltype(obj.buffer_[0]) {
    return obj.buffer_[0];
  }
};

template <typename _T, size_t _Capacity, typename _PtrType> class base {
public:
  using element_type = _T;
  using ptr_type = _PtrType;
  static constexpr const ptr_type capacity = _Capacity;

protected:
  static constexpr const size_t cache_size = 64;
  struct alignas(cache_size) cache_pad {
    std::byte pad[cache_size];
  };
};

template <typename _T, size_t _Capacity, typename _PtrType>
class stack_base : public base<_T, _Capacity, _PtrType> {
protected:
  std::array<_T, _Capacity> buffer_;

  // cannot be instantiated (protected ctor)
  stack_base() {}

  ~stack_base() {}

  template <typename _T2> friend class buffer_checker;
};

template <typename _T, size_t _Capacity, typename _PtrType>
class heap_base : public base<_T, _Capacity, _PtrType> {
protected:
  _T *buffer_;

  // cannot be instantiated (protected ctor)
  heap_base() : buffer_(new _T[_Capacity]()) {}

  ~heap_base() { delete[] buffer_; }

  template <typename _T2> friend class buffer_checker;
};

template <typename _Base, typename _T, size_t _Capacity, typename _PtrType>
concept is_buffer_base = requires(_Base &base) {
  {
    buffer_checker<_Base>::check(base)
  } -> std::same_as<typename _Base::element_type &>;
  std::is_same_v<typename _Base::element_type, _T>;
  _Base::capacity == _Capacity;
  std::is_same_v<typename _Base::ptr_type, _PtrType>;
};

template <typename _T, size_t _Capacity, typename _PtrType, typename _Base>
  requires is_buffer_base<_Base, _T, _Capacity, _PtrType>
class access_helpers : public _Base {
protected:
  static_assert((_Capacity & (_Capacity - 1)) == 0 && _Capacity > 1,
                "Capacity must be a power of 2 greater than 1");

  static constexpr const size_t cap_mask = _Capacity - 1;

  inline _T &buf_at_mod(const auto at) { return _Base::buffer_[at & cap_mask]; }

  inline void set_mod(const auto at, _T &&item)
    requires(std::is_move_assignable_v<_T> and std::is_move_constructible_v<_T>)
  {
    buf_at_mod(at) = std::forward<_T>(item);
  }

  inline void set_mod(const auto at, _T item)
    requires(not std::is_move_assignable_v<_T> or
             not std::is_move_constructible_v<_T>)
  {
    buf_at_mod(at) = item;
  }

  inline std::optional<_T> take_opt_mod(const auto at)
    requires(std::is_move_assignable_v<_T> and std::is_move_constructible_v<_T>)
  {
    return std::move(buf_at_mod(at));
  }

  inline std::optional<_T> take_opt_mod(const auto at)
    requires(not std::is_move_assignable_v<_T> or
             not std::is_move_constructible_v<_T>)
  {
    return buf_at_mod(at);
  }
};

} // namespace ring_buffer