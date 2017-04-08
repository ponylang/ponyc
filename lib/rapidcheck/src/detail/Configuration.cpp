#include "rapidcheck/detail/Configuration.h"

#include <random>
#include <cstdlib>
#include <sstream>
#include <iostream>

#include "MapParser.h"
#include "ParseException.h"
#include "StringSerialization.h"
#include "rapidcheck/detail/Platform.h"

namespace rc {
namespace detail {

std::ostream &operator<<(std::ostream &os, const Configuration &config) {
  os << configToString(config);
  return os;
}

bool operator==(const Configuration &c1, const Configuration &c2) {
  return (c1.testParams == c2.testParams) &&
      (c1.verboseProgress == c2.verboseProgress) &&
      (c1.verboseShrinking == c2.verboseShrinking) &&
      (c1.reproduce == c2.reproduce);
}

bool operator!=(const Configuration &c1, const Configuration &c2) {
  return !(c1 == c2);
}

ConfigurationException::ConfigurationException(std::string msg)
    : m_msg(std::move(msg)) {}

const char *ConfigurationException::what() const noexcept {
  return m_msg.c_str();
}

namespace {

template <typename T>
void fromString(const std::string &str, T &out, bool &ok) {
  std::istringstream in(std::move(str));
  in >> out;
  ok = !in.fail();
}

template <typename T>
void fromString(const std::string &str,
                std::unordered_map<std::string, Reproduce> &out,
                bool &ok) {
  try {
    out = stringToReproduceMap(str);
    ok = true;
  } catch (const ParseException &) {
    ok = false;
  }
}

// Returns false only on invalid format, not on missing key
template <typename T, typename Validator>
bool loadParam(const std::map<std::string, std::string> &map,
               const std::string &key,
               T &dest,
               std::string failMsg,
               const Validator &validate) {
  const auto it = map.find(key);
  if (it == end(map)) {
    return false;
  }

  bool ok = false;
  T value;
  fromString<T>(it->second, value, ok);
  if (!ok || !validate(value)) {
    throw ConfigurationException(std::move(failMsg));
  }
  dest = value;
  return true;
}

template <typename T>
bool isNonNegative(T x) {
  return x >= 0;
}

template <typename T>
bool anything(const T &) {
  return true;
}

Configuration configFromMap(const std::map<std::string, std::string> &map,
                            const Configuration &defaults) {
  Configuration config(defaults);

  loadParam(map,
            "seed",
            config.testParams.seed,
            "'seed' must be a valid integer",
            anything<uint64_t>);

  loadParam(map,
            "max_success",
            config.testParams.maxSuccess,
            "'max_success' must be a valid non-negative integer",
            isNonNegative<int>);

  loadParam(map,
            "max_size",
            config.testParams.maxSize,
            "'max_size' must be a valid non-negative integer",
            isNonNegative<int>);

  loadParam(map,
            "max_discard_ratio",
            config.testParams.maxDiscardRatio,
            "'max_discard_ratio' must be a valid non-negative integer",
            isNonNegative<int>);

  loadParam(map,
            "noshrink",
            config.testParams.disableShrinking,
            "'noshrink' must be either '1' or '0'",
            anything<bool>);

  loadParam(map,
            "verbose_progress",
            config.verboseProgress,
            "'verbose_progress' must be either '1' or '0'",
            anything<bool>);

  loadParam(map,
            "verbose_shrinking",
            config.verboseShrinking,
            "'verbose_shrinking' must be either '1' or '0'",
            anything<bool>);

  loadParam(map,
            "reproduce",
            config.reproduce,
            "'reproduce' string has invalid format",
            anything<decltype(config.reproduce)>);

  return config;
}

std::map<std::string, std::string> mapFromConfig(const Configuration &config) {
  return {
      {"seed", std::to_string(config.testParams.seed)},
      {"max_success", std::to_string(config.testParams.maxSuccess)},
      {"max_size", std::to_string(config.testParams.maxSize)},
      {"max_discard_ratio", std::to_string(config.testParams.maxDiscardRatio)},
      {"noshrink", config.testParams.disableShrinking ? "1" : "0"},
      {"verbose_progress", std::to_string(config.verboseProgress)},
      {"verbose_shrinking", std::to_string(config.verboseShrinking)},
      {"reproduce", reproduceMapToString(config.reproduce)}};
}

std::map<std::string, std::string>
mapDifference(const std::map<std::string, std::string> &lhs,
              const std::map<std::string, std::string> &rhs) {
  std::map<std::string, std::string> result;
  for (const auto &pair : lhs) {
    auto it = rhs.find(pair.first);
    if ((it == end(rhs)) || (pair.second != it->second)) {
      result.insert(pair);
    }
  }

  return result;
}

} // namespace

Configuration configFromString(const std::string &str,
                               const Configuration &defaults) {
  try {
    return configFromMap(parseMap(str), defaults);
  } catch (const ParseException &e) {
    throw ConfigurationException(
        std::string("Failed to parse configuration string -- ") + e.what());
  }
}

std::string configToString(const Configuration &config) {
  return mapToString(mapFromConfig(config));
}

std::string configToMinimalString(const Configuration &config) {
  auto defaults = mapFromConfig(Configuration());
  // Remove keys that we always want to specify
  defaults.erase("seed");
  return mapToString(mapDifference(mapFromConfig(config), defaults));
}

namespace {

Configuration loadConfiguration() {
  Configuration config;
  // Default to random seed
  std::random_device device;
  config.testParams.seed = (static_cast<uint64_t>(device()) << 32) | device();

  const auto params = getEnvValue("RC_PARAMS");
  if (params) {
    try {
      config = configFromString(*params, config);
    } catch (const ConfigurationException &e) {
      std::cerr << "Error parsing configuration: " << e.what() << std::endl;
      std::exit(1);
    }
  }

  // TODO rapidcheck logging framework ftw
  std::cerr << "Using configuration: " << configToMinimalString(config)
            << std::endl;
  return config;
}

} // namespace

const Configuration &configuration() {
  static const Configuration config = loadConfiguration();
  return config;
}

} // namespace detail
} // namespace rc
