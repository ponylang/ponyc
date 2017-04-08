#pragma once

// MSVC HACK - Undefine stupid macros MS macros
#undef min
#undef max

#include "rapidcheck/Seq.h"
#include "rapidcheck/seq/Create.h"
#include "rapidcheck/seq/Operations.h"
#include "rapidcheck/seq/SeqIterator.h"
#include "rapidcheck/seq/Transform.h"

#include "rapidcheck/Shrinkable.h"
#include "rapidcheck/shrinkable/Create.h"
#include "rapidcheck/shrinkable/Operations.h"
#include "rapidcheck/shrinkable/Transform.h"

#include "rapidcheck/Gen.h"
#include "rapidcheck/gen/Arbitrary.h"
#include "rapidcheck/gen/Build.h"
#include "rapidcheck/gen/Chrono.h"
#include "rapidcheck/gen/Container.h"
#include "rapidcheck/gen/Create.h"
#include "rapidcheck/gen/Exec.h"
#include "rapidcheck/gen/Maybe.h"
#include "rapidcheck/gen/Numeric.h"
#include "rapidcheck/gen/Predicate.h"
#include "rapidcheck/gen/Select.h"
#include "rapidcheck/gen/Text.h"
#include "rapidcheck/gen/Transform.h"
#include "rapidcheck/gen/Tuple.h"

#include "rapidcheck/Assertions.h"
#include "rapidcheck/Check.h"
#include "rapidcheck/Classify.h"
#include "rapidcheck/Log.h"
#include "rapidcheck/Show.h"
