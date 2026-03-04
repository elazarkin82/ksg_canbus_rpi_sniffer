#include "utils/Params.hpp"
#include <iostream>
#include <cassert>
#include <string.h>
#include <unistd.h>

using namespace utils;

// Colors
#define GREEN "\033[32m"
#define RED "\033[31m"
#define RESET "\033[0m"

void print_result(const char* test_name, bool passed) {
    std::cout << "TEST: " << test_name << " -> ";
    if (passed) {
        std::cout << GREEN << "PASSED" << RESET << std::endl;
    } else {
        std::cout << RED << "FAILED" << RESET << std::endl;
    }
}

void test_defaults() {
    const char* defaults = "key1=val1\nkey2=val2";
    const char* file = "test_defaults.prop";
    unlink(file);

    Params p(file, defaults);

    bool passed = true;
    if (strcmp(p.get("key1"), "val1") != 0) passed = false;
    if (strcmp(p.get("key2"), "val2") != 0) passed = false;
    if (p.get("key3") != nullptr) passed = false;

    unlink(file);
    print_result("Defaults", passed);
}

void test_load_from_file() {
    const char* file = "test_load.prop";
    FILE* f = fopen(file, "w");
    fprintf(f, "key1=new_val1\n");
    fclose(f);

    const char* defaults = "key1=def1\nkey2=def2";
    Params p(file, defaults);

    bool passed = true;
    if (strcmp(p.get("key1"), "new_val1") != 0) passed = false;
    if (strcmp(p.get("key2"), "def2") != 0) passed = false;

    unlink(file);
    print_result("Load from File", passed);
}

void test_parsing() {
    const char* defaults = "k1=v1\nk2=v2\nk3=v3";
    const char* file = "test_parse.prop";
    FILE* f = fopen(file, "w");
    fprintf(f, "  k1  =  val1  \n");
    fprintf(f, "k2=\" val 2 \"\n");
    fprintf(f, "\n");
    fprintf(f, "k3=val3\n");
    fclose(f);

    Params p(file, defaults);

    bool passed = true;
    if (strcmp(p.get("k1"), "val1") != 0) passed = false;
    if (strcmp(p.get("k2"), " val 2 ") != 0) passed = false;
    if (strcmp(p.get("k3"), "val3") != 0) passed = false;

    unlink(file);
    print_result("Parsing", passed);
}

void test_update_save() {
    const char* file = "test_update.prop";
    const char* defaults = "key1=val1";
    unlink(file);

    bool passed = true;
    {
        Params p(file, defaults);
        p.update("key1=updated");
        if (strcmp(p.get("key1"), "updated") != 0) passed = false;
    }

    {
        Params p(file, defaults);
        if (strcmp(p.get("key1"), "updated") != 0) passed = false;
    }

    unlink(file);
    print_result("Update & Save", passed);
}

void test_unknown_key() {
    const char* file = "test_unknown.prop";
    const char* defaults = "key1=val1";
    unlink(file);

    Params p(file, defaults);
    p.update("unknown=val");

    bool passed = (p.get("unknown") == nullptr);

    unlink(file);
    print_result("Unknown Key", passed);
}

void test_types() {
    const char* file = "test_types.prop";
    const char* defaults = "port=9095\nflag=1";
    unlink(file);

    Params p(file, defaults);

    bool passed = true;
    if (p.getInt("port", 0) != 9095) passed = false;
    if (p.getInt("flag", 0) != 1) passed = false;
    if (p.getInt("missing", 123) != 123) passed = false;

    unlink(file);
    print_result("Types", passed);
}

int main() {
    std::cout << "=== Starting Params Tests ===" << std::endl;
    test_defaults();
    test_load_from_file();
    test_parsing();
    test_update_save();
    test_unknown_key();
    test_types();
    return 0;
}
