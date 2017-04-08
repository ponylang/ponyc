#pragma once

namespace rc {
namespace gen {
namespace detail {

template <typename T>
class Lens;

template <typename Member, typename SourceType>
struct Binding;

} // namespace detail

/// Returns a generator which generates objects of type `T` by constructing them
/// with values from the given generators.
template <typename T, typename... Args>
Gen<T> construct(Gen<Args>... gens);

/// Same as `gen::construct(Gen<Args>...)` but uses `gen::arbitrary` for all
/// types.
template <typename T, typename Arg, typename... Args>
Gen<T> construct();

/// Returns a generator which generates `unique_ptr`s to objects of type `T` by
/// constructing the pointee with values from the given generators.
template <typename T, typename... Args>
Gen<std::unique_ptr<T>> makeUnique(Gen<Args>... gens);

/// Returns a generator which generates `shared_ptr`s to objects of type `T` by
/// constructing the pointee with values from the given generators.
template <typename T, typename... Args>
Gen<std::shared_ptr<T>> makeShared(Gen<Args>... gens);

/// Creates a binding for use with `gen::build` that binds the given member to
/// the given generator. `Member` can be one of the following:
///
/// - A member variable in which case `gen` should be a generator for the type
///   of that variable.
/// - A member function that takes a single argument. `gen` should be a
///   generator for that type.
/// - A member function that takes multiple arguments. `gen` should be a
///   generator of tuples matching those arguments.
template <typename Member, typename T>
detail::Binding<Member, T> set(Member member, Gen<T> gen);

/// Same as `set(Member, Gen)` but uses an appropriate arbitrary generator for
/// the member.
template <typename Member>
detail::Binding<Member, typename detail::Lens<Member>::ValueType>
set(Member member);

/// Creates a generator that builds an object by setting properties of that
/// object from generated values. `gen` will be used to generate the intitial
/// value and then, properties will be set from values generated according to
/// the given bindings. Bindings are created using the `gen::set` family of
/// template functions.
///
/// The advantage to using this function instead of, for example, `gen::exec` to
/// create a generator for a type is that both the initial value and the
/// properties are treated as independent. This gives better shrinking
/// characteristics.
template <typename T, typename... Members, typename... SourceTypes>
Gen<T> build(Gen<T> gen, const detail::Binding<Members, SourceTypes> &... bs);

/// Same as `build(Gen, Bindings...)` but default constructs `T` instead of
/// generating. `T` cannot be deduced and must be specified.
template <typename T, typename... Members, typename... SourceTypes>
Gen<T> build(const detail::Binding<Members, SourceTypes> &... bs);

} // namespace gen
} // namespace rc

#include "Build.hpp"
