#include <gtest/gtest.h>
#include <platform.h>

#include "util.h"


#define TEST_COMPILE(src) DO(test_compile(src, "verify"))

#define TEST_ERRORS_1(src, err1) \
  { const char* errs[] = {err1, NULL}; \
    DO(test_expected_errors(src, "verify", errs)); }


class VerifyTest : public PassTest
{};
