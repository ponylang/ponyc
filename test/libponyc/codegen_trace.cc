#include <gtest/gtest.h>
#include <platform.h>

#include <../libponyrt/actor/actor.h>
#include <../libponyrt/gc/objectmap.h>
#include <../libponyrt/mem/pool.h>

#include "util.h"

#define TEST_COMPILE(src) DO(test_compile(src, "ir"))


class CodegenTraceTest : public PassTest
{};


extern "C"
{

EXPORT_SYMBOL void* raw_cast(void* p)
{
  return p;
}

static void objectmap_copy(objectmap_t* src, objectmap_t* dst)
{
  size_t i = HASHMAP_BEGIN;
  object_t* obj;
  while((obj = ponyint_objectmap_next(src, &i)) != NULL)
  {
    object_t* obj_copy = POOL_ALLOC(object_t);
    memcpy(obj_copy, obj, sizeof(object_t));
    ponyint_objectmap_put(dst, obj_copy);
  }
}

EXPORT_SYMBOL size_t objectmap_size(objectmap_t* map)
{
  return ponyint_objectmap_size(map);
}

EXPORT_SYMBOL objectmap_t* gc_local(pony_actor_t* actor)
{
  return &actor->gc.local;
}

EXPORT_SYMBOL objectmap_t* gc_local_snapshot(pony_actor_t* actor)
{
  objectmap_t* local = &actor->gc.local;
  objectmap_t* copy = POOL_ALLOC(objectmap_t);
  memset(copy, 0, sizeof(objectmap_t));
  objectmap_copy(local, copy);
  return copy;
}

EXPORT_SYMBOL void gc_local_snapshot_destroy(objectmap_t* map)
{
  ponyint_objectmap_destroy(map);
  POOL_FREE(objectmap_t, map);
}

EXPORT_SYMBOL bool objectmap_has_object(objectmap_t* map, void* address)
{
  size_t i = HASHMAP_BEGIN;
  object_t* obj;
  while((obj = ponyint_objectmap_next(map, &i)) != NULL)
  {
    if(obj->address == address)
      return true;
  }

  return false;
}

EXPORT_SYMBOL bool objectmap_has_object_rc(objectmap_t* map, void* address,
  size_t rc)
{
  size_t i = HASHMAP_BEGIN;
  object_t* obj;
  while((obj = ponyint_objectmap_next(map, &i)) != NULL)
  {
    if(obj->address == address)
      return obj->rc == rc;
  }

  return false;
}

}

TEST_F(CodegenTraceTest, TraceObjectStatic)
{
  const char* src =
    "use @raw_cast[B ref](obj: (B tag | A iso!))\n"
    "use @gc_local[Pointer[None]](target: Main)\n"
    "use @gc_local_snapshot[Pointer[None]](target: Main)\n"
    "use @gc_local_snapshot_destroy[None](obj_map: Pointer[None])\n"
    "use @objectmap_has_object[Bool](obj_map: Pointer[None], obj: Any tag)\n"
    "use @objectmap_has_object_rc[Bool](obj_map: Pointer[None], obj: Any tag, rc: USize)\n"
    "use @pony_exitcode[None](code: I32)\n"

    "class A\n"

    "class B\n"
    "  let a: A = A\n"

    "actor Main\n"
    "  var map_before: Pointer[None] = Pointer[None]\n"

    "  new create(env: Env) =>\n"
    "    trace(recover B end)\n"
    "    map_before = @gc_local_snapshot(this)\n"

    "  be trace(b: (B tag | A iso)) =>\n"
    "    let b' = @raw_cast(b)\n"
    "    let map_after = @gc_local(this)\n"
    "    let ok = @objectmap_has_object_rc(map_before, b', USize(1)) and\n"
    "      not @objectmap_has_object(map_before, b'.a) and\n"
    "      @objectmap_has_object_rc(map_after, b', USize(0)) and\n"
    "      not @objectmap_has_object(map_after, b'.a)\n"
    "    @gc_local_snapshot_destroy(map_before)\n"
    "    @pony_exitcode(I32(if ok then 1 else 0 end))";

  TEST_COMPILE(src);

  int exit_code = 0;
  ASSERT_TRUE(run_program(&exit_code));
  ASSERT_EQ(exit_code, 1);
}


TEST_F(CodegenTraceTest, TraceObjectDynamic)
{
  const char* src =
    "use @raw_cast[B ref](obj: (B tag | A iso!))\n"
    "use @gc_local[Pointer[None]](target: Main)\n"
    "use @gc_local_snapshot[Pointer[None]](target: Main)\n"
    "use @gc_local_snapshot_destroy[None](obj_map: Pointer[None])\n"
    "use @objectmap_has_object[Bool](obj_map: Pointer[None], obj: Any tag)\n"
    "use @objectmap_has_object_rc[Bool](obj_map: Pointer[None], obj: Any tag, rc: USize)\n"
    "use @pony_exitcode[None](code: I32)\n"

    "class A\n"

    "class B\n"
    "  let a: A = A\n"

    "actor Main\n"
    "  var map_before: Pointer[None] = Pointer[None]\n"

    "  new create(env: Env) =>\n"
    "    let b: (B iso | A iso) = recover B end\n"
    "    trace(consume b)\n"
    "    map_before = @gc_local_snapshot(this)\n"

    "  be trace(b: (B tag | A iso)) =>\n"
    "    let b' = @raw_cast(b)\n"
    "    let map_after = @gc_local(this)\n"
    "    let ok = @objectmap_has_object_rc(map_before, b', USize(1)) and\n"
    "      not @objectmap_has_object(map_before, b'.a) and\n"
    "      @objectmap_has_object_rc(map_after, b', USize(0)) and\n"
    "      not @objectmap_has_object(map_after, b'.a)\n"
    "    @gc_local_snapshot_destroy(map_before)\n"
    "    @pony_exitcode(I32(if ok then 1 else 0 end))";

  TEST_COMPILE(src);

  int exit_code = 0;
  ASSERT_TRUE(run_program(&exit_code));
  ASSERT_EQ(exit_code, 1);
}


TEST_F(CodegenTraceTest, TraceNumberBoxed)
{
  const char* src =
    "use @gc_local[Pointer[None]](target: Main)\n"
    "use @gc_local_snapshot[Pointer[None]](target: Main)\n"
    "use @gc_local_snapshot_destroy[None](obj_map: Pointer[None])\n"
    "use @objectmap_has_object_rc[Bool](obj_map: Pointer[None], obj: Any tag, rc: USize)\n"
    "use @pony_exitcode[None](code: I32)\n"

    "actor Main\n"
    "  var map_before: Pointer[None] = Pointer[None]\n"

    "  new create(env: Env) =>\n"
    "    trace(U32(42))\n"
    "    map_before = @gc_local_snapshot(this)\n"

    "  be trace(x: Any val) =>\n"
    "    let map_after = @gc_local(this)\n"
    "    let ok = @objectmap_has_object_rc(map_before, x, USize(1)) and\n"
    "      @objectmap_has_object_rc(map_after, x, USize(0))\n"
    "    @gc_local_snapshot_destroy(map_before)\n"
    "    @pony_exitcode(I32(if ok then 1 else 0 end))";

  TEST_COMPILE(src);

  int exit_code = 0;
  ASSERT_TRUE(run_program(&exit_code));
  ASSERT_EQ(exit_code, 1);
}
