#include "define.h"

#ifndef COMMON_H
#define COMMON_H

// Make sure to update the functions in common.cpp and util.cpp when you add a
// type here.
enum TYPE {
  DUMMY = 0,
  INT = 1,
  STRING = 2,
  FLOAT = 3,
  DATE = 4,
  URL_TYPE = 5,
  C_CODE = 6,
  L_CODE = 7,
  LONG = 8,
  IP_TYPE = 9,
  USER_AGENT_TYPE = 10,
  SEARCH_WORD_TYPE = 11,

  DUMMY_INT = 100,
  DUMMY_FLOAT = 101,
  DUMMY_STRING = 102,

  /* PARTIAL_AGG_INT = 5, */
  /* FINAL_AGG_INT = 6, */
  /* PARTIAL_AGG_FLOAT = 7, */
  /* FINAL_AGG_FLOAT = 8 */
};

// Make sure to update the functions in common.cpp when you add an opcode here.
enum OPCODE {
  OP_BD1 = 11,
  OP_BD2 = 10,
  OP_SORT_INTEGERS_TEST = 90,
  OP_SORT_COL1 = 2,
  OP_SORT_COL2 = 50,
  OP_SORT_COL2_IS_DUMMY_COL1 = 53,
  OP_SORT_COL3_IS_DUMMY_COL1 = 52,
  OP_SORT_COL4_IS_DUMMY_COL2 = 51,
  OP_GROUPBY_COL1_SUM_COL2_INT_STEP1 = 102,
  OP_GROUPBY_COL1_SUM_COL2_INT_STEP2 = 103,
  OP_GROUPBY_COL1_SUM_COL2_FLOAT_STEP1 = 107,
  OP_GROUPBY_COL1_SUM_COL2_FLOAT_STEP2 = 108,
  OP_GROUPBY_COL2_SUM_COL3_INT_STEP1 = 1,
  OP_GROUPBY_COL2_SUM_COL3_INT_STEP2 = 101,
  OP_GROUPBY_COL1_AVG_COL2_INT_SUM_COL3_FLOAT_STEP1 = 104,
  OP_GROUPBY_COL1_AVG_COL2_INT_SUM_COL3_FLOAT_STEP2 = 105,
  OP_JOIN_COL1 = 106,
  OP_JOIN_COL2 = 3,
  OP_FILTER_COL2_GT3 = 30,
  OP_FILTER_NOT_DUMMY = 32,
  OP_FILTER_COL1_DATE_BETWEEN_1980_01_01_AND_1980_04_01 = 34,
  OP_PROJECT_PAGERANK_WEIGHT_RANK = 35,
  OP_PROJECT_PAGERANK_APPLY_INCOMING_RANK = 36,
  OP_JOIN_PAGERANK = 37,
};

int get_sort_operation(int op_code);

#ifdef DEBUG
#define debug(...) printf(__VA_ARGS__)
#else
#define debug(...) do {} while (0)
#endif

#ifdef PERF
#define perf(...) printf(__VA_ARGS__)
#else
#define perf(...) do {} while (0)
#endif

#define check(test, ...) do {                   \
    bool result = test;                         \
    if (!result) {                              \
      printf(__VA_ARGS__);                      \
      assert(result);                           \
    }                                           \
  } while (0)

#endif
