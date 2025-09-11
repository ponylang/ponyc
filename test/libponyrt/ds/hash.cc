#include <platform.h>
#include <gtest/gtest.h>

#include <stdlib.h>
#include <stdint.h>

#include <ds/fun.h>
#include <ds/hash.h>
#include <mem/pool.h>
#include <sched/scheduler.h>

#include <memory>

#define INITIAL_SIZE 8
#define BELOW_HALF (INITIAL_SIZE + (2 - 1)) / 2

typedef struct hash_elem_t hash_elem_t;

DECLARE_HASHMAP_SERIALISE(testmap, testmap_t, hash_elem_t);

class HashMapTest: public testing::Test
{
  protected:
    testmap_t _map;

    virtual void SetUp();
    virtual void TearDown();

    hash_elem_t* get_element();
    void put_elements(size_t count);

  public:
    static size_t hash_tst(hash_elem_t* p);
    static bool cmp_tst(hash_elem_t* a, hash_elem_t* b);
    static void free_elem(hash_elem_t* p);
};

struct hash_elem_t
{
  size_t key;
  size_t val;
};

static void hash_elem_serialise_trace(pony_ctx_t* ctx, void* object)
{
  (void)ctx;
  (void)object;
}

static void hash_elem_serialise(pony_ctx_t* ctx, void* object, void* buf,
  size_t offset, int mutability)
{
  (void)ctx;
  (void)mutability;

  hash_elem_t* elem = (hash_elem_t*)object;
  hash_elem_t* dst = (hash_elem_t*)((uintptr_t)buf + offset);

  dst->key = elem->key;
  dst->val = elem->val;
}

static void hash_elem_deserialise(pony_ctx_t* ctx, void* object)
{
  (void)ctx;
  (void)object;
}

static pony_type_t hash_elem_pony =
{
  0,
  sizeof(hash_elem_t),
  0,
  0,
  0,
  NULL,
#if defined(USE_RUNTIME_TRACING)
  NULL,
  NULL,
#endif
  NULL,
  hash_elem_serialise_trace,
  hash_elem_serialise,
  hash_elem_deserialise,
  NULL,
  NULL,
  NULL,
  NULL,
  0,
  0,
  NULL,
  NULL,
  NULL
};

DEFINE_HASHMAP_SERIALISE(testmap, testmap_t, hash_elem_t, HashMapTest::hash_tst,
  HashMapTest::cmp_tst, HashMapTest::free_elem, &hash_elem_pony);

void HashMapTest::SetUp()
{
  testmap_init(&_map, 1);
}

void HashMapTest::TearDown()
{
  testmap_destroy(&_map);
}

void HashMapTest::put_elements(size_t count)
{
  hash_elem_t* curr = NULL;

  for(size_t i = 0; i < count; i++)
  {
    curr = get_element();
    curr->key = i;

    testmap_put(&_map, curr);
  }
}

hash_elem_t* HashMapTest::get_element()
{
  return POOL_ALLOC(hash_elem_t);
}

size_t HashMapTest::hash_tst(hash_elem_t* p)
{
  return ponyint_hash_size(p->key);
}

bool HashMapTest::cmp_tst(hash_elem_t* a, hash_elem_t* b)
{
  return a->key == b->key;
}

void HashMapTest::free_elem(hash_elem_t* p)
{
  POOL_FREE(hash_elem_t, p);
}

template <typename T>
struct pool_deleter
{
  void operator()(T* ptr)
  {
    POOL_FREE(T, ptr);
  }
};

/** The default size of a map is 0 or at least 8,
 *  i.e. a full cache line of void* on 64-bit systems.
 */
TEST_F(HashMapTest, InitialSizeCacheLine)
{
  ASSERT_EQ((size_t)INITIAL_SIZE, _map.contents.size);
}

/** The size of a list is the number of distinct elements
 *  that have been added to the list.
 */
TEST_F(HashMapTest, HashMapSize)
{
  put_elements(100);

  ASSERT_EQ((size_t)100, testmap_size(&_map));
}

/** Hash maps are resized by (size << 3)
 *  once a size threshold of 0.5 is exceeded.
 */
TEST_F(HashMapTest, Resize)
{
  put_elements(BELOW_HALF);

  ASSERT_EQ((size_t)BELOW_HALF, testmap_size(&_map));
  // the map was not resized yet.
  ASSERT_EQ((size_t)INITIAL_SIZE, _map.contents.size);

  hash_elem_t* curr = get_element();
  curr->key = BELOW_HALF;

  testmap_put(&_map, curr);

  ASSERT_EQ((size_t)BELOW_HALF+1, testmap_size(&_map));
  ASSERT_EQ((size_t)INITIAL_SIZE << 3, _map.contents.size);
}

/** After having put an element with a
 *  some key, it should be possible
 *  to retrieve that element using the key.
 */
TEST_F(HashMapTest, InsertAndRetrieve)
{
  hash_elem_t* e = get_element();
  e->key = 1;
  e->val = 42;
  size_t index = HASHMAP_UNKNOWN;

  testmap_put(&_map, e);

  hash_elem_t* n = testmap_get(&_map, e, &index);

  ASSERT_EQ(e->val, n->val);
}

/** Getting an element which is not in the HashMap
 *  should result in NULL.
 */
TEST_F(HashMapTest, TryGetNonExistent)
{
  hash_elem_t* e1 = get_element();
  hash_elem_t* e2 = get_element();
  std::unique_ptr<hash_elem_t, pool_deleter<hash_elem_t>> elem_guard{e2};
  size_t index = HASHMAP_UNKNOWN;

  e1->key = 1;
  e2->key = 2;

  testmap_put(&_map, e1);

  hash_elem_t* n = testmap_get(&_map, e2, &index);

  ASSERT_EQ(NULL, n);
}

/** Replacing elements with equivalent keys
 *  returns the previous one.
 */
TEST_F(HashMapTest, ReplacingElementReturnsReplaced)
{
  hash_elem_t* e1 = get_element();
  hash_elem_t* e2 = get_element();
  size_t index = HASHMAP_UNKNOWN;

  e1->key = 1;
  e2->key = 1;

  testmap_put(&_map, e1);

  hash_elem_t* n = testmap_put(&_map, e2);
  std::unique_ptr<hash_elem_t, pool_deleter<hash_elem_t>> elem_guard{n};
  ASSERT_EQ(n, e1);

  hash_elem_t* m = testmap_get(&_map, e2, &index);
  ASSERT_EQ(m, e2);
}

/** Deleting an element in a hash map returns it.
 *  The element cannot be retrieved anymore after that.
 *
 *  All other elements remain within the map.
 */
TEST_F(HashMapTest, DeleteElement)
{
  hash_elem_t* e1 = get_element();
  hash_elem_t* e2 = get_element();

  e1->key = 1;
  e2->key = 2;
  size_t index = HASHMAP_UNKNOWN;

  testmap_put(&_map, e1);
  testmap_put(&_map, e2);

  size_t l = testmap_size(&_map);

  ASSERT_EQ(l, (size_t)2);

  hash_elem_t* n1 = testmap_remove(&_map, e1);
  std::unique_ptr<hash_elem_t, pool_deleter<hash_elem_t>> elem_guard{n1};

  l = testmap_size(&_map);

  ASSERT_EQ(n1, e1);
  ASSERT_EQ(l, (size_t)1);

  hash_elem_t* n2 = testmap_get(&_map, e2, &index);

  ASSERT_EQ(n2, e2);
}

/** Iterating over a hash map returns every element
 *  in it.
 */
TEST_F(HashMapTest, MapIterator)
{
  hash_elem_t* curr = NULL;
  size_t expect = 0;

  for(uint32_t i = 0; i < 100; i++)
  {
    expect += i;
    curr = get_element();

    curr->key = i;
    curr->val = i;

    testmap_put(&_map, curr);
  }

  size_t s = HASHMAP_BEGIN;
  size_t l = testmap_size(&_map);
  size_t c = 0; //executions
  size_t e = 0; //sum

  ASSERT_EQ(l, (size_t)100);

  while((curr = testmap_next(&_map, &s)) != NULL)
  {
    c++;
    e += curr->val;
  }

  ASSERT_EQ(e, expect);
  ASSERT_EQ(c, l);
}

/** An element removed by index cannot be retrieved
 *  after being removed.
 */
TEST_F(HashMapTest, RemoveByIndex)
{
  hash_elem_t* curr = NULL;
  put_elements(100);

  size_t i = HASHMAP_BEGIN;
  size_t index = HASHMAP_UNKNOWN;
  hash_elem_t* p = NULL;

  while((curr = testmap_next(&_map, &i)) != NULL)
  {
    if(curr->key == 20)
    {
      p = curr;
      break;
    }
  }

  testmap_removeindex(&_map, i);
  std::unique_ptr<hash_elem_t, pool_deleter<hash_elem_t>> elem_guard{p};

  ASSERT_EQ(NULL, testmap_get(&_map, p, &index));
}

/** An element not found can be put by index into
 * an empty map and retrieved successfully.
 */
TEST_F(HashMapTest, EmptyPutByIndex)
{
  hash_elem_t* e = get_element();
  e->key = 1000;
  e->val = 42;
  std::unique_ptr<hash_elem_t, pool_deleter<hash_elem_t>> elem_guard{e};
  size_t index = HASHMAP_UNKNOWN;

  hash_elem_t* n = testmap_get(&_map, e, &index);

  ASSERT_EQ(NULL, n);
  ASSERT_EQ(HASHMAP_UNKNOWN, index);

  elem_guard.release();
  testmap_putindex(&_map, e, index);

  hash_elem_t* m = testmap_get(&_map, e, &index);

  ASSERT_EQ(e->val, m->val);
}

/** An element not found can be put by index into
 * a non-empty map and retrieved successfully.
 */
TEST_F(HashMapTest, NotEmptyPutByIndex)
{
  put_elements(100);

  hash_elem_t* e = get_element();
  e->key = 1000;
  e->val = 42;
  std::unique_ptr<hash_elem_t, pool_deleter<hash_elem_t>> elem_guard{e};
  size_t index = HASHMAP_UNKNOWN;

  hash_elem_t* n = testmap_get(&_map, e, &index);

  ASSERT_EQ(NULL, n);

  elem_guard.release();
  testmap_putindex(&_map, e, index);

  hash_elem_t* m = testmap_get(&_map, e, &index);

  ASSERT_EQ(e->val, m->val);
}

struct testmap_deleter
{
  void operator()(testmap_t* ptr)
  {
    testmap_destroy(ptr);
    POOL_FREE(testmap_t, ptr);
  }
};

struct pool_size_deleter
{
  size_t size;

  void operator()(void* ptr)
  {
    ponyint_pool_free_size(size, ptr);
  }
};

std::unique_ptr<char, pool_size_deleter> manage_array(ponyint_array_t& array)
{
  std::unique_ptr<char, pool_size_deleter> p{array.ptr};
  p.get_deleter().size = array.alloc;
  return p;
}

TEST_F(HashMapTest, Serialisation)
{
  for(uint32_t i = 0; i < 100; i++)
  {
    hash_elem_t* curr = get_element();

    curr->key = i;
    curr->val = i;

    testmap_put(&_map, curr);
  }

  auto alloc_fn = [](pony_ctx_t* ctx, size_t size)
    {
      (void)ctx;
      return ponyint_pool_alloc_size(size);
    };
  auto throw_fn = [](){throw std::exception{}; };

  pony_ctx_t ctx;
  memset(&ctx, 0, sizeof(pony_ctx_t));
  ponyint_array_t array;
  memset(&array, 0, sizeof(ponyint_array_t));

  pony_serialise(&ctx, &_map, testmap_pony_type(), &array, alloc_fn, throw_fn);
  auto array_guard = manage_array(array);
  std::unique_ptr<testmap_t, testmap_deleter> out_guard{
    (testmap_t*)pony_deserialise(&ctx, testmap_pony_type(), &array, alloc_fn,
      alloc_fn, throw_fn)};

  testmap_t* out = out_guard.get();

  ASSERT_NE(out, (void*)NULL);

  size_t i = HASHMAP_BEGIN;
  size_t j = i;

  hash_elem_t* orig_elem;
  hash_elem_t* out_elem;

  while((orig_elem = testmap_next(&_map, &i)) != NULL)
  {
    out_elem = testmap_get(out, orig_elem, &j);
    ASSERT_NE(out_elem, (void*)NULL);
    ASSERT_EQ(i, j);
    ASSERT_EQ(orig_elem->val, out_elem->val);
  }

  i = j = HASHMAP_BEGIN;

  while((out_elem = testmap_next(out, &j)) != NULL)
  {
    orig_elem = testmap_get(&_map, out_elem, &i);
    ASSERT_NE(orig_elem, (void*)NULL);
    ASSERT_EQ(j, i);
    ASSERT_EQ(out_elem->val, orig_elem->val);
  }
}
