#include "rapidcheck/detail/Property.h"

#include <algorithm>

namespace rc {
namespace detail {

AdapterContext::AdapterContext()
    : m_resultType(CaseResult::Type::Success) {}

bool AdapterContext::reportResult(const CaseResult &result) {
  switch (result.type) {
  case CaseResult::Type::Discard:
    // Discard overrides all, no previous result is valid. However, we're only
    // interested in the first discard.
    if (m_resultType != CaseResult::Type::Discard) {
      m_messages.clear();
      m_messages.push_back(result.description);
      m_resultType = CaseResult::Type::Discard;
    }
    break;

  case CaseResult::Type::Failure:
    // Discard overrides failure so ignore this failure if already discarded.
    if (m_resultType != CaseResult::Type::Discard) {
      if (m_resultType == CaseResult::Type::Success) {
        // Previous type was success, clear any success messages because now we
        // are in failure mode
        m_messages.clear();
      }

      m_messages.push_back(result.description);
      m_resultType = CaseResult::Type::Failure;
    }
    break;

  case CaseResult::Type::Success:
    if (m_resultType == CaseResult::Type::Success) {
      // We only keep the last success message and we're only insterested if we
      // haven't failed
      m_messages.clear();
      m_messages.push_back(result.description);
    }
    break;
  }

  return true;
}

std::ostream &AdapterContext::logStream() {
  return m_logStream;
}

void AdapterContext::addTag(std::string str) {
  m_tags.push_back(std::move(str));
}

TaggedResult AdapterContext::result() const {
  TaggedResult result;
  result.result.type = m_resultType;
  for (auto it = begin(m_messages); it != end(m_messages); it++) {
    if (it != begin(m_messages)) {
      result.result.description += "\n\n";
    }
    result.result.description += std::move(*it);
  }

  const auto log = m_logStream.str();
  if (!log.empty()) {
    result.result.description += "\n\nLog:\n";
    result.result.description += log;
  }

  result.tags = std::move(m_tags);
  return result;
}

bool operator==(const CaseDescription &lhs, const CaseDescription &rhs) {
  const bool equalExample = (!lhs.example && !rhs.example) ||
      (lhs.example && rhs.example && (lhs.example() == rhs.example()));
  return (lhs.result == rhs.result) && (lhs.tags == rhs.tags) && equalExample;
}

bool operator!=(const CaseDescription &lhs, const CaseDescription &rhs) {
  return !(lhs == rhs);
}

std::ostream &operator<<(std::ostream &os, const CaseDescription &desc) {
  os << "{result='" << desc.result << "', tags=" << toString(desc.tags);
  if (desc.example) {
    os << ", example=" << toString(desc.example());
  }
  os << "}";
  return os;
}

CaseResult toCaseResult(bool value) {
  return value ? CaseResult(CaseResult::Type::Success, "Returned true")
               : CaseResult(CaseResult::Type::Failure, "Returned false");
}

CaseResult toCaseResult(std::string value) {
  return value.empty()
      ? CaseResult(CaseResult::Type::Success, "Returned empty string")
      : CaseResult(CaseResult::Type::Failure, std::move(value));
}

CaseResult toCaseResult(CaseResult caseResult) {
  return caseResult;
}

namespace {

std::pair<std::string, std::string>
tryDescribeIngredientValue(const gen::detail::Recipe::Ingredient &ingredient) {
  const auto value = ingredient.shrinkable.value();

  std::string description = ingredient.description;
  if (description.empty()) {
    std::ostringstream typeString;
    value.showType(typeString);
    description = typeString.str();
  }

  std::ostringstream valueString;
  value.showValue(valueString);

  return {description, valueString.str()};
}

std::pair<std::string, std::string>
describeIngredient(const gen::detail::Recipe::Ingredient &ingredient) {
  // TODO I don't know if this is the right approach with counterexamples
  // even...
  try {
    return tryDescribeIngredientValue(ingredient);
  } catch (const GenerationFailure &e) {
    return {"Generation failed", e.what()};
  } catch (const std::exception &e) {
    return {"Exception while generating", e.what()};
  } catch (...) {
    return {"Unknown exception", "<\?\?\?>"};
  }
}

} // namespace

Gen<CaseDescription>
mapToCaseDescription(Gen<std::pair<TaggedResult, gen::detail::Recipe>> gen) {
  return gen::map(std::move(gen),
                  [](std::pair<TaggedResult, gen::detail::Recipe> &&p) {
                    CaseDescription description;
                    description.result = std::move(p.first.result);
                    description.tags = std::move(p.first.tags);

                    const auto ingredients = std::move(p.second.ingredients);
                    description.example = [ingredients] {
                      Example example;
                      example.reserve(ingredients.size());
                      std::transform(begin(ingredients),
                                     end(ingredients),
                                     std::back_inserter(example),
                                     &describeIngredient);
                      return example;
                    };

                    return description;
                  });
}

} // namespace detail
} // namespace rc
