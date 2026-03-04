#include "core/MainService.h"

extern "C" {

void* service_create(const char* config_path) {
    return new core::MainService(config_path);
}

void service_run(void* handle) {
    if (handle) {
        ((core::MainService*)handle)->run();
    }
}

void service_stop(void* handle) {
    if (handle) {
        ((core::MainService*)handle)->stop();
    }
}

void service_destroy(void* handle) {
    if (handle) {
        delete (core::MainService*)handle;
    }
}

}
