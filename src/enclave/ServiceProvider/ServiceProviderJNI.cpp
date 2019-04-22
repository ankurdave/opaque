#include "SP.h"

#include <cstdint>
#include <cstdio>

#include "ServiceProvider.h"

/**
 * Throw a Java exception with the specified message.
 *
 * Important: Note that this function will return to the caller. The exception is only thrown at the
 * end of the JNI method invocation.
 */
void jni_throw(JNIEnv *env, const char *message) {
  jclass exception = env->FindClass("edu/berkeley/cs/rise/opaque/OpaqueException");
  env->ThrowNew(exception, message);
}

JNIEXPORT void JNICALL Java_edu_berkeley_cs_rise_opaque_execution_SP_LoadKeys(
  JNIEnv *env, jobject obj, jbyteArray shared_key) {
  (void)env;
  (void)obj;

  jboolean if_copy = false;
  jbyte *shared_key_bytes = env->GetByteArrayElements(shared_key, &if_copy);

  try {
    std::string private_key_filename(std::getenv("PRIVATE_KEY_PATH"));
    service_provider.load_private_key(private_key_filename);
    service_provider.set_shared_key(reinterpret_cast<uint8_t *>(shared_key_bytes));
  } catch (const std::runtime_error &e) {
    jni_throw(env, e.what());
  }

  env->ReleaseByteArrayElements(shared_key, shared_key_bytes, 0);
}


JNIEXPORT void JNICALL Java_edu_berkeley_cs_rise_opaque_execution_SP_SPProcMsg0(
  JNIEnv *env, jobject obj, jbyteArray msg0_input) {
  (void)obj;

  jboolean if_copy = false;
  jbyte *msg0_bytes = env->GetByteArrayElements(msg0_input, &if_copy);
  uint32_t *extended_epid_group_id = reinterpret_cast<uint32_t *>(msg0_bytes);

  // "The Intel Attestation Service only supports the value of zero for the extended GID."
  if (extended_epid_group_id != 0) {
    jni_throw(env, "Remote attestation step 0: Unsupported extended EPID group");
  }

  env->ReleaseByteArrayElements(msg0_input, msg0_bytes, 0);
}

JNIEXPORT jbyteArray JNICALL Java_edu_berkeley_cs_rise_opaque_execution_SP_SPProcMsg1(
  JNIEnv *env, jobject obj, jbyteArray msg1_input) {
  (void)obj;

  jboolean if_copy = false;
  jbyte *msg1_bytes = env->GetByteArrayElements(msg1_input, &if_copy);
  sgx_ra_msg1_t *msg1 = reinterpret_cast<sgx_ra_msg1_t *>(msg1_bytes);

  uint32_t msg2_size = 0;
  std::unique_ptr<sgx_ra_msg2_t> msg2;
  try {
     msg2 = service_provider.process_msg1(msg1, &msg2_size);
  } catch (const std::runtime_error &e) {
    jni_throw(env, e.what());
  }

  jbyteArray array_ret = env->NewByteArray(msg2_size);
  env->SetByteArrayRegion(array_ret, 0, msg2_size, reinterpret_cast<jbyte *>(msg2.get()));

  env->ReleaseByteArrayElements(msg1_input, msg1_bytes, 0);

  return array_ret;
}

JNIEXPORT jbyteArray JNICALL Java_edu_berkeley_cs_rise_opaque_execution_SP_SPProcMsg3(
  JNIEnv *env, jobject obj, jbyteArray msg3_input) {
  (void)obj;

  jboolean if_copy = false;
  jbyte *msg3_bytes = env->GetByteArrayElements(msg3_input, &if_copy);
  sgx_ra_msg3_t *msg3 = reinterpret_cast<sgx_ra_msg3_t *>(msg3_bytes);
  uint32_t msg3_size = static_cast<uint32_t>(env->GetArrayLength(msg3_input));

  std::unique_ptr<ra_msg4_t> msg4;
  try {
    msg4 = service_provider.process_msg3(msg3, msg3_size);
  } catch (const std::runtime_error &e) {
    jni_throw(env, e.what());
  }

  jbyteArray ret = env->NewByteArray(sizeof(ra_msg4_t));
  env->SetByteArrayRegion(ret, 0, sizeof(ra_msg4_t), reinterpret_cast<jbyte *>(msg4.get()));

  env->ReleaseByteArrayElements(msg3_input, msg3_bytes, 0);

  return ret;
}
