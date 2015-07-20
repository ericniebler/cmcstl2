#ifndef STL2_CONCEPTS_ITERATOR_HPP
#define STL2_CONCEPTS_ITERATOR_HPP

#include <type_traits>

#include <meta/meta.hpp>

#include <stl2/detail/config.hpp>
#include <stl2/detail/fwd.hpp>
#include <stl2/concepts/foundational.hpp>
#include <stl2/utility.hpp>

////////////////////
// Iterator concepts
//
namespace stl2 { inline namespace v1 { inline namespace concepts {

template <class T>
using ReferenceType =
  decltype(*declval<T>());

} // namespace concepts

namespace detail {
template <class R>
using __iter_move_t =
  meta::if_<
    std::is_reference<ReferenceType<R>>,
    std::remove_reference_t<ReferenceType<R>> &&,
    std::decay_t<ReferenceType<R>>>;
}

template <class T>
detail::__iter_move_t<T> iter_move(T t)
  noexcept(noexcept(detail::__iter_move_t<T>(stl2::move(*t))));

namespace concepts {
template <class T>
using RvalueReferenceType = decltype(iter_move(declval<T&>()));
}

namespace detail {

using meta::detail::uncvref_t;

template <class>
struct acceptable_value_type {};

template <class T>
  requires std::is_object<T>::value
struct acceptable_value_type<T> {
  using type = T;
};

template <class T>
concept bool Void =
  std::is_void<T>::value;

template <class T>
struct nonvoid { using type = T; };

Void{T}
struct nonvoid<T> {};

template <class T>
using nvuncvref_t = meta::_t<nonvoid<uncvref_t<T>>>;

template <class T>
concept bool HasValueType =
  requires { typename T::value_type; };

template <class T>
concept bool HasElementType =
  requires { typename T::element_type; };

template <class T>
concept bool HasReferenceType =
  requires { typename ReferenceType<T>; };

template <class T>
concept bool HasDifferenceType =
  requires { typename T::difference_type; };

} // namespace detail

template <class>
struct value_type {};

template <detail::HasValueType T>
struct value_type<T> {
  using type = typename T::value_type;
};

template <detail::HasElementType T>
  requires !detail::HasValueType<T>
struct value_type<T> {
  using type = typename T::element_type;
};

template <detail::HasReferenceType T>
  requires !detail::HasElementType<T> && !detail::HasValueType<T>
struct value_type<T> {
  using type = detail::uncvref_t<ReferenceType<T>>;
};

namespace concepts {

template <class T>
using ValueType =
  meta::_t<detail::acceptable_value_type<
    meta::_t<value_type<std::remove_cv_t<T>>>>>;

template <class I>
concept bool Readable() {
  return Semiregular<I>() &&
    requires(I& i) {
      typename ValueType<I>;
      typename ReferenceType<I>;
      typename RvalueReferenceType<I>;
      { *i } -> const ValueType<I>&;
    };
}

template <class Out, class T>
concept bool MoveWritable() {
  return Semiregular<Out>() &&
    requires(Out o, T t) {
      typename ReferenceType<Out>;
      *o = stl2::move(t);
    };
}

template <class Out, class T>
concept bool Writable() {
  return MoveWritable<Out, T>() &&
    requires(Out o, const T& t) {
      *o = t;
    };
}

template <class In, class Out>
concept bool IndirectlyMovable() {
  return Readable<In>() && Movable<ValueType<In>>() &&
    Constructible<ValueType<In>, RvalueReferenceType<In>>() &&
    Assignable<ValueType<In> &, RvalueReferenceType<In>>() &&
    MoveWritable<Out, RvalueReferenceType<In>>() &&
    MoveWritable<Out, ValueType<In>>();
}

template <class In, class Out>
concept bool IndirectlyCopyable() {
  return IndirectlyMovable<In, Out>() && Copyable<ValueType<In>>() &&
    Constructible<ValueType<In>, ReferenceType<In>>() &&
    Assignable<ValueType<In> &, ReferenceType<In>>() &&
    Writable<Out, ReferenceType<In>>() &&
    Writable<Out, ValueType<In>>();
}

} // namespace concepts

template <class In, class Out>
constexpr bool is_nothrow_indirectly_movable_v = false;

concepts::IndirectlyMovable{In, Out}
constexpr bool is_nothrow_indirectly_movable_v =
  std::is_nothrow_constructible<ValueType<In>, RvalueReferenceType<In>>::value &&
  std::is_nothrow_assignable<ReferenceType<Out>, RvalueReferenceType<In>>::value &&
  std::is_nothrow_assignable<ReferenceType<Out>, ValueType<In>>::value;

template <class In, class Out>
struct is_nothrow_indirectly_movable
  : meta::bool_<is_nothrow_indirectly_movable_v<In, Out>> {};

template <class In, class Out>
using is_nothrow_indirectly_movable_t = meta::_t<is_nothrow_indirectly_movable<In, Out>>;

// iter_swap2
template <concepts::Readable R1, concepts::Readable R2>
  requires Swappable<ReferenceType<R1>, ReferenceType<R2>>()
void iter_swap2(R1 r1, R2 r2)
  noexcept(is_nothrow_swappable_v<ReferenceType<R1>, ReferenceType<R2>>);

template <concepts::Readable R1, concepts::Readable R2>
  requires concepts::IndirectlyMovable<R1, R2>() && concepts::IndirectlyMovable<R2, R1>() &&
    !concepts::Swappable<ReferenceType<R1>, ReferenceType<R2>>()
void iter_swap2(R1 r1, R2 r2)
  noexcept(is_nothrow_indirectly_movable_v<R1, R2> &&
           is_nothrow_indirectly_movable_v<R2, R1>);

namespace concepts {

template <class I1, class I2 = I1>
concept bool IndirectlySwappable() {
  return Readable<I1>() && Readable<I2>() &&
    requires (I1 i1, I2 i2) {
      iter_swap2(i1, i2);
      iter_swap2(i2, i1);
      iter_swap2(i1, i1);
      iter_swap2(i2, i2);
    };
}

} // namespace concepts

template <class> struct difference_type {};

template <> struct difference_type<void*> {};

template <detail::HasDifferenceType T>
struct difference_type<T> { using type = typename T::difference_type; };

template <class T>
  requires !detail::HasDifferenceType<T> &&
    requires(T& a, T& b) {
      a - b;
      requires !detail::Void<decltype(a - b)>;
    }
struct difference_type<T> :
  std::remove_cv<decltype(declval<T>() - declval<T>())> {};

template <>
struct difference_type<std::nullptr_t> {
  using type = std::ptrdiff_t;
};

namespace concepts {

template <class T>
using DifferenceType =
  detail::nvuncvref_t<meta::_t<difference_type<T>>>;

} // namespace concepts

namespace detail {

template <class T>
concept bool IntegralDifference =
  requires {
    typename DifferenceType<T>;
    requires Integral<DifferenceType<T>>();
  };

template <class T>
concept bool HasDistanceType =
  requires { typename T::distance_type; };

} // namespace detail

template <class> struct distance_type {};

template <detail::HasDistanceType T>
struct distance_type<T> {
  using type = typename T::distance_type;
};

template <detail::IntegralDifference T>
  requires(!detail::HasDistanceType<T>)
struct distance_type<T> :
  std::make_unsigned<DifferenceType<T>> {};

namespace concepts {

template <class T>
using DistanceType =
  meta::_t<distance_type<T>>;

template <class I>
concept bool WeaklyIncrementable() {
  return Semiregular<I>() &&
    requires(I& i) {
      typename DistanceType<I>;
      requires Integral<DistanceType<I>>(); // Try without this?
      ++i;
      requires Same<I&, decltype(++i)>();
      i++;
    };
}

template <class I>
concept bool Incrementable() {
  return WeaklyIncrementable<I>() &&
    EqualityComparable<I>() &&
    requires(I& i) {
      i++;
      requires Same<I, decltype(i++)>();
    };
}

} // namespace concepts

struct weak_input_iterator_tag {};
struct input_iterator_tag :
  weak_input_iterator_tag {};
struct forward_iterator_tag :
  input_iterator_tag {};
struct bidirectional_iterator_tag :
  forward_iterator_tag {};
struct random_access_iterator_tag :
  bidirectional_iterator_tag {};
struct contiguous_iterator_tag :
  random_access_iterator_tag {};

template <class>
struct iterator_category {};
template <class T>
struct iterator_category<T*> {
  using type = contiguous_iterator_tag;
};
template <class T>
  requires requires { typename T::iterator_category; }
struct iterator_category<T> {
  using type = typename T::iterator_category;
};

namespace concepts {

template <class T>
using IteratorCategory =
  meta::_t<iterator_category<T>>;

template <class I>
concept bool WeakIterator() {
  return WeaklyIncrementable<I>() &&
    requires(I& i) {
      //{ *i } -> auto&&;
      *i;
      requires !detail::Void<decltype(*i)>;
    };
}

template <class I>
concept bool Iterator() {
  return WeakIterator<I>() &&
    EqualityComparable<I>();
}

template <class S, class I>
concept bool Sentinel() {
  return Iterator<I>() &&
    Regular<S>() &&
    EqualityComparable<I, S>();
}

template <class I>
concept bool WeakInputIterator() {
  return WeakIterator<I>() &&
  Readable<I>() &&
  requires(I i) {
    typename IteratorCategory<I>;
    Derived<IteratorCategory<I>, weak_input_iterator_tag>();
    //{ i++ } -> Readable;
    requires Readable<decltype(i++)>();
    requires Same<ValueType<I>, ValueType<decltype(i++)>>();
  };
}

template <class I>
concept bool InputIterator() {
  return WeakInputIterator<I>() &&
    Iterator<I>() &&
    Derived<IteratorCategory<I>, input_iterator_tag>();
}

template <class I, class T>
concept bool WeakOutputIterator() {
  return WeakIterator<I>() &&
    Writable<I, T>();
}

template <class I, class T>
concept bool OutputIterator() {
  return WeakOutputIterator<I, T>() &&
    Iterator<I>();
}

template <class I>
concept bool ForwardIterator() {
  return InputIterator<I>() &&
    Incrementable<I>() &&
    Derived<IteratorCategory<I>, forward_iterator_tag>();
}

template <class I>
concept bool BidirectionalIterator() {
  return ForwardIterator<I>() &&
    Derived<IteratorCategory<I>, bidirectional_iterator_tag>() &&
    requires(I i) {
      --i; requires Same<I&, decltype(--i)>();
      i--; requires Same<I, decltype(i--)>();
    };
}

template <class I, class S = I>
concept bool SizedIteratorRange() {
  return Sentinel<S, I>() &&
    requires(I i, S s) {
      typename DifferenceType<I>;
      // Common<DifferenceType<I>, DistanceType<I>> ??
      { i - i } -> DifferenceType<I>;
      { s - s } -> DifferenceType<I>;
      { s - i } -> DifferenceType<I>;
      { i - s } -> DifferenceType<I>;
    };
}

template <class I>
concept bool RandomAccessIterator() {
  return BidirectionalIterator<I>() &&
    Derived<IteratorCategory<I>, random_access_iterator_tag>() &&
    TotallyOrdered<I>() &&
    SizedIteratorRange<I>() &&
    requires(I i, I j, DifferenceType<I> n) {
      i += n; requires Same<I&, decltype(i += n)>();
      i + n; requires Same<I, decltype(i + n)>();
      n + i; requires Same<I, decltype(n + i)>();
      i -= n; requires Same<I&, decltype(i -= n)>();
      i - n; requires Same<I, decltype(i - n)>();
      i[n]; requires Same<ReferenceType<I>,decltype(i[n])>();
    };
}

template <class I>
concept bool ContiguousIterator() {
  return RandomAccessIterator<I>() &&
    Derived<IteratorCategory<I>, contiguous_iterator_tag>() &&
    std::is_reference<ReferenceType<I>>::value;
}

namespace models {

template <class>
constexpr bool readable() { return false; }

Readable{T}
constexpr bool readable() { return true; }


template <class, class>
constexpr bool move_writable() { return false; }

MoveWritable{O, T}
constexpr bool move_writable() { return true; }

template <class, class>
constexpr bool writable() { return false; }

Writable{O, T}
constexpr bool writable() { return true; }


template <class>
constexpr bool weakly_incrementable() { return false; }

WeaklyIncrementable{T}
constexpr bool weakly_incrementable() { return true; }

template <class>
constexpr bool incrementable() { return false; }

Incrementable{T}
constexpr bool incrementable() { return true; }

template <class>
constexpr bool weak_iterator() { return false; }

WeakIterator{T}
constexpr bool weak_iterator() { return true; }

template <class>
constexpr bool iterator() { return false; }

Iterator{T}
constexpr bool iterator() { return true; }

#if 0 // FIXME: explodes memory
template <class, class>
constexpr bool sentinel() { return false; }

Sentinel{S, I}
constexpr bool sentinel() { return true; }
#endif

}} // namespace concepts::models


template <class R>
detail::__iter_move_t<R> iter_move(R r)
  noexcept(noexcept(detail::__iter_move_t<R>(stl2::move(*r)))) {
  return stl2::move(*r);
}

// iter_swap2
template <concepts::Readable R1, concepts::Readable R2>
  requires concepts::Swappable<ReferenceType<R1>, ReferenceType<R2>>()
void iter_swap2(R1 r1, R2 r2)
  noexcept(is_nothrow_swappable_v<ReferenceType<R1>, ReferenceType<R2>>) {
  swap(*r1, *r2);
}

template <concepts::Readable R1, concepts::Readable R2>
  requires concepts::IndirectlyMovable<R1, R2>() && concepts::IndirectlyMovable<R2, R1>() &&
    !concepts::Swappable<ReferenceType<R1>, ReferenceType<R2>>()
void iter_swap2(R1 r1, R2 r2)
  noexcept(is_nothrow_indirectly_movable_v<R1, R2> &&
           is_nothrow_indirectly_movable_v<R2, R1>) {
  ValueType<R1> tmp = iter_move(r1);
  *r1 = iter_move(r2);
  *r2 = std::move(tmp);
}

}} // namespace stl2::v1

#endif // STL2_CONCEPTS_ITERATOR_HPP
