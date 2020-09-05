#include <benchmark/benchmark.h>
#include <platform.h>

#include <stdlib.h>
#include <stdint.h>

#include <ds/fun.h>
#include <ds/hash.h>
#include <mem/pool.h>
#include <iostream>

#define INITIAL_SIZE 8
#define BELOW_HALF (INITIAL_SIZE + (2 - 1)) / 2

typedef struct hash_elem_t hash_elem_t;

DECLARE_HASHMAP(testmap, testmap_t, hash_elem_t)

class HashMapBench: public ::benchmark::Fixture
{
  protected:
    testmap_t _map;

    virtual void SetUp(const ::benchmark::State& st);
    virtual void TearDown(const ::benchmark::State& st);

    hash_elem_t* get_element();
    void put_elements(size_t count);
    void delete_elements(size_t del_count, size_t count);

  public:
    static size_t hash_tst(hash_elem_t* p);
    static bool cmp_tst(hash_elem_t* a, hash_elem_t* b);
    static void free_elem(hash_elem_t* p);
    static void free_buckets(size_t size, void* p);
};

DEFINE_HASHMAP(testmap, testmap_t, hash_elem_t, HashMapBench::hash_tst,
  HashMapBench::cmp_tst, HashMapBench::free_elem)

struct hash_elem_t
{
  size_t key;
  size_t val;
};

void HashMapBench::SetUp(const ::benchmark::State& st)
{
  if (st.thread_index == 0) {
    // range(0) == initial size of hashmap
    testmap_init(&_map, static_cast<size_t>(st.range(0)));
    // range(1) == # of items to insert
    put_elements(static_cast<size_t>(st.range(1)));
    srand(635356);
  }
}

void HashMapBench::TearDown(const ::benchmark::State& st)
{
  if (st.thread_index == 0) {
    testmap_destroy(&_map);
  }
}

void HashMapBench::put_elements(size_t count)
{
  hash_elem_t* curr = NULL;

  for(size_t i = 0; i < count; i++)
  {
    curr = get_element();
    curr->key = i;

    testmap_put(&_map, curr);
  }
}

void HashMapBench::delete_elements(size_t del_pct, size_t count)
{
  hash_elem_t* e1 = get_element();
  size_t del_count = (del_pct * count) / 100;
  size_t final_count = (del_count > count) ? 0 : count - del_count;

  // delete random items until map size is as small as required
  while(testmap_size(&_map) > final_count)
  {
    e1->key = (size_t)rand() % count;

    hash_elem_t* d = testmap_remove(&_map, e1);
    if(d != NULL)
      free_elem(d);
  }
  free_elem(e1);
}

hash_elem_t* HashMapBench::get_element()
{
  return POOL_ALLOC(hash_elem_t);
}

size_t HashMapBench::hash_tst(hash_elem_t* p)
{
  return ponyint_hash_size(p->key);
}

bool HashMapBench::cmp_tst(hash_elem_t* a, hash_elem_t* b)
{
  return a->key == b->key;
}

void HashMapBench::free_elem(hash_elem_t* p)
{
  POOL_FREE(hash_elem_t, p);
}

BENCHMARK_DEFINE_F(HashMapBench, HashDestroy$)(benchmark::State& st) {
  testmap_destroy(&_map);
  size_t sz = static_cast<size_t>(st.range(0));
  while (st.KeepRunning()) {
    st.PauseTiming();
    testmap_init(&_map, sz);
    // exclude time to fill map to exactly 50%
    size_t old_size = _map.contents.size;
    put_elements(old_size/2);
    if(testmap_size(&_map) != (size_t)(old_size/2))
    {
      st.SkipWithError("Map should be exactly half filled!");
      break; // Needed to skip the rest of the iteration.
    }
    st.ResumeTiming();
    // destroy map
    testmap_destroy(&_map);
  }
  testmap_init(&_map, sz);
  st.SetItemsProcessed((int64_t)st.iterations());
}

BENCHMARK_REGISTER_F(HashMapBench, HashDestroy$)->RangeMultiplier(2)->Ranges({{1, 32<<10}, {0, 0}});

BENCHMARK_DEFINE_F(HashMapBench, HashResize$)(benchmark::State& st) {
  size_t old_size = 0;
  size_t count = static_cast<size_t>(st.range(0));
  while (st.KeepRunning()) {
    st.PauseTiming();
    if(_map.contents.size == old_size)
    {
      std::cout << "Shouldn't happen b: count: " << testmap_size(&_map) << ", size: "
        << _map.contents.size << ", old_size: " << old_size << std::endl;
      st.SkipWithError("Map should have resized!");
      break; // Needed to skip the rest of the iteration.
    }
    testmap_destroy(&_map);
    testmap_init(&_map, count);
    // exclude time to fill map to exactly 50%
    old_size = _map.contents.size;
    put_elements(old_size/2);
    if(testmap_size(&_map) != (size_t)(old_size/2))
    {
      st.SkipWithError("Map should be exactly half filled!");
      break; // Needed to skip the rest of the iteration.
    }
    hash_elem_t* curr = get_element();
    curr->key = (old_size/2) + 1;
    st.ResumeTiming();
    testmap_put(&_map, curr);
  }
  st.SetItemsProcessed((int64_t)st.iterations());
}

BENCHMARK_REGISTER_F(HashMapBench, HashResize$)->RangeMultiplier(2)->Ranges({{1, 32<<10}, {0, 0}});

BENCHMARK_DEFINE_F(HashMapBench, HashNext)(benchmark::State& st) {
  while (st.KeepRunning()) {
    size_t ind = HASHMAP_UNKNOWN;
    for(size_t i = 0; i < testmap_size(&_map); i++) {
      hash_elem_t* n = testmap_next(&_map, &ind);
      if(n == NULL)
      {
        st.SkipWithError("Item shouldn't be NULL!");
        break; // Needed to skip the rest of the iteration.
      }
    }
    hash_elem_t* n = testmap_next(&_map, &ind);
    if(n != NULL)
    {
      st.SkipWithError("Item should be NULL!");
      break; // Needed to skip the rest of the iteration.
    }
  }
  st.SetItemsProcessed((int64_t)(st.iterations() * (testmap_size(&_map) + 1)));
}

BENCHMARK_REGISTER_F(HashMapBench, HashNext)->RangeMultiplier(2)->Ranges({{1, 32<<10}, {1, 32}});
BENCHMARK_REGISTER_F(HashMapBench, HashNext)->RangeMultiplier(2)->Ranges({{1, 1}, {1, 32<<10}});

BENCHMARK_DEFINE_F(HashMapBench, HashPut)(benchmark::State& st) {
  hash_elem_t* curr = get_element();
  size_t i = 0;
  size_t mod = static_cast<size_t>(st.range(0));
  while (st.KeepRunning()) {
    curr->key = i & mod;

    // put item
    testmap_put(&_map, curr);
    i++;
  }
  st.SetItemsProcessed((int64_t)st.iterations());
  for(i = 0; i < _map.contents.size; i++)
    testmap_clearindex(&_map, i);
  free_elem(curr);
}

BENCHMARK_REGISTER_F(HashMapBench, HashPut)->RangeMultiplier(2)->Ranges({{32<<10, 32<<10}, {0, 0}});

BENCHMARK_DEFINE_F(HashMapBench, HashPutResize)(benchmark::State& st) {
  hash_elem_t* curr = get_element();
  size_t i = 1;
  while (st.KeepRunning()) {
    curr->key = i;

    // put item
    testmap_put(&_map, curr);
    i++;
  }
  st.SetItemsProcessed((int64_t)st.iterations());
  for(i = 0; i < _map.contents.size; i++)
    testmap_clearindex(&_map, i);
  free_elem(curr);
}

BENCHMARK_REGISTER_F(HashMapBench, HashPutResize)->RangeMultiplier(2)->Ranges({{1, 1}, {0, 0}});

BENCHMARK_DEFINE_F(HashMapBench, HashPutNew)(benchmark::State& st) {
  hash_elem_t* curr = NULL;
  size_t i = 0;
  size_t mod = static_cast<size_t>(st.range(0));
  while (st.KeepRunning()) {
    if(curr == NULL)
      curr = get_element();
    curr->key = i & mod;

    // put item
    curr = testmap_put(&_map, curr);
    i++;
  }
  if(curr != NULL)
    free_elem(curr);
  st.SetItemsProcessed((int64_t)st.iterations());
}

BENCHMARK_REGISTER_F(HashMapBench, HashPutNew)->RangeMultiplier(2)->Ranges({{32<<10, 32<<10}, {0, 0}});

BENCHMARK_DEFINE_F(HashMapBench, HashPutNewResize)(benchmark::State& st) {
  hash_elem_t* curr = NULL;
  size_t i = 1;
  while (st.KeepRunning()) {
    curr = get_element();
    curr->key = i;

    // put item
    testmap_put(&_map, curr);
    i++;
  }
  st.SetItemsProcessed((int64_t)st.iterations());
}

BENCHMARK_REGISTER_F(HashMapBench, HashPutNewResize)->RangeMultiplier(2)->Ranges({{1, 1}, {0, 0}});

BENCHMARK_DEFINE_F(HashMapBench, HashRemove$_)(benchmark::State& st) {
  size_t sz = static_cast<size_t>(st.range(2));
  hash_elem_t* curr = get_element();
  hash_elem_t** a =
    (hash_elem_t**)ponyint_pool_alloc(sz * sizeof(hash_elem_t*));
  for(size_t i = 0; i < sz; i++)
  {
    a[i] = get_element();
    a[i]->key = i;
  }
  while (st.KeepRunning()) {
    st.PauseTiming();
    for(size_t i = 0; i < sz; i++)
      testmap_put(&_map, a[i]);
    st.ResumeTiming();
    for(size_t i = 0; i < sz; i++)
    {
      curr->key = i;
      // remove item
      testmap_remove(&_map, curr);
    }
  }
  st.SetItemsProcessed((int64_t)(st.iterations() * sz));
  for(size_t i = 0; i < sz; i++)
    free_elem(a[i]);
  free_elem(curr);
  ponyint_pool_free(sz, a);
}

BENCHMARK_REGISTER_F(HashMapBench, HashRemove$_)->RangeMultiplier(2)->Ranges({{32<<10, 32<<10}, {0, 0}, {1<<10, 64<<10}});

BENCHMARK_DEFINE_F(HashMapBench, HashSearch)(benchmark::State& st) {
  hash_elem_t* e1 = get_element();
  size_t map_count = testmap_size(&_map);
  size_t map_count_mask = map_count - 1;
  size_t* a = (size_t*)ponyint_pool_alloc(map_count * sizeof(size_t));
  size_t index = HASHMAP_UNKNOWN;
  size_t incr = static_cast<size_t>(st.range(1));
  if(st.range(2) == 0) {
    for(size_t i = 0; i < map_count; i++) {
      hash_elem_t* n = testmap_next(&_map, &index);
      if(n != NULL) {
        a[i] = n->key;
      }
    }
    size_t t = 0;
    size_t r = 0;
    for(size_t i = 0; i < map_count; i++) {
      r = (size_t)rand() & map_count_mask;
      t = a[r];
      a[r] = a[i];
      a[i] = t;
    }
  } else {
    for(size_t i = 0; i < map_count; i++) {
      a[i] = (size_t)rand() + incr;
    }
  }

  index = HASHMAP_UNKNOWN;
  size_t j = 0;
  while (st.KeepRunning()) {
    e1->key = a[j & map_count_mask];
    testmap_get(&_map, e1, &index);
    j++;
  }
  st.SetItemsProcessed((int64_t)st.iterations());
  ponyint_pool_free(map_count, a);
  free_elem(e1);
  e1 = nullptr;
}

BENCHMARK_REGISTER_F(HashMapBench, HashSearch)->RangeMultiplier(2)->Ranges({{1, 1}, {1<<10, 32<<10}, {0, 1}});

BENCHMARK_DEFINE_F(HashMapBench, HashSearchDeletes)(benchmark::State& st) {
  size_t count = static_cast<size_t>(st.range(1));
  // range(2) == % of items to delete at random
  delete_elements(static_cast<size_t>(st.range(2)), count);
  hash_elem_t* e1 = get_element();
  size_t* a = NULL;
  size_t map_count = testmap_size(&_map);
  // we're testing our pathological case
  if(map_count == 0)
  {
    for(size_t i = 0; i < _map.contents.size; i++) {
      e1->key = i;

      // putindex item
      testmap_putindex(&_map, e1, i);

      // remove item
      testmap_removeindex(&_map, i);
    }
    // put item so it's not empty
    hash_elem_t* e2 = get_element();
    e2->key = count - 1;
    testmap_put(&_map, e2);
  }
  map_count = testmap_size(&_map);
  size_t array_size = ponyint_next_pow2(map_count);
  size_t array_size_mask = array_size - 1;
  array_size = array_size < 128 ? 128 : array_size;
  a = (size_t*)ponyint_pool_alloc(array_size * sizeof(size_t));
  size_t ind = HASHMAP_UNKNOWN;
  if(st.range(3) == 0) {
    for(size_t i = 0; i < array_size; i++) {
      if(i < map_count) {
        hash_elem_t* n = testmap_next(&_map, &ind);
        if(n != NULL) {
          a[i] = n->key;
        }
      } else {
        a[i] = a[(size_t)rand() % map_count];
      }
    }
    size_t t = 0;
    size_t r = 0;
    for(size_t i = 0; i < array_size; i++) {
      r = (size_t)rand() & array_size_mask;
      t = a[r];
      a[r] = a[i];
      a[i] = t;
    }
  } else {
    for(size_t i = 0; i < array_size; i++) {
      a[i] = (size_t)rand() + count;
    }
  }

  ind = HASHMAP_UNKNOWN;
  size_t j = 0;
  while (st.KeepRunning()) {
    e1->key = a[j & array_size_mask];
    testmap_get(&_map, e1, &ind);
    j++;
  }
  st.SetItemsProcessed((int64_t)st.iterations());
  ponyint_pool_free(array_size, a);
  free_elem(e1);
  e1 = nullptr;
}

BENCHMARK_REGISTER_F(HashMapBench, HashSearchDeletes)
  ->Args({1, 1<<10, 64, 0})
  ->Args({1, 2<<10, 64, 0})
  ->Args({1, 4<<10, 64, 0})
  ->Args({1, 8<<10, 64, 0})
  ->Args({1, 16<<10, 64, 0})
  ->Args({1, 32<<10, 64, 0})
  ->Args({1, 1<<10, 90, 0})
  ->Args({1, 2<<10, 90, 0})
  ->Args({1, 4<<10, 90, 0})
  ->Args({1, 8<<10, 90, 0})
  ->Args({1, 16<<10, 90, 0})
  ->Args({1, 32<<10, 90, 0})
  ->Args({1, 1<<10, 99, 0})
  ->Args({1, 2<<10, 99, 0})
  ->Args({1, 4<<10, 99, 0})
  ->Args({1, 8<<10, 99, 0})
  ->Args({1, 16<<10, 99, 0})
  ->Args({1, 32<<10, 99, 0})
  ->Args({1, 1<<10, 101, 0})
  ->Args({1, 2<<10, 101, 0})
  ->Args({1, 4<<10, 101, 0})
  ->Args({1, 8<<10, 101, 0})
  ->Args({1, 16<<10, 101, 0})
  ->Args({1, 32<<10, 101, 0})
  ->Args({1, 1<<10, 64, 1})
  ->Args({1, 2<<10, 64, 1})
  ->Args({1, 4<<10, 64, 1})
  ->Args({1, 8<<10, 64, 1})
  ->Args({1, 16<<10, 64, 1})
  ->Args({1, 32<<10, 64, 1})
  ->Args({1, 1<<10, 90, 1})
  ->Args({1, 2<<10, 90, 1})
  ->Args({1, 4<<10, 90, 1})
  ->Args({1, 8<<10, 90, 1})
  ->Args({1, 16<<10, 90, 1})
  ->Args({1, 32<<10, 90, 1})
  ->Args({1, 1<<10, 99, 1})
  ->Args({1, 2<<10, 99, 1})
  ->Args({1, 4<<10, 99, 1})
  ->Args({1, 8<<10, 99, 1})
  ->Args({1, 16<<10, 99, 1})
  ->Args({1, 32<<10, 99, 1})
  ->Args({1, 1<<10, 101, 1})
  ->Args({1, 2<<10, 101, 1})
  ->Args({1, 4<<10, 101, 1})
  ->Args({1, 8<<10, 101, 1})
  ->Args({1, 16<<10, 101, 1})
  ->Args({1, 32<<10, 101, 1});
