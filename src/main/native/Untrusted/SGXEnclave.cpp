#include "SGXEnclave.h"

// This file implements the JNI methods exposed in
// src/main/scala/edu/berkeley/cs/rise/opaque/execution/SGXEnclave.scala.

#include <climits>
#include <cstdarg>
#include <cstdio>
#include <cstdlib>
#include <sys/time.h> // struct timeval
#include <time.h> // gettimeofday
#include <sstream>
#include <ios>
#include <sgx_error.h>       /* sgx_status_t */
#include <sgx_eid.h>     /* sgx_enclave_id_t */
#include <sgx_urts.h>

#include "common.h"
#include "Enclave_u.h"

JavaVM *jvm = nullptr;

/**
 * Throw a Java exception indicating that the specified operation failed with the specified SGX
 * error.
 */
void throw_sgx_error(const char *operation, sgx_status_t ret) {
  std::stringstream s;
  s << operation << "failed. Error code is 0x"
    << std::hex << ret
    << ". See the Intel SGX SDK Developer Reference.";

  const char *full_message = s.str().c_str();
  printf("%s\n", full_message);
  ocall_throw(full_message);
}

/** Check the SGX return value of the specified operation and fail if it indicates an error. */
void sgx_check_quiet(const char *operation, sgx_status_t ret) {
  if (ret != SGX_SUCCESS) {
    throw_sgx_error(operation, ret);
  }
}

/** A timer that stops on deletion. */
class scoped_timer {
public:
  scoped_timer(uint64_t *total_time) {
    this->total_time = total_time;
    struct timeval start;
    gettimeofday(&start, NULL);
    time_start = start.tv_sec * 1000000 + start.tv_usec;
  }

  ~scoped_timer() {
    struct timeval end;
    gettimeofday(&end, NULL);
    time_end = end.tv_sec * 1000000 + end.tv_usec;
    *total_time += time_end - time_start;
  }

  uint64_t * total_time;
  uint64_t time_start, time_end;
};

/**
 * A macro that checks the SGX return value of the specified operation. If the DEBUG or PERF symbols
 * are set, it also prints the elapsed time while executing the operation.
 */
#if defined(PERF) || defined(DEBUG)
#define sgx_check(message, op) do {                     \
    printf("%s running...\n", message);                 \
    uint64_t t_ = 0;                                    \
    sgx_status_t ret_;                                  \
    {                                                   \
      scoped_timer timer_(&t_);                         \
      ret_ = op;                                        \
    }                                                   \
    double t_ms_ = ((double) t_) / 1000;                \
    if (ret_ != SGX_SUCCESS) {                          \
      printf("%s failed (%f ms)\n", message, t_ms_);    \
      throw_sgx_error(ret_);                            \
    } else {                                            \
      printf("%s done (%f ms).\n", message, t_ms_);     \
    }                                                   \
  } while (0)
#else
#define sgx_check(message, op) sgx_check_quiet(message, op)
#endif

JNIEXPORT jlong JNICALL Java_edu_berkeley_cs_rise_opaque_execution_SGXEnclave_StartEnclave(
  JNIEnv *env, jobject obj, jstring library_path) {
  (void)env;
  (void)obj;

  env->GetJavaVM(&jvm);

  sgx_enclave_id_t eid;
  sgx_launch_token_t token = {0};
  int updated = 0;

  const char *library_path_str = env->GetStringUTFChars(library_path, nullptr);
  sgx_check("StartEnclave",
            sgx_create_enclave(
              library_path_str, SGX_DEBUG_FLAG, &token, &updated, &eid, nullptr));
  env->ReleaseStringUTFChars(library_path, library_path_str);

  return eid;
}

JNIEXPORT void JNICALL Java_edu_berkeley_cs_rise_opaque_execution_SGXEnclave_StopEnclave(
  JNIEnv *env, jobject obj, jlong eid) {
  (void)env;
  (void)obj;

  sgx_check("StopEnclave", sgx_destroy_enclave(eid));
}

JNIEXPORT jbyteArray JNICALL Java_edu_berkeley_cs_rise_opaque_execution_SGXEnclave_Project(
  JNIEnv *env, jobject obj, jlong eid, jbyteArray project_list, jbyteArray input_rows) {
  (void)obj;

  jboolean if_copy;

  uint32_t project_list_length = (uint32_t) env->GetArrayLength(project_list);
  uint8_t *project_list_ptr = (uint8_t *) env->GetByteArrayElements(project_list, &if_copy);

  uint32_t input_rows_length = (uint32_t) env->GetArrayLength(input_rows);
  uint8_t *input_rows_ptr = (uint8_t *) env->GetByteArrayElements(input_rows, &if_copy);

  uint8_t *output_rows;
  size_t output_rows_length;

  sgx_check("Project",
            ecall_project(
              eid,
              project_list_ptr, project_list_length,
              input_rows_ptr, input_rows_length,
              &output_rows, &output_rows_length));

  env->ReleaseByteArrayElements(project_list, (jbyte *) project_list_ptr, 0);
  env->ReleaseByteArrayElements(input_rows, (jbyte *) input_rows_ptr, 0);

  jbyteArray ret = env->NewByteArray(output_rows_length);
  env->SetByteArrayRegion(ret, 0, output_rows_length, (jbyte *) output_rows);
  free(output_rows);

  return ret;
}

JNIEXPORT jbyteArray JNICALL Java_edu_berkeley_cs_rise_opaque_execution_SGXEnclave_Filter(
  JNIEnv *env, jobject obj, jlong eid, jbyteArray condition, jbyteArray input_rows) {
  (void)obj;

  jboolean if_copy;

  size_t condition_length = (size_t) env->GetArrayLength(condition);
  uint8_t *condition_ptr = (uint8_t *) env->GetByteArrayElements(condition, &if_copy);

  uint32_t input_rows_length = (uint32_t) env->GetArrayLength(input_rows);
  uint8_t *input_rows_ptr = (uint8_t *) env->GetByteArrayElements(input_rows, &if_copy);

  uint8_t *output_rows;
  size_t output_rows_length;

  sgx_check("Filter",
            ecall_filter(
              eid,
              condition_ptr, condition_length,
              input_rows_ptr, input_rows_length,
              &output_rows, &output_rows_length));

  env->ReleaseByteArrayElements(condition, (jbyte *) condition_ptr, 0);
  env->ReleaseByteArrayElements(input_rows, (jbyte *) input_rows_ptr, 0);

  jbyteArray ret = env->NewByteArray(output_rows_length);
  env->SetByteArrayRegion(ret, 0, output_rows_length, (jbyte *) output_rows);
  free(output_rows);

  return ret;
}

JNIEXPORT jbyteArray JNICALL Java_edu_berkeley_cs_rise_opaque_execution_SGXEnclave_Encrypt(
  JNIEnv *env, jobject obj, jlong eid, jbyteArray plaintext) {
  (void)obj;

  uint32_t plength = (uint32_t) env->GetArrayLength(plaintext);
  jboolean if_copy = false;
  jbyte *ptr = env->GetByteArrayElements(plaintext, &if_copy);

  uint8_t *plaintext_ptr = (uint8_t *) ptr;

  const jsize clength = plength + SGX_AESGCM_IV_SIZE + SGX_AESGCM_MAC_SIZE;
  jbyteArray ciphertext = env->NewByteArray(clength);

  uint8_t *ciphertext_copy = new uint8_t[clength];

  sgx_check_quiet(
    "Encrypt", ecall_encrypt(eid, plaintext_ptr, plength, ciphertext_copy, (uint32_t) clength));

  env->SetByteArrayRegion(ciphertext, 0, clength, (jbyte *) ciphertext_copy);

  env->ReleaseByteArrayElements(plaintext, ptr, 0);

  delete[] ciphertext_copy;

  return ciphertext;
}

JNIEXPORT jbyteArray JNICALL Java_edu_berkeley_cs_rise_opaque_execution_SGXEnclave_Sample(
  JNIEnv *env, jobject obj, jlong eid, jbyteArray input_rows) {
  (void)obj;

  jboolean if_copy;
  size_t input_rows_length = static_cast<size_t>(env->GetArrayLength(input_rows));
  uint8_t *input_rows_ptr = reinterpret_cast<uint8_t *>(
    env->GetByteArrayElements(input_rows, &if_copy));

  uint8_t *output_rows;
  size_t output_rows_length;

  sgx_check("Sample",
            ecall_sample(
              eid,
              input_rows_ptr, input_rows_length,
              &output_rows, &output_rows_length));

  jbyteArray ret = env->NewByteArray(output_rows_length);
  env->SetByteArrayRegion(ret, 0, output_rows_length, (jbyte *) output_rows);
  free(output_rows);

  env->ReleaseByteArrayElements(input_rows, (jbyte *) input_rows_ptr, 0);

  return ret;
}

JNIEXPORT jbyteArray JNICALL Java_edu_berkeley_cs_rise_opaque_execution_SGXEnclave_FindRangeBounds(
  JNIEnv *env, jobject obj, jlong eid, jbyteArray sort_order, jint num_partitions,
  jbyteArray input_rows) {
  (void)obj;

  jboolean if_copy;

  size_t sort_order_length = static_cast<size_t>(env->GetArrayLength(sort_order));
  uint8_t *sort_order_ptr = reinterpret_cast<uint8_t *>(
    env->GetByteArrayElements(sort_order, &if_copy));

  size_t input_rows_length = static_cast<size_t>(env->GetArrayLength(input_rows));
  uint8_t *input_rows_ptr = reinterpret_cast<uint8_t *>(
    env->GetByteArrayElements(input_rows, &if_copy));

  uint8_t *output_rows;
  size_t output_rows_length;

  sgx_check("Find Range Bounds",
            ecall_find_range_bounds(
              eid,
              sort_order_ptr, sort_order_length,
              num_partitions,
              input_rows_ptr, input_rows_length,
              &output_rows, &output_rows_length));

  jbyteArray ret = env->NewByteArray(output_rows_length);
  env->SetByteArrayRegion(ret, 0, output_rows_length, reinterpret_cast<jbyte *>(output_rows));
  free(output_rows);

  env->ReleaseByteArrayElements(sort_order, reinterpret_cast<jbyte *>(sort_order_ptr), 0);
  env->ReleaseByteArrayElements(input_rows, reinterpret_cast<jbyte *>(input_rows_ptr), 0);

  return ret;
}

JNIEXPORT jobjectArray JNICALL
Java_edu_berkeley_cs_rise_opaque_execution_SGXEnclave_PartitionForSort(
  JNIEnv *env, jobject obj, jlong eid, jbyteArray sort_order, jint num_partitions,
  jbyteArray input_rows, jbyteArray boundary_rows) {
  (void)obj;

  jboolean if_copy;

  size_t sort_order_length = static_cast<size_t>(env->GetArrayLength(sort_order));
  uint8_t *sort_order_ptr = reinterpret_cast<uint8_t *>(
    env->GetByteArrayElements(sort_order, &if_copy));

  size_t input_rows_length = static_cast<size_t>(env->GetArrayLength(input_rows));
  uint8_t *input_rows_ptr = reinterpret_cast<uint8_t *>(
    env->GetByteArrayElements(input_rows, &if_copy));

  size_t boundary_rows_length = static_cast<size_t>(env->GetArrayLength(boundary_rows));
  uint8_t *boundary_rows_ptr = reinterpret_cast<uint8_t *>(
    env->GetByteArrayElements(boundary_rows, &if_copy));

  uint8_t **output_partitions = new uint8_t *[num_partitions];
  size_t *output_partition_lengths = new size_t[num_partitions];

  sgx_check("Partition For Sort",
            ecall_partition_for_sort(
              eid,
              sort_order_ptr, sort_order_length,
              num_partitions,
              input_rows_ptr, input_rows_length,
              boundary_rows_ptr, boundary_rows_length,
              output_partitions, output_partition_lengths));

  env->ReleaseByteArrayElements(sort_order, reinterpret_cast<jbyte *>(sort_order_ptr), 0);
  env->ReleaseByteArrayElements(input_rows, reinterpret_cast<jbyte *>(input_rows_ptr), 0);
  env->ReleaseByteArrayElements(boundary_rows, reinterpret_cast<jbyte *>(boundary_rows_ptr), 0);

  jobjectArray result = env->NewObjectArray(num_partitions,  env->FindClass("[B"), nullptr);
  for (jint i = 0; i < num_partitions; i++) {
    jbyteArray partition = env->NewByteArray(output_partition_lengths[i]);
    env->SetByteArrayRegion(partition, 0, output_partition_lengths[i],
                            reinterpret_cast<jbyte *>(output_partitions[i]));
    free(output_partitions[i]);
    env->SetObjectArrayElement(result, i, partition);
  }
  delete[] output_partitions;
  delete[] output_partition_lengths;

  return result;
}

JNIEXPORT jbyteArray JNICALL Java_edu_berkeley_cs_rise_opaque_execution_SGXEnclave_ExternalSort(
  JNIEnv *env, jobject obj, jlong eid, jbyteArray sort_order, jbyteArray input_rows) {
  (void)obj;

  jboolean if_copy;

  size_t sort_order_length = static_cast<size_t>(env->GetArrayLength(sort_order));
  uint8_t *sort_order_ptr = reinterpret_cast<uint8_t *>(
    env->GetByteArrayElements(sort_order, &if_copy));

  size_t input_rows_length = static_cast<size_t>(env->GetArrayLength(input_rows));
  uint8_t *input_rows_ptr = reinterpret_cast<uint8_t *>(
    env->GetByteArrayElements(input_rows, &if_copy));

  uint8_t *output_rows;
  size_t output_rows_length;

  sgx_check("External non-oblivious sort",
            ecall_external_sort(eid,
                                sort_order_ptr, sort_order_length,
                                input_rows_ptr, input_rows_length,
                                &output_rows, &output_rows_length));

  jbyteArray ret = env->NewByteArray(output_rows_length);
  env->SetByteArrayRegion(ret, 0, output_rows_length, reinterpret_cast<jbyte *>(output_rows));
  free(output_rows);

  env->ReleaseByteArrayElements(sort_order, reinterpret_cast<jbyte *>(sort_order_ptr), 0);
  env->ReleaseByteArrayElements(input_rows, reinterpret_cast<jbyte *>(input_rows_ptr), 0);

  return ret;
}

JNIEXPORT jbyteArray JNICALL
Java_edu_berkeley_cs_rise_opaque_execution_SGXEnclave_ScanCollectLastPrimary(
  JNIEnv *env, jobject obj, jlong eid, jbyteArray join_expr, jbyteArray input_rows) {
  (void)obj;

  jboolean if_copy;

  uint32_t join_expr_length = (uint32_t) env->GetArrayLength(join_expr);
  uint8_t *join_expr_ptr = (uint8_t *) env->GetByteArrayElements(join_expr, &if_copy);

  uint32_t input_rows_length = (uint32_t) env->GetArrayLength(input_rows);
  uint8_t *input_rows_ptr = (uint8_t *) env->GetByteArrayElements(input_rows, &if_copy);

  uint8_t *output_rows;
  size_t output_rows_length;

  sgx_check("Scan Collect Last Primary",
            ecall_scan_collect_last_primary(
              eid,
              join_expr_ptr, join_expr_length,
              input_rows_ptr, input_rows_length,
              &output_rows, &output_rows_length));

  jbyteArray ret = env->NewByteArray(output_rows_length);
  env->SetByteArrayRegion(ret, 0, output_rows_length, (jbyte *) output_rows);
  free(output_rows);

  env->ReleaseByteArrayElements(join_expr, (jbyte *) join_expr_ptr, 0);
  env->ReleaseByteArrayElements(input_rows, (jbyte *) input_rows_ptr, 0);

  return ret;
}

JNIEXPORT jbyteArray JNICALL
Java_edu_berkeley_cs_rise_opaque_execution_SGXEnclave_NonObliviousSortMergeJoin(
  JNIEnv *env, jobject obj, jlong eid, jbyteArray join_expr, jbyteArray input_rows,
  jbyteArray join_row) {
  (void)obj;

  jboolean if_copy;

  uint32_t join_expr_length = (uint32_t) env->GetArrayLength(join_expr);
  uint8_t *join_expr_ptr = (uint8_t *) env->GetByteArrayElements(join_expr, &if_copy);

  uint32_t input_rows_length = (uint32_t) env->GetArrayLength(input_rows);
  uint8_t *input_rows_ptr = (uint8_t *) env->GetByteArrayElements(input_rows, &if_copy);

  uint32_t join_row_length = (uint32_t) env->GetArrayLength(join_row);
  uint8_t *join_row_ptr = (uint8_t *) env->GetByteArrayElements(join_row, &if_copy);

  uint8_t *output_rows;
  size_t output_rows_length;

  sgx_check("Non-oblivious SortMergeJoin",
            ecall_non_oblivious_sort_merge_join(
              eid,
              join_expr_ptr, join_expr_length,
              input_rows_ptr, input_rows_length,
              join_row_ptr, join_row_length,
              &output_rows, &output_rows_length));

  jbyteArray ret = env->NewByteArray(output_rows_length);
  env->SetByteArrayRegion(ret, 0, output_rows_length, (jbyte *) output_rows);
  free(output_rows);

  env->ReleaseByteArrayElements(join_expr, (jbyte *) join_expr_ptr, 0);
  env->ReleaseByteArrayElements(input_rows, (jbyte *) input_rows_ptr, 0);
  env->ReleaseByteArrayElements(join_row, (jbyte *) join_row_ptr, 0);

  return ret;
}

JNIEXPORT jobject JNICALL
Java_edu_berkeley_cs_rise_opaque_execution_SGXEnclave_NonObliviousAggregateStep1(
  JNIEnv *env, jobject obj, jlong eid, jbyteArray agg_op, jbyteArray input_rows) {
  (void)obj;

  jboolean if_copy;

  uint32_t agg_op_length = (uint32_t) env->GetArrayLength(agg_op);
  uint8_t *agg_op_ptr = (uint8_t *) env->GetByteArrayElements(agg_op, &if_copy);

  uint32_t input_rows_length = (uint32_t) env->GetArrayLength(input_rows);
  uint8_t *input_rows_ptr = (uint8_t *) env->GetByteArrayElements(input_rows, &if_copy);

  uint8_t *first_row;
  size_t first_row_length;

  uint8_t *last_group;
  size_t last_group_length;

  uint8_t *last_row;
  size_t last_row_length;

  sgx_check("Non-Oblivious Aggregate Step 1",
            ecall_non_oblivious_aggregate_step1(
              eid,
              agg_op_ptr, agg_op_length,
              input_rows_ptr, input_rows_length,
              &first_row, &first_row_length,
              &last_group, &last_group_length,
              &last_row, &last_row_length));

  jbyteArray first_row_array = env->NewByteArray(first_row_length);
  env->SetByteArrayRegion(first_row_array, 0, first_row_length, (jbyte *) first_row);
  free(first_row);

  jbyteArray last_group_array = env->NewByteArray(last_group_length);
  env->SetByteArrayRegion(last_group_array, 0, last_group_length, (jbyte *) last_group);
  free(last_group);

  jbyteArray last_row_array = env->NewByteArray(last_row_length);
  env->SetByteArrayRegion(last_row_array, 0, last_row_length, (jbyte *) last_row);
  free(last_row);

  env->ReleaseByteArrayElements(agg_op, (jbyte *) agg_op_ptr, 0);
  env->ReleaseByteArrayElements(input_rows, (jbyte *) input_rows_ptr, 0);

  jclass tuple3_class = env->FindClass("scala/Tuple3");
  jobject ret = env->NewObject(
    tuple3_class,
    env->GetMethodID(tuple3_class, "<init>",
                     "(Ljava/lang/Object;Ljava/lang/Object;Ljava/lang/Object;)V"),
    first_row_array, last_group_array, last_row_array);

  return ret;
}

JNIEXPORT jbyteArray JNICALL
Java_edu_berkeley_cs_rise_opaque_execution_SGXEnclave_NonObliviousAggregateStep2(
  JNIEnv *env, jobject obj, jlong eid, jbyteArray agg_op, jbyteArray input_rows,
  jbyteArray next_partition_first_row, jbyteArray prev_partition_last_group,
  jbyteArray prev_partition_last_row) {
  (void)obj;

  jboolean if_copy;

  uint32_t agg_op_length = (uint32_t) env->GetArrayLength(agg_op);
  uint8_t *agg_op_ptr = (uint8_t *) env->GetByteArrayElements(agg_op, &if_copy);

  uint32_t input_rows_length = (uint32_t) env->GetArrayLength(input_rows);
  uint8_t *input_rows_ptr = (uint8_t *) env->GetByteArrayElements(input_rows, &if_copy);

  uint32_t next_partition_first_row_length =
    (uint32_t) env->GetArrayLength(next_partition_first_row);
  uint8_t *next_partition_first_row_ptr =
    (uint8_t *) env->GetByteArrayElements(next_partition_first_row, &if_copy);

  uint32_t prev_partition_last_group_length =
    (uint32_t) env->GetArrayLength(prev_partition_last_group);
  uint8_t *prev_partition_last_group_ptr =
    (uint8_t *) env->GetByteArrayElements(prev_partition_last_group, &if_copy);

  uint32_t prev_partition_last_row_length =
    (uint32_t) env->GetArrayLength(prev_partition_last_row);
  uint8_t *prev_partition_last_row_ptr =
    (uint8_t *) env->GetByteArrayElements(prev_partition_last_row, &if_copy);

  uint8_t *output_rows;
  size_t output_rows_length;

  sgx_check("Non-Oblivious Aggregate Step 2",
            ecall_non_oblivious_aggregate_step2(
              eid,
              agg_op_ptr, agg_op_length,
              input_rows_ptr, input_rows_length,
              next_partition_first_row_ptr, next_partition_first_row_length,
              prev_partition_last_group_ptr, prev_partition_last_group_length,
              prev_partition_last_row_ptr, prev_partition_last_row_length,
              &output_rows, &output_rows_length));

  jbyteArray ret = env->NewByteArray(output_rows_length);
  env->SetByteArrayRegion(ret, 0, output_rows_length, (jbyte *) output_rows);
  free(output_rows);

  env->ReleaseByteArrayElements(agg_op, (jbyte *) agg_op_ptr, 0);
  env->ReleaseByteArrayElements(input_rows, (jbyte *) input_rows_ptr, 0);
  env->ReleaseByteArrayElements(
    next_partition_first_row, (jbyte *) next_partition_first_row_ptr, 0);
  env->ReleaseByteArrayElements(
    prev_partition_last_group, (jbyte *) prev_partition_last_group_ptr, 0);
  env->ReleaseByteArrayElements(
    prev_partition_last_row, (jbyte *) prev_partition_last_row_ptr, 0);

  return ret;
}
