#include <gtest/gtest.h>
#include "util.h"


// Parsing tests regarding expressions

#define TEST_COMPILE(src) DO(test_compile(src, "expr"))

#define TEST_ERRORS_1(src, err1) \
  { const char* errs[] = {err1, NULL}; \
    DO(test_expected_errors(src, "expr", errs)); }


class LambdaTest : public PassTest
{};


// Lambda tests

TEST_F(LambdaTest, Lambda)
{
  const char* src =
    "class Foo\n"
    "  fun f() =>\n"
    "    {() => None}";

  TEST_COMPILE(src);
}


TEST_F(LambdaTest, LambdaCap)
{
  const char* src =
    "class Foo\n"
    "  fun f() =>\n"
    "    {iso() => None}";

  TEST_COMPILE(src);
}


TEST_F(LambdaTest, LambdaCaptureVariable)
{
  const char* src =
    "class Foo\n"
    "  fun f() =>\n"
    "    let x = \"hi\"\n"
    "    {()(x): String => x}";

  TEST_COMPILE(src);
}


TEST_F(LambdaTest, LambdaCaptureExpression)
{
  const char* src =
    "class Foo\n"
    "  fun f() =>\n"
    "    let y = \"hi\"\n"
    "    {()(x = y): String => x}";

  TEST_COMPILE(src);
}


TEST_F(LambdaTest, LambdaCaptureExpressionAndType)
{
  const char* src =
    "class Foo\n"
    "  fun f() =>\n"
    "    let y = \"hi\"\n"
    "    {()(x: String = y): String => x}";

  TEST_COMPILE(src);
}


TEST_F(LambdaTest, LambdaCaptureTypeWithoutExpressionFail)
{
  const char* src =
    "class Foo\n"
    "  fun f() =>\n"
    "    let x = \"hi\"\n"
    "    {()(x: String): String => x}";

  TEST_ERRORS_1(src, "value missing for lambda expression capture");
}


TEST_F(LambdaTest, LambdaCaptureFree)
{
  const char* src =
    "class Foo\n"
    "  fun f() =>\n"
    "    let x = \"hi\"\n"
    "    {(): String => x}";

  TEST_COMPILE(src);
}


TEST_F(LambdaTest, LambdaCaptureRefInIso)
{
  const char* src =
    "class Foo\n"
    "  fun f() =>\n"
    "    let x: String ref = String\n"
    "    {(): String ref => x} iso";

  TEST_ERRORS_1(src, "this parameter must be sendable");
}


TEST_F(LambdaTest, LambdaCaptureRefInVal)
{
  const char* src =
    "class Foo\n"
    "  fun f() =>\n"
    "    let x: String ref = String\n"
    "    {(): String ref => x} val";

  TEST_ERRORS_1(src, "this parameter must be sendable");
}


TEST_F(LambdaTest, LambdaCaptureDontcareVariable)
{
  const char* src =
    "class Foo\n"
    "  fun f() =>\n"
    "    {()(_) => None}";

  TEST_COMPILE(src);
}


TEST_F(LambdaTest, LambdaCaptureDontcareExpression)
{
  const char* src =
    "class Foo\n"
    "  fun f() =>\n"
    "    {()(_ = Foo) => None}";

  TEST_COMPILE(src);
}


TEST_F(LambdaTest, LambdaCaptureDontcareExpressionAndType)
{
  const char* src =
    "class Foo\n"
    "  fun f() =>\n"
    "    {()(_: Foo = Foo) => None}";

  TEST_COMPILE(src);
}


TEST_F(LambdaTest, LambdaCaptureDontcareFree)
{
  const char* src =
    "class Foo\n"
    "  fun f() =>\n"
    "    {() => _}";

  TEST_COMPILE(src);
}


TEST_F(LambdaTest, ObjectCaptureFree)
{
  const char* src =
    "class Foo\n"
    "  fun f() =>\n"
    "    let x = \"hi\"\n"
    "    object\n"
    "      fun apply(): String => x\n"
    "    end";

  TEST_COMPILE(src);
}


TEST_F(LambdaTest, ObjectCaptureRefInActor)
{
  const char* src =
    "class Foo\n"
    "  fun f() =>\n"
    "    let x: String ref = String\n"
    "    object\n"
    "      be apply() => x\n"
    "    end";

  TEST_ERRORS_1(src, "this parameter must be sendable");
}

TEST_F(LambdaTest, ObjectSyntaxErrorInMethod)
{
  const char* src =
    "class Foo\n"
    "  fun f() =>\n"
    "    object\n"
    "      fun wrong_syntax() =>\n"
    "        1 + 2 * 4\n"
    "    end";

  TEST_ERRORS_1(src, "Operator precedence is not supported. Parentheses required.");
}


TEST_F(LambdaTest, InferFromArgType)
{
  const char* src =
    "primitive X\n"
    "primitive Y\n"
    "primitive Z\n"
    "type Fn is {iso(X, Y): Z} iso\n"

    "primitive Test\n"
    "  fun test(fn: Fn) => None\n"
    "  fun apply() =>\n"
    "    test({iso(x: X, y: Y): Z => Z } iso)\n"
    "    test({iso(x: X, y: Y): Z => Z })\n"
    "    test({(x: X, y: Y): Z => Z })\n"
    "    test({(x: X, y: Y) => Z })\n"
    "    test({(x: X, y) => Z })\n"
    "    test({(x, y) => Z })\n"
    "    test({(_, _) => Z })";

  TEST_COMPILE(src);
}


TEST_F(LambdaTest, InferFromNamedArgType)
{
  const char* src =
    "primitive X\n"
    "primitive Y\n"
    "primitive Z\n"
    "type Fn is {iso(X, Y): Z} iso\n"

    "primitive Test\n"
    "  fun test(fn: Fn) => None\n"
    "  fun apply() =>\n"
    "    test(where fn = {iso(x: X, y: Y): Z => Z } iso)\n"
    "    test(where fn = {iso(x: X, y: Y): Z => Z })\n"
    "    test(where fn = {(x: X, y: Y): Z => Z })\n"
    "    test(where fn = {(x: X, y: Y) => Z })\n"
    "    test(where fn = {(x: X, y) => Z })\n"
    "    test(where fn = {(x, y) => Z })\n"
    "    test(where fn = {(_, _) => Z })";

  TEST_COMPILE(src);
}


TEST_F(LambdaTest, InferFromParamType)
{
  const char* src =
    "primitive X\n"
    "primitive Y\n"
    "primitive Z\n"
    "type Fn is {iso(X, Y): Z} iso\n"

    "class Test\n"
    "  fun apply(\n"
    "    f1: Fn = {iso(x: X, y: Y): Z => Z } iso,\n"
    "    f2: Fn = {iso(x: X, y: Y): Z => Z },\n"
    "    f3: Fn = {(x: X, y: Y): Z => Z },\n"
    "    f4: Fn = {(x: X, y: Y) => Z },\n"
    "    f5: Fn = {(x: X, y) => Z },\n"
    "    f6: Fn = {(x, y) => Z },\n"
    "    f7: Fn = {(_, _) => Z })\n"
    "  => None\n";

  TEST_COMPILE(src);
}


TEST_F(LambdaTest, InferFromAssignType)
{
  const char* src =
    "primitive X\n"
    "primitive Y\n"
    "primitive Z\n"
    "type Fn is {iso(X, Y): Z} iso\n"

    "class Test\n"
    "  fun apply() =>\n"
    "    let f1: Fn = {iso(x: X, y: Y): Z => Z } iso\n"
    "    let f2: Fn = {iso(x: X, y: Y): Z => Z }\n"
    "    let f3: Fn = {(x: X, y: Y): Z => Z }\n"
    "    let f4: Fn = {(x: X, y: Y) => Z }\n"
    "    let f5: Fn = {(x: X, y) => Z }\n"
    "    let f6: Fn = {(x, y) => Z }\n"
    "    let f7: Fn = {(_, _) => Z }";

  TEST_COMPILE(src);
}


TEST_F(LambdaTest, InferFromLambdaCaptureType)
{
  const char* src =
    "primitive X\n"
    "primitive Y\n"
    "primitive Z\n"
    "type Fn is {iso(X, Y): Z} iso\n"

    "class Test\n"
    "  fun apply() =>\n"
    "    {()(fn: Fn = {iso(x: X, y: Y): Z => Z } iso) => None }\n"
    "    {()(fn: Fn = {iso(x: X, y: Y): Z => Z }) => None }\n"
    "    {()(fn: Fn = {(x: X, y: Y): Z => Z }) => None }\n"
    "    {()(fn: Fn = {(x: X, y: Y) => Z }) => None }\n"
    "    {()(fn: Fn = {(x: X, y) => Z }) => None }\n"
    "    {()(fn: Fn = {(x, y) => Z }) => None }\n"
    "    {()(fn: Fn = {(_, _) => Z }) => None }";

  TEST_COMPILE(src);
}


TEST_F(LambdaTest, InferFromReturnType)
{
  const char* src =
    "primitive X\n"
    "primitive Y\n"
    "primitive Z\n"
    "type Fn is {iso(X, Y): Z} iso\n"

    "class Test\n"
    "  fun f1(): Fn => {iso(x: X, y: Y): Z => Z } iso\n"
    "  fun f2(): Fn => {iso(x: X, y: Y): Z => Z }\n"
    "  fun f3(): Fn => {(x: X, y: Y): Z => Z }\n"
    "  fun f4(): Fn => {(x: X, y: Y) => Z }\n"
    "  fun f5(): Fn => {(x: X, y) => Z }\n"
    "  fun f6(): Fn => {(x, y) => Z }\n"
    "  fun f7(): Fn => {(_, _) => Z }";

  TEST_COMPILE(src);
}


TEST_F(LambdaTest, InferFromEarlyReturnType)
{
  const char* src =
    "primitive X\n"
    "primitive Y\n"
    "primitive Z\n"
    "type Fn is {iso(X, Y): Z} iso\n"

    "class Test\n"
    "  fun apply(u: U8): Fn =>\n"
    "    if u == 1 then return {iso(x: X, y: Y): Z => Z } iso end\n"
    "    if u == 2 then return {iso(x: X, y: Y): Z => Z } end\n"
    "    if u == 3 then return {(x: X, y: Y): Z => Z } end\n"
    "    if u == 4 then return {(x: X, y: Y) => Z } end\n"
    "    if u == 5 then return {(x: X, y) => Z } end\n"
    "    if u == 6 then return {(x, y) => Z } end\n"
    "    {(_, _) => Z }";

  TEST_COMPILE(src);
}


TEST_F(LambdaTest, InferFromCallToCallableObject)
{
  const char* src =
    "primitive X\n"
    "primitive Y\n"
    "primitive Z\n"
    "type Fn is {iso(X, Y): Z} iso\n"

    "class Test\n"
    "  fun apply(callable: {(Fn)}) =>\n"
    "    callable({iso(x: X, y: Y): Z => Z } iso)\n"
    "    callable({iso(x: X, y: Y): Z => Z })\n"
    "    callable({(x: X, y: Y): Z => Z })\n"
    "    callable({(x: X, y: Y) => Z })\n"
    "    callable({(x: X, y) => Z })\n"
    "    callable({(x, y) => Z })\n"
    "    callable({(_, _) => Z })";

  TEST_COMPILE(src);
}


TEST_F(LambdaTest, InferFromArrayElementType)
{
  const char* src =
    "primitive X\n"
    "primitive Y\n"
    "primitive Z\n"
    "type Fn is {iso(X, Y): Z} iso\n"

    "class Test\n"
    "  fun apply(): Array[Fn] =>\n"
    "    [as Fn:\n"
    "      {iso(x: X, y: Y): Z => Z } iso\n"
    "      {iso(x: X, y: Y): Z => Z }\n"
    "      {(x: X, y: Y): Z => Z }\n"
    "      {(x: X, y: Y) => Z }\n"
    "      {(x: X, y) => Z }\n"
    "      {(x, y) => Z }\n"
    "      {(_, _) => Z }\n"
    "    ]";

  TEST_COMPILE(src);
}


TEST_F(LambdaTest, InferFromInferredArrayElementType)
{
  const char* src =
    "primitive X\n"
    "primitive Y\n"
    "primitive Z\n"
    "type Fn is {iso(X, Y): Z} iso\n"

    "class Test\n"
    "  fun apply(): Array[Fn] =>\n"
    "    [\n"
    "      {iso(x: X, y: Y): Z => Z } iso\n"
    "      {iso(x: X, y: Y): Z => Z }\n"
    "      {(x: X, y: Y): Z => Z }\n"
    "      {(x: X, y: Y) => Z }\n"
    "      {(x: X, y) => Z }\n"
    "      {(x, y) => Z }\n"
    "      {(_, _) => Z }\n"
    "    ]";

  TEST_COMPILE(src);
}


TEST_F(LambdaTest, InferThroughDeeplyNestedExpressions)
{
  const char* src =
    "primitive X\n"
    "primitive Y\n"
    "primitive Z\n"
    "type Fn is {iso(X, Y): Z} iso\n"

    "class Test[A: Any val]\n"
    "  fun apply(): Fn =>\n"
    "    if true then\n"
    "      ifdef linux then\n"
    "        {(_, _) => Z }\n"
    "      elseif osx then\n"
    "        {(_, _) => Z }\n"
    "      else\n"
    "        iftype A <: X then\n"
    "          {(_, _) => Z }\n"
    "        elseif A <: Y then\n"
    "          {(_, _) => Z }\n"
    "        elseif A <: Z then\n"
    "          {(_, _) => Z }\n"
    "        else\n"
    "          {(_, _) => Z }\n"
    "        end\n"
    "      end\n"
    "    else\n"
    "      while false do\n"
    "        if false then break {(_, _) => Z } end\n"
    "        match true\n"
    "        | true => {(_, _) => Z }\n"
    "        | false =>\n"
    "          repeat\n"
    "            if false then break {(_, _) => Z } end\n"
    "            {(_, _) => Z }\n"
    "          until true else {(_, _) => Z } end\n"
    "        end\n"
    "      else\n"
    "        try error else {(_, _) => Z } end\n"
    "      end\n"
    "    end";

  TEST_COMPILE(src);
}


TEST_F(LambdaTest, InferThroughComplexTypes)
{
  const char* src =
    "primitive X\n"
    "primitive Y\n"
    "primitive Z\n"
    "type Fn is {iso(X, Y): Z} iso\n"

    "class Test\n"
    "  fun apply() =>\n"
    "    let f1: (Fn, U8) = ({(_, _) => Z }, 0)\n"
    "    let f2: (X | (Fn, U8)) = ({(_, _) => Z }, 0)\n"
    "    let f3: this->(Fn, U8) = ({(_, _) => Z }, 0)";

  TEST_COMPILE(src);
}


TEST_F(LambdaTest, InferThroughComplexTypesWithTypeArgs)
{
  const char* src =
    "primitive X\n"
    "primitive Y\n"
    "primitive Z\n"
    "type Fn is FnGeneric[X, Y, Z]\n"
    "interface iso FnGeneric[A, B, C]\n"
    "  fun iso apply(a: A, b: B): C\n"

    "class Test\n"
    "  fun apply() =>\n"
    "    let f1: (Fn, U8) = ({(_, _) => Z }, 0)\n"
    "    let f2: (X | (Fn, U8)) = ({(_, _) => Z }, 0)\n"
    "    let f3: this->(Fn, U8) = ({(_, _) => Z }, 0)";

  TEST_COMPILE(src);
}


TEST_F(LambdaTest, InferFromUnambiguousLambdaChoices)
{
  const char* src =
    "primitive X\n"
    "primitive Y\n"
    "primitive Z\n"

    "class Test\n"
    "  fun apply() =>\n"
    "    let f1: ({iso(X): Z} iso | {iso(X, Y): Z} iso) = {(_, _) => Z }\n"
    "    let f2: ({iso(X): Z} iso | {iso(X, Y): Z} iso) = {(_) => Z }\n"
    "    let f3: ({(X, Y): Z} ref | {(X, Y): Z} val) = {(_, _) => Z } ref\n"
    "    let f4: ({box(X): Z} ref | {ref(X): Z} ref) = {ref(_) => Z } ref";

  TEST_COMPILE(src);
}


TEST_F(LambdaTest, CantInferFromAmbiguousLambdaChoices)
{
  const char* src =
    "primitive X\n"
    "primitive Y\n"
    "primitive Z\n"

    "class Test\n"
    "  fun apply() =>\n"
    "    let f1: ({iso(Y, X): Z} iso | {iso(X, Y): Z} iso) = {(_, _) => Z }";

  TEST_ERRORS_1(src,
    "a lambda parameter must specify a type or be inferable from context");
}


TEST_F(LambdaTest, InferWithTypeParamRefInLambdaTypeSignature)
{
  const char* src =
    "primitive Test[A: Any #share]\n"
    "  fun pass(a: A): None => None\n"
    "  fun apply(): {(A): None} =>\n"
    "    let that = this\n"
    "    {(a) => that.pass(a) }";

  TEST_COMPILE(src);
}


TEST_F(LambdaTest, LambdaCaptureUndefinedFieldExplicit)
{
  const char* src =
    "class Foo\n"
    "  let _x: USize\n"
    "  let _f: {(): USize}\n"
    "  new create() =>\n"
    "    _f = {()(_x) => _x }\n"
    "    _x = 42";

  TEST_ERRORS_1(src, "can't use an undefined variable in an expression");
}


TEST_F(LambdaTest, LambdaCaptureUndefinedFieldImplicit)
{
  const char* src =
    "class Foo\n"
    "  let _x: USize\n"
    "  let _f: {(): USize}\n"
    "  new create() =>\n"
    "    _f = {() => _x }\n"
    "    _x = 42";

  TEST_ERRORS_1(src, "can't use an undefined variable in an expression");
}


TEST_F(LambdaTest, LambdaCaptureUndefinedFieldImplicitNested)
{
  const char* src =
    "class Foo\n"
    "  let _x: USize\n"
    "  let _f: {(): USize}\n"
    "  new create() =>\n"
    "    _f = {() => _x + 1 }\n"
    "    _x = 42";

  TEST_ERRORS_1(src, "can't use an undefined variable in an expression");
}


TEST_F(LambdaTest, LambdaCaptureDefinedFieldOk)
{
  const char* src =
    "class Foo\n"
    "  let _x: USize\n"
    "  let _f: {(): USize}\n"
    "  new create() =>\n"
    "    _x = 42\n"
    "    _f = {() => _x }";

  TEST_COMPILE(src);
}
