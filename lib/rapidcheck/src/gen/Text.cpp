#include "rapidcheck/gen/Text.h"

template rc::Gen<std::string> rc::gen::string<std::string>();
template rc::Gen<std::wstring> rc::gen::string<std::wstring>();
template struct rc::Arbitrary<std::string>;
template struct rc::Arbitrary<std::wstring>;
