#include <benchmark/benchmark.h>
#include <platform.h>

#include <stdlib.h>
#include <stdint.h>

#include <ds/fun.h>
#include <ds/hash.h>
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
  HashMapBench::cmp_tst, malloc, HashMapBench::free_buckets,
  HashMapBench::free_elem)

struct hash_elem_t
{
  size_t key;
  size_t val;
};

void HashMapBench::SetUp(const ::benchmark::State& st)
{
  if (st.thread_index == 0) {
    // range(0) == initial size of hashmap
    testmap_init(&_map, st.range(0));
    // range(1) == # of items to insert
    put_elements(st.range(1));
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
    e1->key = rand() % count;

    hash_elem_t* d = testmap_remove(&_map, e1);
    if(d != NULL)
      free_elem(d);
  }
  free_elem(e1);
}

hash_elem_t* HashMapBench::get_element()
{
  return (hash_elem_t*) malloc(sizeof(hash_elem_t));
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
  free(p);
}

void HashMapBench::free_buckets(size_t len, void* p)
{
  (void)len;
  free(p);
}

BENCHMARK_DEFINE_F(HashMapBench, HashDestroy)(benchmark::State& st) {
  while (st.KeepRunning()) {
    st.PauseTiming();
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
    st.PauseTiming();
    testmap_init(&_map, st.range(0));
    st.ResumeTiming();
  }
  st.SetItemsProcessed(st.iterations());
}

BENCHMARK_REGISTER_F(HashMapBench, HashDestroy)->RangeMultiplier(2)->Ranges({{1, 32<<10}, {0, 0}});

BENCHMARK_DEFINE_F(HashMapBench, HashResize)(benchmark::State& st) {
  while (st.KeepRunning()) {
    st.PauseTiming();
    // exclude time to fill map to exactly 50%
    size_t old_size = _map.contents.size;
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
    st.PauseTiming();
    if(_map.contents.size == old_size)
    {
      std::cout << "Shouldn't happen b: count: " << testmap_size(&_map) << ", size: "
        << _map.contents.size << ", old_size: " << old_size << std::endl;
      st.SkipWithError("Map should have resized!");
      break; // Needed to skip the rest of the iteration.
    }
    testmap_destroy(&_map);
    testmap_init(&_map, st.range(0));
    st.ResumeTiming();
  }
  st.SetItemsProcessed(st.iterations());
}

BENCHMARK_REGISTER_F(HashMapBench, HashResize)->RangeMultiplier(2)->Ranges({{1, 32<<10}, {0, 0}});

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
  st.SetItemsProcessed(st.iterations() * (testmap_size(&_map) + 1));
}

BENCHMARK_REGISTER_F(HashMapBench, HashNext)->RangeMultiplier(2)->Ranges({{1, 32<<10}, {1, 32}});
BENCHMARK_REGISTER_F(HashMapBench, HashNext)->RangeMultiplier(2)->Ranges({{1, 1}, {1, 32<<10}});

BENCHMARK_DEFINE_F(HashMapBench, HashPut)(benchmark::State& st) {
  hash_elem_t* curr = get_element();
  if(testmap_size(&_map) == (_map.contents.size/2))
  {
    // delete 1 element at random to ensure we don't cause a resize
    curr->key = rand() % st.range(1);
    hash_elem_t* n2 = testmap_remove(&_map, curr);
    free_elem(n2);
  }
  while (st.KeepRunning()) {
    st.PauseTiming();
    // exclude random # time
    curr->key = rand() + st.range(1);
    st.ResumeTiming();

    // put item
    testmap_put(&_map, curr);

    st.PauseTiming();
    // remove item
    testmap_remove(&_map, curr);
    st.ResumeTiming();
  }
  st.SetItemsProcessed(st.iterations());
}

BENCHMARK_REGISTER_F(HashMapBench, HashPut)->RangeMultiplier(2)->Ranges({{32<<10, 32<<10}, {1<<10, 32<<10}});

BENCHMARK_DEFINE_F(HashMapBench, HashPutIndex)(benchmark::State& st) {
  hash_elem_t* curr = get_element();
  if(testmap_size(&_map) == (_map.contents.size/2))
  {
    // delete 1 element at random to ensure we don't cause a resize
    curr->key = rand() % st.range(1);
    hash_elem_t* n2 = testmap_remove(&_map, curr);
    free_elem(n2);
  }
  while (st.KeepRunning()) {
    st.PauseTiming();
    // exclude random # time
    curr->key = rand() + st.range(1);
    size_t i = HASHMAP_UNKNOWN;
    hash_elem_t* n2 = testmap_get(&_map, curr, &i);
    if(n2 != NULL)
      st.SkipWithError("Item shouldn't be in map!");
    st.ResumeTiming();

    // putindex item
    testmap_putindex(&_map, curr, i);

    st.PauseTiming();
    // remove item
    testmap_remove(&_map, curr);
    st.ResumeTiming();
  }
  st.SetItemsProcessed(st.iterations());
}

BENCHMARK_REGISTER_F(HashMapBench, HashPutIndex)->RangeMultiplier(2)->Ranges({{32<<10, 32<<10}, {1<<10, 32<<10}});

BENCHMARK_DEFINE_F(HashMapBench, HashRemove)(benchmark::State& st) {
  hash_elem_t* curr = get_element();
  while (st.KeepRunning()) {
    st.PauseTiming();
    // find an item at random to remove
    curr->key = rand() % st.range(1);
    st.ResumeTiming();

    // remove item
    hash_elem_t* n2 = testmap_remove(&_map, curr);

    st.PauseTiming();
    if(n2 != NULL) {
      // put item back
      testmap_put(&_map, n2);
    } else {
      std::cout << curr->key << " not found" << std::endl;
      st.SkipWithError("Item shouldn't be NULL!");
    }
    st.ResumeTiming();
  }
  st.SetItemsProcessed(st.iterations());
  free_elem(curr);
}

BENCHMARK_REGISTER_F(HashMapBench, HashRemove)->RangeMultiplier(2)->Ranges({{1, 1}, {1<<10, 32<<10}});

BENCHMARK_DEFINE_F(HashMapBench, HashRemoveIndex)(benchmark::State& st) {
  hash_elem_t* curr = get_element();
  size_t ind = HASHMAP_UNKNOWN;
  while (st.KeepRunning()) {
    st.PauseTiming();
    // find an item at random to remove
    curr->key = rand() % st.range(1);
    hash_elem_t* n2 = testmap_get(&_map, curr, &ind);
    st.ResumeTiming();

    // remove index
    testmap_removeindex(&_map, ind);

    st.PauseTiming();
    if(n2 != NULL) {
      // put item back
      testmap_put(&_map, n2);
    } else {
      std::cout << curr->key << " not found" << std::endl;
      st.SkipWithError("Item shouldn't be NULL!");
    }
    st.ResumeTiming();
  }
  st.SetItemsProcessed(st.iterations());
  free_elem(curr);
}

BENCHMARK_REGISTER_F(HashMapBench, HashRemoveIndex)->RangeMultiplier(2)->Ranges({{1, 1}, {1<<10, 32<<10}});

BENCHMARK_DEFINE_F(HashMapBench, HashSearch)(benchmark::State& st) {
  hash_elem_t* e1 = get_element();
  while (st.KeepRunning()) {
    st.PauseTiming();
    // exclude random # time
    if(st.range(2) == 0)
      e1->key = rand() % st.range(1);
    else
      e1->key = rand();
    size_t index = HASHMAP_UNKNOWN;
    st.ResumeTiming();
    hash_elem_t* n2 = testmap_get(&_map, e1, &index);
    st.PauseTiming();
    if(st.range(2) == 0 && n2 == NULL)
    {
      st.SkipWithError("Item shouldn't be NULL!");
    }
    st.ResumeTiming();
  }
  st.SetItemsProcessed(st.iterations());
  free_elem(e1);
  e1 = nullptr;
}

BENCHMARK_REGISTER_F(HashMapBench, HashSearch)->RangeMultiplier(2)->Ranges({{1, 1}, {1<<10, 32<<10}, {0, 1}});

BENCHMARK_DEFINE_F(HashMapBench, HashSearchDeletes)(benchmark::State& st) {
  // range(2) == % of items to delete at random
  delete_elements(st.range(2), st.range(1));
  hash_elem_t* e1 = get_element();
  size_t *a = NULL;
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
    e2->key = rand();
    testmap_put(&_map, e2);
  }
  a = new size_t[testmap_size(&_map)];
  size_t ind = HASHMAP_UNKNOWN;
  for(size_t i = 0; i < testmap_size(&_map); i++) {
    hash_elem_t* n = testmap_next(&_map, &ind);
    if(n != NULL) {
      a[i] = n->key;
    } else {
      st.SkipWithError("Item shouldn't be NULL!");
    }
  }
  while (st.KeepRunning()) {
    st.PauseTiming();
    // exclude random # time
    if(st.range(3) == 0)
      e1->key = a[rand() % testmap_size(&_map)];
    else
      e1->key = rand();
    size_t index = HASHMAP_UNKNOWN;
    st.ResumeTiming();
    hash_elem_t* n2 = testmap_get(&_map, e1, &index);
    st.PauseTiming();
    if(st.range(3) == 0 && n2 == NULL)
    {
      st.SkipWithError("Item shouldn't be NULL!");
    }
    st.ResumeTiming();
  }
  st.SetItemsProcessed(st.iterations());
  delete[] a;
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
