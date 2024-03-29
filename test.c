#include "common.h"
#include "keyvaluestore.h"
#include "lists.h"
#include "minunit.h"
#include "sets.h"
#include "strings.h"
#include <stdio.h>
#include <string.h>

int tests_run = 0;

static int arr_contains_str(char **arr, int arr_length, char *str) {
  for (int i = 0; i < arr_length; ++i) {
    if (strcmp(arr[i], str) == 0)
      return 1;
  }
  return 0;
}

static char *test_strings_set() {
  struct KeyValueStore *kv = new_kv_store();
  mu_assert("error, nonempty kv store", kv->size == 0);
  strings_set(kv, "a", "hello world");
  strings_set(kv, "ab", "yoyo");
  strings_set(kv, "ab", "yoyo1");
  mu_assert("size != 2 after adding 2 keys", kv->size == 2);
  delete_kv_store(kv);
  return 0;
}

static char *test_strings_get() {
  struct KeyValueStore *kv = new_kv_store();
  strings_set(kv, "hello", "world");
  char *v = strings_get(kv, "hello")->string;
  mu_assert("kv_get should return value that was inserted",
            strcmp("world", v) == 0);

  mu_assert("kv_get should return NIL when key does not exist",
            strings_get(kv, "blah")->type == NIL_RETURN);
  delete_kv_store(kv);
  return 0;
}

static char *test_kv_exists() {
  struct KeyValueStore *kv = new_kv_store();
  strings_set(kv, "hello", "world");
  mu_assert("kv_store_key_exists should return 1 when key exists",
            kv_store_key_exists(kv, "hello")->integer);
  mu_assert("kv_store_key_exists should return 0 when key does not exist",
            !kv_store_key_exists(kv, "fdsafdsa")->integer);
  delete_kv_store(kv);
  return 0;
}

static char *test_kv_delete() {
  struct KeyValueStore *kv = new_kv_store();
  strings_set(kv, "a", "hello world");
  strings_set(kv, "ab", "yoyo");
  kv_store_delete_entry(kv, "a");
  mu_assert("strings_delete should delete element from store",
            !kv_store_key_exists(kv, "a")->integer);
  mu_assert("Deleting same key twice should be fine",
            kv_store_delete_entry(kv, "a")->integer == 0);
  delete_kv_store(kv);
  return 0;
}

static char *test_strings_increment() {
  struct KeyValueStore *kv = new_kv_store();
  strings_set(kv, "counter", "123");
  strings_increment(kv, "counter");
  mu_assert("strings_increment should increment integers",
            strcmp(strings_get(kv, "counter")->string, "124") == 0);

  strings_increment(kv, "a");
  mu_assert("strings_increment should create new keys if key does not exist",
            strcmp(strings_get(kv, "a")->string, "1") == 0);

  strings_set(kv, "string", "hello world");
  mu_assert("kv_store increment should return error if value is not an integer",
            strings_increment(kv, "string")->type == ERR_RETURN);
  delete_kv_store(kv);
  return 0;
}

static char *test_lists_lpush_lpop() {
  struct KeyValueStore *kv = new_kv_store();
  mu_assert("non-existing lists should have lengt 0",
            lists_length(kv, "fdsfodj")->integer == 0);
  mu_assert("lists_lpush should increase list length",
            lists_lpush(kv, "mylist", "hello world")->integer == 1);
  mu_assert("list_store_length should return list length",
            lists_length(kv, "mylist")->integer == 1);

  lists_lpush(kv, "mylist", "a");
  lists_lpush(kv, "mylist", "b");

  mu_assert("lists_lpop should return head of list",
            strcmp(lists_lpop(kv, "mylist")->string, "b") == 0);

  mu_assert("lists_lpop should reduce list length",
            lists_length(kv, "mylist")->integer == 2);
  delete_kv_store(kv);
  return 0;
}

static char *test_lists_rpush_rpop() {
  struct KeyValueStore *kv = new_kv_store();
  mu_assert("lists_rpush should increase list length",
            lists_rpush(kv, "mylist", "hello world")->integer == 1);

  lists_rpush(kv, "mylist", "a");
  lists_rpush(kv, "mylist", "b");
  struct ReturnValue *mylist = lists_range(kv, "mylist", 0, 99);
  mu_assert("list should have length 3", mylist->array_length == 3);
  mu_assert("first list element should be the first rpushed",
            strcmp(mylist->array[0], "hello world") == 0);
  mu_assert("second list element should be the second rpushed",
            strcmp(mylist->array[1], "a") == 0);
  mu_assert("this list element should be the third rpushed",
            strcmp(mylist->array[2], "b") == 0);

  mu_assert("lists_rpop should return tail of list",
            strcmp(lists_rpop(kv, "mylist")->string, "b") == 0);

  mu_assert("lists_rpop should reduce list length",
            lists_length(kv, "mylist")->integer == 2);
  delete_kv_store(kv);
  return 0;
}

static char *test_lists_move() {
  struct KeyValueStore *kv = new_kv_store();
  // list1: a
  // list2: b, c
  // move list1 list2 left left -> list2: a, b, c. list1: empty
  lists_rpush(kv, "list1", "a");
  lists_rpush(kv, "list2", "b");
  lists_rpush(kv, "list2", "c");
  mu_assert("lists_move should return the element being popped/pushed",
            strcmp(lists_move(kv, "list1", "list2", LEFT, LEFT)->string, "a") ==
                0);
  mu_assert("lists_move should remove the element from the source list",
            lists_length(kv, "list1")->integer == 0);
  struct ReturnValue *dest_list = lists_range(kv, "list2", 0, 10);
  mu_assert("lists_move should add the element to the destination list at the "
            "specified end",
            dest_list->array_length == 3);
  mu_assert("lists_move should add the element to the destination list at the "
            "specified end",
            strcmp(dest_list->array[0], "a") == 0 &&
                strcmp(dest_list->array[1], "b") == 0 &&
                strcmp(dest_list->array[2], "c") == 0);

  delete_kv_store(kv);
  return 0;
}

static char *test_incorrect_type_handling() {
  struct KeyValueStore *kv = new_kv_store();
  strings_set(kv, "string1", "hwllo");
  mu_assert("calling function on wrong datastructure should return error",
            lists_rpush(kv, "string1", "test")->type == ERR_RETURN);
  mu_assert("calling sets_add on wrong datastructure returns error",
            sets_add(kv, "string1", "fdsfds")->type == ERR_RETURN);
  delete_kv_store(kv);
  return 0;
}

static char *test_lists_trim() {
  struct KeyValueStore *kv = new_kv_store();
  lists_rpush(kv, "list1", "a");
  lists_rpush(kv, "list1", "b");
  lists_rpush(kv, "list1", "c");
  // list1: a,b,c
  lists_trim(kv, "list1", 0, 99);
  mu_assert("lists_trim should not modify list when range includes whole list",
            lists_length(kv, "list1")->integer == 3);
  lists_trim(kv, "list1", 1, 1);
  mu_assert("lists_trim should trim list to specified range (inclusive)",
            lists_length(kv, "list1")->integer == 1);
  // list1: b
  lists_rpush(kv, "list1", "a");
  lists_rpush(kv, "list1", "b");
  // list1: b,a,b
  struct ReturnValue *beforetrimmed = lists_range(kv, "list1", 0, 99);
  mu_assert("lists_range should return the list items in correct order",
            strcmp(beforetrimmed->array[0], "b") == 0 &&
                strcmp(beforetrimmed->array[1], "a") == 0 &&
                strcmp(beforetrimmed->array[2], "b") == 0);
  lists_trim(kv, "list1", -99, -2);
  mu_assert("lists_trim should support negative indeces",
            lists_length(kv, "list1")->integer == 2);
  struct ReturnValue *trimmed = lists_range(kv, "list1", 0, 99);
  mu_assert("lists_range returns the correct number of items",
            trimmed->array_length == 2);
  mu_assert("lists_range returns the correct items",
            strcmp(trimmed->array[0], "b") == 0 &&
                strcmp(trimmed->array[1], "a") == 0);
  delete_kv_store(kv);
  return 0;
}

static char *test_sets_basic_commands() {
  struct KeyValueStore *kv = new_kv_store();
  mu_assert("empty keys should have cardinality 0",
            sets_cardinality(kv, "f12")->integer == 0);
  mu_assert("sets_cardinality return type on empty sets should be integer",
            sets_cardinality(kv, "rdsardsa")->type == INT_RETURN);
  struct ReturnValue *r = sets_add(kv, "set", "hello");
  mu_assert(
      "sets_add should return the number of new elements added to the set",
      r->integer == 1);
  mu_assert("set cardinality should increase after adding member",
            sets_cardinality(kv, "set")->integer == 1);
  mu_assert("the same value is not added twice",
            sets_add(kv, "set", "hello")->integer == 0);
  mu_assert("set size stays the same when the same element is added twice",
            sets_cardinality(kv, "set")->integer == 1);
  mu_assert("sets_remove should return 0 when called with non-existent key",
            sets_remove(kv, "fdsafdsafff", "fdsafd")->integer == 0);
  mu_assert("sets_remove should return 0 when called with member which does "
            "not belong to the set",
            sets_remove(kv, "set", "fdsfdsfdsfds")->integer == 0);
  mu_assert(
      "sets_remove should return the number of elements removed from the set",
      sets_remove(kv, "set", "hello")->integer == 1);
  mu_assert("sets_ismember should return 0 if the set does not exist",
            sets_ismember(kv, "wwww", "test")->integer == 0);
  sets_add(kv, "set2", "value1");
  sets_add(kv, "set2", "value2");
  sets_add(kv, "set2", "value3");
  int card_before_remove = sets_cardinality(kv, "set2")->integer;
  mu_assert(
      "sets_ismember should return 0 if the value is not a member of the set",
      sets_ismember(kv, "set2", "blahblah")->integer == 0);
  mu_assert("sets_ismember should return 1 if the value is a member of the set",
            sets_ismember(kv, "set2", "value1")->integer == 1);
  sets_remove(kv, "set2", "value1");
  mu_assert("Removing an element should decrease the cardinality of the set",
            sets_cardinality(kv, "set2")->integer == card_before_remove - 1);
  mu_assert("sets_ismember should_return 0 for values that have been removed "
            "from the set",
            sets_ismember(kv, "set2", "value1")->integer == 0);

  delete_kv_store(kv);
  return 0;
}

static char *test_sets_intersection() {
  struct KeyValueStore *kv = new_kv_store();
  // test with two empty sets -> empty
  mu_assert("intersection of two empty sets returns no elements",
            sets_intersection(kv, "bla1", "bla5")->array_length == 0);
  // sets_intersection(kv, "set1", "set2"). test with one empty set -> empty
  sets_add(kv, "set1", "a");
  sets_add(kv, "set1", "b");
  sets_add(kv, "set1", "c");
  mu_assert("intersection of a non-empty and an empty set is empty",
            sets_intersection(kv, "set1", "blabal")->array_length == 0);
  mu_assert("intersection of a non-empty and an empty set is empty",
            sets_intersection(kv, "blablab", "set1")->array_length == 0);
  // test intersection of a,b,c, and d,e,f -> should be empty
  sets_add(kv, "set2", "d");
  sets_add(kv, "set2", "e");
  sets_add(kv, "set2", "f");
  mu_assert(
      "intersection of two sets without elements in common should be empty",
      sets_intersection(kv, "set1", "set2")->array_length == 0);

  sets_add(kv, "set1", "d");
  struct ReturnValue *inter1 = sets_intersection(kv, "set1", "set2");
  mu_assert("intersection of two sets should return all elements in common",
            inter1->array_length == 1);
  mu_assert("intersection contains correct elements",
            arr_contains_str(inter1->array, inter1->array_length, "d"));

  delete_kv_store(kv);
  return 0;
}

static char *test_sets_difference() {
  struct KeyValueStore *kv = new_kv_store();
  mu_assert("difference between empty sets is empty",
            sets_difference(kv, "fdsafdsa", "foooofa")->array_length == 0);
  sets_add(kv, "set1", "a");
  mu_assert("difference between empty sets and non-empty sets is empty",
            sets_difference(kv, "ffffffsaf", "set1")->array_length == 0);
  struct ReturnValue *diff1 = sets_difference(kv, "set1", "fdsafpd");
  mu_assert("difference between non-empty set and empty set is the entire "
            "non-empty set",
            diff1->array_length == 1);
  mu_assert("difference between non-empty set and empty set is the entire "
            "non-empty set",
            arr_contains_str(diff1->array, diff1->array_length, "a"));

  sets_add(kv, "set1", "b");
  sets_add(kv, "set1", "c");
  sets_add(kv, "set1", "f");
  sets_add(kv, "set2", "b");
  sets_add(kv, "set2", "c");
  sets_add(kv, "set2", "d");
  struct ReturnValue *diff2 = sets_difference(kv, "set1", "set2");
  mu_assert("set difference has correct number of elements",
            diff2->array_length == 2);
  mu_assert("set difference contains correct elements",
            arr_contains_str(diff2->array, diff2->array_length, "a") &&
                arr_contains_str(diff2->array, diff2->array_length, "f"));
  delete_kv_store(kv);
  return 0;
}

static char *test_sets_union() {
  struct KeyValueStore *kv = new_kv_store();
  mu_assert("union of empty sets is empty",
            sets_union(kv, "fds8fd", "fds089")->array_length == 0);
  sets_add(kv, "set1", "a");
  sets_add(kv, "set1", "b");
  mu_assert("union of a set with an empty set is the entire first set",
            sets_union(kv, "set1", "fdsfsd")->array_length == 2);
  mu_assert(
      "union of an empty set with a non-empty set is the entire second set "
      "independent of argument order",
      sets_union(kv, "fdsfsd", "set1")->array_length == 2);
  delete_kv_store(kv);
  return 0;
}

static char *all_tests() {
  mu_run_test(test_strings_set);
  mu_run_test(test_strings_get);
  mu_run_test(test_kv_exists);
  mu_run_test(test_kv_delete);
  mu_run_test(test_strings_increment);

  mu_run_test(test_lists_lpush_lpop);
  mu_run_test(test_lists_rpush_rpop);
  mu_run_test(test_lists_move);
  mu_run_test(test_lists_trim);

  mu_run_test(test_sets_basic_commands);
  mu_run_test(test_sets_intersection);
  mu_run_test(test_sets_difference);
  mu_run_test(test_sets_union);

  mu_run_test(test_incorrect_type_handling);
  return 0;
}

int main(int argc, char **argv) {
  char *result = all_tests();
  if (result != 0) {
    printf("%s\n", result);
  } else {
    printf("ALL TESTS PASSED\n");
  }
  printf("Tests run: %d\n", tests_run);

  return result != 0;
}
