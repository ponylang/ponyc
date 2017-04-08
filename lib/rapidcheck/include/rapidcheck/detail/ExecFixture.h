#pragma once

namespace rc {
namespace detail {

// Helper class for adapting test fixtures to callables that can work as
// RapidCheck properties. `Fixture` is reinstantiated each time the call
// operator is invoked.
template <typename Fixture, typename FuncType = ArgTypes<Fixture>>
struct ExecFixture;

template <typename Fixture, typename... Args>
struct ExecFixture<Fixture, TypeList<Args...>> {
  static void exec(Args &&... args) {
    Fixture fixture;
    fixture.rapidCheck_fixtureSetUp();
    fixture(std::forward<Args>(args)...);
    fixture.rapidCheck_fixtureTearDown();
  }
};

} // namespace detail
} // namespace rc
