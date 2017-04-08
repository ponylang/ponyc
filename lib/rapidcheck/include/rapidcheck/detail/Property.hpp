#pragma once

#include "rapidcheck/detail/FunctionTraits.h"
#include "rapidcheck/gen/detail/ExecRaw.h"
#include "rapidcheck/detail/PropertyContext.h"

namespace rc {
namespace detail {

struct TaggedResult {
  CaseResult result;
  Tags tags;
};

class AdapterContext : public PropertyContext {
public:
  AdapterContext();

  bool reportResult(const CaseResult &result) override;
  std::ostream &logStream() override;
  void addTag(std::string str) override;
  TaggedResult result() const;

private:
  CaseResult::Type m_resultType;
  std::vector<std::string> m_messages;
  std::ostringstream m_logStream;
  Tags m_tags;
};

CaseResult toCaseResult(bool value);
CaseResult toCaseResult(std::string value);
CaseResult toCaseResult(CaseResult caseResult);

/// Helper class to convert different return types to `CaseResult`.
template <typename ReturnType>
struct CaseResultHelper {
  template <typename Callable, typename... Args>
  static CaseResult resultOf(const Callable &callable, Args &&... args) {
    return toCaseResult(callable(std::forward<Args>(args)...));
  }
};

template <>
struct CaseResultHelper<void> {
  template <typename Callable, typename... Args>
  static CaseResult resultOf(const Callable &callable, Args &&... args) {
    callable(std::forward<Args>(args)...);
    return CaseResult(CaseResult::Type::Success, "no exceptions thrown");
  }
};

template <typename Callable, typename Type = FunctionType<Callable>>
class PropertyAdapter;

template <typename Callable, typename ReturnType, typename... Args>
class PropertyAdapter<Callable, ReturnType(Args...)> {
public:
  template <typename Arg,
            typename = typename std::enable_if<
                !std::is_same<Decay<Arg>, PropertyAdapter>::value>::type>
  PropertyAdapter(Arg &&callable)
      : m_callable(std::forward<Arg>(callable)) {}

  TaggedResult operator()(Args &&... args) const {
    AdapterContext context;
    ImplicitParam<param::CurrentPropertyContext> letContext(&context);

    try {
      context.reportResult(CaseResultHelper<ReturnType>::resultOf(
          m_callable, static_cast<Args &&>(args)...));
    } catch (const CaseResult &result) {
      context.reportResult(result);
    } catch (const GenerationFailure &e) {
      context.reportResult(CaseResult(
          CaseResult::Type::Discard,
          std::string("Generation failed with message:\n") + e.what()));
    } catch (const std::exception &e) {
      context.reportResult(CaseResult(
          CaseResult::Type::Failure,
          std::string("Exception thrown with message:\n") + e.what()));
    } catch (const std::string &str) {
      context.reportResult(CaseResult(CaseResult::Type::Failure, str));
    } catch (...) {
      context.reportResult(
          CaseResult(CaseResult::Type::Failure, "Unknown object thrown"));
    }

    return context.result();
  }

private:
  Callable m_callable;
};

Gen<CaseDescription>
mapToCaseDescription(Gen<std::pair<TaggedResult, gen::detail::Recipe>> gen);

template <typename Callable>
Property toProperty(Callable &&callable) {
  using Adapter = PropertyAdapter<Decay<Callable>>;
  return mapToCaseDescription(
      gen::detail::execRaw(Adapter(std::forward<Callable>(callable))));
}

} // namespace detail
} // namespace rc

#include "Property.hpp"
