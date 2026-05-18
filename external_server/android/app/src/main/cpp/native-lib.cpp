#include <jni.h>
#include <string>
#include <vector>
#include "sniffer_client.h"

extern "C" JNIEXPORT jlong JNICALL
Java_elazarkin_ksg_external_service_jni_NativeInterface_clientCreate(
        JNIEnv* env, jclass clazz, jstring ip, jint remotePort, jint localPort, jint keepAliveMs)
{
    const char* nativeIp = env->GetStringUTFChars(ip, nullptr);
    void* handle = client_create(nativeIp, (uint16_t)remotePort, (uint16_t)localPort, (uint32_t)keepAliveMs);
    env->ReleaseStringUTFChars(ip, nativeIp);
    return reinterpret_cast<jlong>(handle);
}

extern "C" JNIEXPORT jint JNICALL
Java_elazarkin_ksg_external_service_jni_NativeInterface_clientStart(
        JNIEnv* env, jclass clazz, jlong handle)
{
    return client_start(reinterpret_cast<void*>(handle));
}

extern "C" JNIEXPORT void JNICALL
Java_elazarkin_ksg_external_service_jni_NativeInterface_clientStop(
        JNIEnv* env, jclass clazz, jlong handle)
{
    client_stop(reinterpret_cast<void*>(handle));
}

extern "C" JNIEXPORT void JNICALL
Java_elazarkin_ksg_external_service_jni_NativeInterface_clientDestroy(
        JNIEnv* env, jclass clazz, jlong handle)
{
    client_destroy(reinterpret_cast<void*>(handle));
}

extern "C" JNIEXPORT jboolean JNICALL
Java_elazarkin_ksg_external_service_jni_NativeInterface_clientIsConnected(
        JNIEnv* env, jclass clazz, jlong handle)
{
    return (jboolean)client_is_connected(reinterpret_cast<void*>(handle));
}

extern "C" JNIEXPORT jstring JNICALL
Java_elazarkin_ksg_external_service_jni_NativeInterface_clientGetSnifferStatus(
        JNIEnv* env, jclass clazz, jlong handle)
{
    char buf[1024];
    if (client_get_sniffer_status(reinterpret_cast<void*>(handle), buf, sizeof(buf)))
    {
        return env->NewStringUTF(buf);
    }
    return nullptr;
}

extern "C" JNIEXPORT jint JNICALL
Java_elazarkin_ksg_external_service_jni_NativeInterface_clientReadMessage(
        JNIEnv* env, jclass clazz, jlong handle, jintArray cmd, jdoubleArray timeMs, jbyteArray data, jintArray dataLen, jint timeoutMs)
{
    uint32_t nativeCommand;
    double nativeTimeMs;
    uint32_t nativeLen;
    
    // Get pointers to Java arrays
    jint* pCmd = env->GetIntArrayElements(cmd, nullptr);
    jdouble* pTime = env->GetDoubleArrayElements(timeMs, nullptr);
    jint* pLen = env->GetIntArrayElements(dataLen, nullptr);
    jbyte* pData = env->GetByteArrayElements(data, nullptr);
    
    int res = client_read_message(reinterpret_cast<void*>(handle), &nativeCommand, &nativeTimeMs, reinterpret_cast<uint8_t*>(pData), &nativeLen, timeoutMs);
    
    if (res > 0)
    {
        pCmd[0] = (jint)nativeCommand;
        pTime[0] = (jdouble)nativeTimeMs;
        pLen[0] = (jint)nativeLen;
    }
    
    // Release and commit changes back to Java
    env->ReleaseIntArrayElements(cmd, pCmd, 0);
    env->ReleaseDoubleArrayElements(timeMs, pTime, 0);
    env->ReleaseIntArrayElements(dataLen, pLen, 0);
    env->ReleaseByteArrayElements(data, pData, 0);
    
    return (jint)res;
}

extern "C" JNIEXPORT void JNICALL
Java_elazarkin_ksg_external_service_jni_NativeInterface_clientSendRawCommand(
        JNIEnv* env, jclass clazz, jlong handle, jint commandId, jbyteArray payload)
{
    if (payload == nullptr)
    {
        client_send_raw_command(reinterpret_cast<void*>(handle), (uint32_t)commandId, nullptr, 0);
    }
    else
    {
        jsize len = env->GetArrayLength(payload);
        jbyte* body = env->GetByteArrayElements(payload, nullptr);
        client_send_raw_command(reinterpret_cast<void*>(handle), (uint32_t)commandId, reinterpret_cast<const uint8_t*>(body), (size_t)len);
        env->ReleaseByteArrayElements(payload, body, JNI_ABORT);
    }
}

extern "C" JNIEXPORT jstring JNICALL
Java_elazarkin_ksg_external_service_MainActivity_stringFromJNI(
        JNIEnv* env,
        jobject /* this */)
{
    std::string hello = "KSG Sniffer Native Active";
    return env->NewStringUTF(hello.c_str());
}
