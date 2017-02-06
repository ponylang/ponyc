#include <benchmark/benchmark.h>
#include <platform.h>

#include <stdlib.h>
#include <stdint.h>

#include <ds/fun.h>
#include <ds/hash.h>

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
    // range(2) == % of items to delete at random
    delete_elements(st.range(2), st.range(1));
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

  // delete random items until map size is as small as required
  while(testmap_size(&_map) > count - del_count)
  {
    e1->key = rand() % count;

    hash_elem_t* d = testmap_remove(&_map, e1);
    if(d != NULL)
      free_elem(d);
  }
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
      printf("Shouldn't happen b: count: %lu, size: %lu, old_size: %lu\n", testmap_size(&_map), _map.contents.size, old_size);
      st.SkipWithError("Map should have resized!");
      break; // Needed to skip the rest of the iteration.
    }
    testmap_destroy(&_map);
    testmap_init(&_map, st.range(0));
    st.ResumeTiming();
  }
  st.SetItemsProcessed(st.iterations());
}

BENCHMARK_REGISTER_F(HashMapBench, HashResize)->RangeMultiplier(2)->Ranges({{1, 32<<10}, {0, 0}, {0, 0}, {0, 0}});

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

BENCHMARK_REGISTER_F(HashMapBench, HashNext)->RangeMultiplier(2)->Ranges({{1, 32<<10}, {1, 32}, {0, 0}, {0, 0}});
BENCHMARK_REGISTER_F(HashMapBench, HashNext)->RangeMultiplier(2)->Ranges({{1, 1}, {1, 32<<10}, {0, 0}, {0, 0}});

BENCHMARK_DEFINE_F(HashMapBench, HashPut)(benchmark::State& st) {
  hash_elem_t* curr = NULL;
  while (st.KeepRunning()) {
    st.PauseTiming();
    // exclude deleting previously inserted items time
    size_t ind = HASHMAP_UNKNOWN;
    size_t num_elems = testmap_size(&_map);
    for(size_t i = 0; i < num_elems; i++) {
      hash_elem_t* n = testmap_next(&_map, &ind);
      testmap_removeindex(&_map, ind);
      if(n == NULL)
      {
        st.SkipWithError("Item shouldn't be NULL!");
        break; // Needed to skip the rest of the iteration.
      }
      free_elem(n);
    }
    hash_elem_t* n = testmap_next(&_map, &ind);
    if(n != NULL)
    {
      st.SkipWithError("Item should be NULL!");
      break; // Needed to skip the rest of the iteration.
    }
    for(int i = 0; i < st.range(3); i++)
    {
      // exclude allocating new item time
      curr = get_element();
      curr->key = i;

      st.ResumeTiming();
      testmap_put(&_map, curr);
      st.PauseTiming();
    }
    st.ResumeTiming();
  }
  st.SetItemsProcessed(st.iterations() * st.range(3));
}

BENCHMARK_REGISTER_F(HashMapBench, HashPut)->RangeMultiplier(2)->Ranges({{32<<10, 32<<10}, {0, 0}, {0, 0}, {1<<10, 16<<10}});

BENCHMARK_DEFINE_F(HashMapBench, HashPutIndex)(benchmark::State& st) {
  hash_elem_t* curr = NULL;
  while (st.KeepRunning()) {
    st.PauseTiming();
    // exclude deleting previously inserted items time
    size_t ind = HASHMAP_UNKNOWN;
    size_t num_elems = testmap_size(&_map);
    for(size_t i = 0; i < num_elems; i++) {
      hash_elem_t* n = testmap_next(&_map, &ind);
      testmap_removeindex(&_map, ind);
      if(n == NULL)
      {
        st.SkipWithError("Item shouldn't be NULL!");
        break; // Needed to skip the rest of the iteration.
      }
      free_elem(n);
    }
    hash_elem_t* n = testmap_next(&_map, &ind);
    if(n != NULL)
    {
      st.SkipWithError("Item should be NULL!");
      break; // Needed to skip the rest of the iteration.
    }
    for(int i = 0; i < st.range(3); i++)
    {
      // exclude allocating new item time
      curr = get_element();
      curr->key = i;

      st.ResumeTiming();
      testmap_putindex(&_map, curr, i);
      st.PauseTiming();
    }
    st.ResumeTiming();
  }
  st.SetItemsProcessed(st.iterations() * st.range(3));
}

BENCHMARK_REGISTER_F(HashMapBench, HashPutIndex)->RangeMultiplier(2)->Ranges({{32<<10, 32<<10}, {0, 0}, {0, 0}, {1<<10, 16<<10}});

BENCHMARK_DEFINE_F(HashMapBench, HashRemove)(benchmark::State& st) {
  hash_elem_t* curr = get_element();
  while (st.KeepRunning()) {
    st.PauseTiming();
    // exclude inserting items to delete time
    put_elements(st.range(3));
    for(int i = 0; i < st.range(3); i++)
    {
      curr->key = i;

      st.ResumeTiming();
      hash_elem_t* n2 = testmap_remove(&_map, curr);
      st.PauseTiming();
      if(n2 != NULL) {
        // exclude memory free time
        free_elem(n2);
      } else {
        printf("%d not found\n", i);
        st.SkipWithError("Item shouldn't be NULL!");
        break; // Needed to skip the rest of the iteration.
      }
    }
    st.ResumeTiming();
  }
  st.SetItemsProcessed(st.iterations() * st.range(3));
  free_elem(curr);
}

BENCHMARK_REGISTER_F(HashMapBench, HashRemove)->RangeMultiplier(2)->Ranges({{1, 1}, {0, 0}, {0, 0}, {1<<10, 32<<10}});

BENCHMARK_DEFINE_F(HashMapBench, HashRemoveIndex)(benchmark::State& st) {
  hash_elem_t* curr = get_element();
  size_t ind = HASHMAP_UNKNOWN;
  while (st.KeepRunning()) {
    st.PauseTiming();
    // exclude inserting items to delete time
    put_elements(st.range(3));
    for(int i = 0; i < st.range(3); i++)
    {
      curr->key = i;

      hash_elem_t* n2 = testmap_get(&_map, curr, &ind);
      st.ResumeTiming();

      testmap_removeindex(&_map, ind);

      st.PauseTiming();
      if(n2 != NULL) {
        // exclude memory free time
        free_elem(n2);
      } else {
        printf("%d not found\n", i);
        st.SkipWithError("Item shouldn't be NULL!");
        break; // Needed to skip the rest of the iteration.
      }
    }
    st.ResumeTiming();
  }
  st.SetItemsProcessed(st.iterations() * st.range(3));
  free_elem(curr);
}

BENCHMARK_REGISTER_F(HashMapBench, HashRemoveIndex)->RangeMultiplier(2)->Ranges({{1, 1}, {0, 0}, {0, 0}, {1<<10, 32<<10}});

BENCHMARK_DEFINE_F(HashMapBench, HashSearch)(benchmark::State& st) {
  hash_elem_t* e1 = get_element();
  while (st.KeepRunning()) {
    st.PauseTiming();
    for(int i = 0; i < st.range(3); i++) {
      // exclude random # time
      e1->key = rand() % st.range(1);
      size_t index = HASHMAP_UNKNOWN;
      st.ResumeTiming();
      hash_elem_t* n2 = testmap_get(&_map, e1, &index);
      st.PauseTiming();
      if(n2 == NULL)
      {
        st.SkipWithError("Item shouldn't be NULL!");
        break; // Needed to skip the rest of the iteration.
      }
    }
    st.ResumeTiming();
  }
  st.SetItemsProcessed(st.iterations() * st.range(3));
  free_elem(e1);
  e1 = nullptr;
}

BENCHMARK_REGISTER_F(HashMapBench, HashSearch)->RangeMultiplier(2)->Ranges({{1, 1}, {1<<10, 32<<10}, {0, 0}, {64, 1024}});

BENCHMARK_DEFINE_F(HashMapBench, HashSearchDeletes)(benchmark::State& st) {
  hash_elem_t* e1 = get_element();
  bool first_time = true;
  size_t *a = NULL;
  while (st.KeepRunning()) {
    st.PauseTiming();
    // exclude array building time
    if(first_time)
    {
      a = new size_t[testmap_size(&_map)];
      size_t ind = HASHMAP_UNKNOWN;
      for(size_t i = 0; i < testmap_size(&_map); i++) {
        hash_elem_t* n = testmap_next(&_map, &ind);
        if(n != NULL) {
          a[i] = n->key;
        } else {
          st.SkipWithError("Item shouldn't be NULL!");
          break; // Needed to skip the rest of the iteration.
        }
      }
      first_time = false;
    }
    st.ResumeTiming();

    for(int i = 0; i < st.range(3); i++) {
      // exclude random # time
      e1->key = a[rand() % testmap_size(&_map)];
      size_t index = HASHMAP_UNKNOWN;
      st.ResumeTiming();
      hash_elem_t* n2 = testmap_get(&_map, e1, &index);
      st.PauseTiming();
      if(n2 == NULL)
      {
        st.SkipWithError("Item shouldn't be NULL!");
        break; // Needed to skip the rest of the iteration.
      }
    }
    st.ResumeTiming();
  }
  st.SetItemsProcessed(st.iterations() * st.range(3));
  delete[] a;
  free_elem(e1);
  e1 = nullptr;
}

BENCHMARK_REGISTER_F(HashMapBench, HashSearchDeletes)->RangeMultiplier(2)->Ranges({{1, 1}, {1<<10, 32<<10}, {64, 90}, {64, 1024}});
