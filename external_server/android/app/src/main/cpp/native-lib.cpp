#include <jni.h>
#include <string>
#include <vector>
#include "sniffer_client.h"

extern "C" JNIEXPORT jlong JNICALL
Java_elazarkin_ksg_external_service_jni_NativeInterface_clientCreate(
        JNIEnv* env, jclass clazz, jstring ip, jint remotePort, jint localPort, jint keepAliveMs) {
    const char* nativeIp = env->GetStringUTFChars(ip, nullptr);
    void* handle = client_create(nativeIp, (uint16_t)remotePort, (uint16_t)localPort, (uint32_t)keepAliveMs);
    env->ReleaseStringUTFChars(ip, nativeIp);
    return reinterpret_cast<jlong>(handle);
}

extern "C" JNIEXPORT jint JNICALL
Java_elazarkin_ksg_external_service_jni_NativeInterface_clientStart(
        JNIEnv* env, jclass clazz, jlong handle) {
    return client_start(reinterpret_cast<void*>(handle));
}

extern "C" JNIEXPORT void JNICALL
Java_elazarkin_ksg_external_service_jni_NativeInterface_clientStop(
        JNIEnv* env, jclass clazz, jlong handle) {
    client_stop(reinterpret_cast<void*>(handle));
}

extern "C" JNIEXPORT void JNICALL
Java_elazarkin_ksg_external_service_jni_NativeInterface_clientDestroy(
        JNIEnv* env, jclass clazz, jlong handle) {
    client_destroy(reinterpret_cast<void*>(handle));
}

extern "C" JNIEXPORT jboolean JNICALL
Java_elazarkin_ksg_external_service_jni_NativeInterface_clientIsConnected(
        JNIEnv* env, jclass clazz, jlong handle) {
    return (jboolean)client_is_connected(reinterpret_cast<void*>(handle));
}

extern "C" JNIEXPORT jstring JNICALL
Java_elazarkin_ksg_external_service_jni_NativeInterface_clientGetSnifferStatus(
        JNIEnv* env, jclass clazz, jlong handle) {
    char buf[1024];
    if (client_get_sniffer_status(reinterpret_cast<void*>(handle), buf, sizeof(buf))) {
        return env->NewStringUTF(buf);
    }
    return nullptr;
}

extern "C" JNIEXPORT jbyteArray JNICALL
Java_elazarkin_ksg_external_service_jni_NativeInterface_clientReadMessage(
        JNIEnv* env, jclass clazz, jlong handle, jint timeoutMs) {
    uint32_t command;
    double time_ms;
    uint8_t data[2048]; // Enough for ExternalMessageV1
    uint32_t len;
    
    int res = client_read_message(reinterpret_cast<void*>(handle), &command, &time_ms, data, &len, timeoutMs);
    
    if (res > 0) {
        // For simplicity in this first step, we return the raw payload as a byte array
        // In a more advanced version, we would return an object or a structured array
        jbyteArray result = env->NewByteArray(len);
        env->SetByteArrayRegion(result, 0, len, reinterpret_cast<jbyte*>(data));
        return result;
    }
    return nullptr;
}

extern "C" JNIEXPORT jstring JNICALL
Java_elazarkin_ksg_external_service_MainActivity_stringFromJNI(
        JNIEnv* env,
        jobject /* this */) {
    std::string hello = "KSG Sniffer Native Active";
    return env->NewStringUTF(hello.c_str());
}
