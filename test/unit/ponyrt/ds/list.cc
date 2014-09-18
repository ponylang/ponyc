#include <platform.h>
#include <gtest/gtest.h>

#include <stdlib.h>

PONY_EXTERN_C_BEGIN
#include <ds/list.h>
PONY_EXTERN_C_END

typedef struct elem_t elem_t;

DECLARE_LIST(testlist, elem_t);

class ListTest: public testing::Test
{
  protected:
    testlist_t* list = NULL;
    testlist_t* head = NULL;

    virtual void TearDown()
    {
      testlist_free(head);
      head = list = NULL;
    }

  public:
    void remember_head(testlist_t* list);
    static elem_t* times2(elem_t* e, void* arg);
    static bool compare(elem_t* a, elem_t* b);
};

DEFINE_LIST(testlist, elem_t, ListTest::compare, NULL);

struct elem_t
{
  uint32_t val; /* only needed for map_fn test */
};

bool ListTest::compare(elem_t* a, elem_t* b)
{
  return a == b;
}

void ListTest::remember_head(testlist_t* list)
{
  head = list;
}

elem_t* ListTest::times2(elem_t* e, void* arg)
{
  e->val = e->val << 1;

  return e;
}

/** Calling list_append with NULL as argument
 *  returns a new list with a single element.
 */
TEST_F(ListTest, InitializeList)
{
  elem_t e;

  ASSERT_EQ(NULL, list);
  list = testlist_append(list, &e);


  ASSERT_TRUE(list != NULL);
  ASSERT_EQ(1, testlist_length(list));
}

/** A call to list_length returns the number of elements
 *  until the end of the list is reached.
 */
TEST_F(ListTest, ListLength)
{
  elem_t e1;
  elem_t e2;
  elem_t e3;

  list = testlist_append(list, &e1);
  list = testlist_append(list, &e2);
  list = testlist_append(list, &e3);

  ASSERT_EQ(3, testlist_length(list));
}

/** A call to list_append appends an element to
 *  the end of the list.
 *
 *  After pushing an element, the resulting
 *  pointer still points to the head of the list.
 *
 *  If the end is reached, testlist_next returns NULL.
 */
TEST_F(ListTest, AppendElement)
{
  elem_t e1;
  elem_t e2;

  list = testlist_append(list, &e1);

  remember_head(list);

  list = testlist_append(list, &e2);

  ASSERT_EQ(head, list);

  ASSERT_EQ(&e1, testlist_data(list));

  list = testlist_next(list);

  ASSERT_EQ(&e2, testlist_data(list));

  list = testlist_next(list);

  ASSERT_EQ(NULL, list);
}

/** A call to list_push prepends an element to the list.
 *
 *  The pointer returned points to the new head.
 */
TEST_F(ListTest, PushElement)
{
  elem_t e1;
  elem_t e2;

  list = testlist_append(list, &e1);
  list = testlist_push(list, &e2);

  ASSERT_EQ(&e2, testlist_data(list));

  remember_head(list);

  list = testlist_next(list);

  ASSERT_EQ(&e1, testlist_data(list));
}

/** A call to list_pop provides the head of the list.
 *  The second element prior to list_pop becomes the new head.
 */
TEST_F(ListTest, ListPop)
{
  elem_t* p = NULL;

  elem_t e1;
  elem_t e2;

  list = testlist_append(list, &e1);
  list = testlist_append(list, &e2);

  list = testlist_pop(list, &p);

  ASSERT_EQ(p, &e1);

  ASSERT_EQ(
    &e2,
    testlist_data(
      testlist_index(list, 0)
    )
  );
}

/** A call to list_index (with index > 0) advances
 *  the list pointer to the element at position
 *  "index" (starting from 0).
 */
TEST_F(ListTest, ListIndexAbsolute)
{
  size_t i = 1;

  elem_t e1;
  elem_t e2;

  list = testlist_append(list, &e1);
  remember_head(list);

  list = testlist_append(list, &e2);
  list = testlist_index(list, i);

  ASSERT_EQ(&e2, testlist_data(list));
}

/** A call to list_index with a negative index
 *  allows to traverse the list relative to its end.
 */
TEST_F(ListTest, ListIndexRelative)
{
  ssize_t i = -2;

  elem_t e1;
  elem_t e2;
  elem_t e3;

  list = testlist_append(list, &e1);
  list = testlist_append(list, &e2);
  list = testlist_append(list, &e3);

  remember_head(list);

  //should advance list to the second-to-last element
  list = testlist_index(list, i);

  ASSERT_EQ(&e2, testlist_data(list));
}

/** A call to list_find returns a matching element according
 *  to the provided compare function.
 *
 *  If an element is not in the list, list_find returns NULL.
 */
TEST_F(ListTest, ListFind)
{
  elem_t e1;
  elem_t e2;
  elem_t e3;

  list = testlist_append(list, &e1);
  list = testlist_append(list, &e2);

  ASSERT_EQ(
    &e2,
    testlist_find(list, &e2)
  );

  ASSERT_EQ(
    NULL,
    testlist_find(list, &e3)
  );
}

/** A call to list_findindex returns the position
 *  of an element within the list, -1 otherwise.
 */
TEST_F(ListTest, ListFindIndex)
{
  elem_t e1;
  elem_t e2;
  elem_t e3;

  list = testlist_append(list, &e1);
  list = testlist_append(list, &e2);

  ASSERT_EQ(
    1,
    testlist_findindex(list, &e2)
  );

  ASSERT_EQ(
    -1,
     testlist_findindex(list, &e3)
  );
}

/** Lists where elements are pair-wise equivalent
 *  (starting from the provided head, according to
 *  the compare function), are equivalent.
 */
TEST_F(ListTest, ListEquals)
{
  testlist_t* a = NULL;
  testlist_t* b = NULL;

  elem_t e1;
  elem_t e2;

  a = testlist_append(a, &e1);
  a = testlist_append(a, &e2);

  b = testlist_append(b, &e1);
  b = testlist_append(b, &e2);

  ASSERT_TRUE(
    testlist_equals(a,b)
  );

  testlist_free(a);
  testlist_free(b);
}

/** Lists where one is a pair-wise prefix of
 *  the other, are subsets.
 */
TEST_F(ListTest, ListSubset)
{
  testlist_t* a = NULL;
  testlist_t* b = NULL;

  elem_t e1;
  elem_t e2;

  a = testlist_append(a, &e1);
  a = testlist_append(a, &e2);

  b = testlist_append(a, &e1);

  ASSERT_TRUE(
    testlist_subset(b, a)
  );
}

/** A call to list_reverse the returns a pointer
 *  to a new list of reversed order.
 */
TEST_F(ListTest, ListReverse)
{
  elem_t* e = NULL;

  elem_t e1;
  elem_t e2;
  elem_t e3;

  list = testlist_append(list, &e1);
  list = testlist_append(list, &e2);
  list = testlist_append(list, &e3);

  list = testlist_reverse(list);

  list = testlist_pop(list, &e);

  ASSERT_EQ(e, &e3);

  list = testlist_pop(list, &e);

  ASSERT_EQ(e, &e2);

  list = testlist_pop(list, &e);

  ASSERT_EQ(e, &e1);
}

/** A call to list_map invokes the provided map function
 *  for each element within the list.
 */
TEST_F(ListTest, ListMap)
{
  elem_t* c = NULL;

  elem_t e1;
  elem_t e2;
  elem_t e3;

  e1.val = 1;
  e2.val = 2;
  e3.val = 3;

  list = testlist_append(list, &e1);
  list = testlist_append(list, &e2);
  list = testlist_append(list, &e3);

  remember_head(list);

  list = testlist_map(list, times2, NULL);

  for(uint32_t i = 1; i < 4; i++)
  {
    c = testlist_data(list);

    ASSERT_EQ(i << 1, c->val);

    list = testlist_next(list);
  }
}
