#include "utils/Params.hpp"
#include <iostream>
#include <cassert>
#include <string.h>
#include <unistd.h>

using namespace utils;

void test_defaults() {
    std::cout << "Running test_defaults..." << std::endl;
    const char* defaults = "key1=val1\nkey2=val2";
    const char* file = "test_defaults.prop";
    unlink(file); // Ensure clean state

    Params p(file, defaults);

    assert(strcmp(p.get("key1"), "val1") == 0);
    assert(strcmp(p.get("key2"), "val2") == 0);
    assert(p.get("key3") == nullptr);

    unlink(file);
    std::cout << "PASSED" << std::endl;
}

void test_load_from_file() {
    std::cout << "Running test_load_from_file..." << std::endl;
    const char* file = "test_load.prop";
    FILE* f = fopen(file, "w");
    fprintf(f, "key1=new_val1\n");
    fclose(f);

    const char* defaults = "key1=def1\nkey2=def2";
    Params p(file, defaults);

    assert(strcmp(p.get("key1"), "new_val1") == 0);
    assert(strcmp(p.get("key2"), "def2") == 0);

    unlink(file);
    std::cout << "PASSED" << std::endl;
}

void test_parsing() {
    std::cout << "Running test_parsing..." << std::endl;
    const char* defaults = "k1=v1\nk2=v2\nk3=v3";
    const char* file = "test_parse.prop";
    FILE* f = fopen(file, "w");
    fprintf(f, "  k1  =  val1  \n"); // Whitespace
    fprintf(f, "k2=\" val 2 \"\n"); // Quotes
    fprintf(f, "\n"); // Empty line
    fprintf(f, "k3=val3\n");
    fclose(f);

    Params p(file, defaults);

    assert(strcmp(p.get("k1"), "val1") == 0);
    assert(strcmp(p.get("k2"), " val 2 ") == 0);
    assert(strcmp(p.get("k3"), "val3") == 0);

    unlink(file);
    std::cout << "PASSED" << std::endl;
}

void test_update_save() {
    std::cout << "Running test_update_save..." << std::endl;
    const char* file = "test_update.prop";
    const char* defaults = "key1=val1";
    unlink(file);

    {
        Params p(file, defaults);
        p.update("key1=updated");
        assert(strcmp(p.get("key1"), "updated") == 0);
    }

    // Reload to check persistence
    {
        Params p(file, defaults);
        assert(strcmp(p.get("key1"), "updated") == 0);
    }

    unlink(file);
    std::cout << "PASSED" << std::endl;
}

void test_unknown_key() {
    std::cout << "Running test_unknown_key..." << std::endl;
    const char* file = "test_unknown.prop";
    const char* defaults = "key1=val1";
    unlink(file);

    Params p(file, defaults);

    // Try to update unknown key
    // Capture stderr? Hard in simple test. Just check logic.
    p.update("unknown=val");

    assert(p.get("unknown") == nullptr);

    unlink(file);
    std::cout << "PASSED" << std::endl;
}

void test_types() {
    std::cout << "Running test_types..." << std::endl;
    const char* file = "test_types.prop";
    const char* defaults = "port=9095\nflag=1";
    unlink(file);

    Params p(file, defaults);

    assert(p.getInt("port", 0) == 9095);
    assert(p.getInt("flag", 0) == 1);
    assert(p.getInt("missing", 123) == 123);

    unlink(file);
    std::cout << "PASSED" << std::endl;
}

int main() {
    test_defaults();
    test_load_from_file();
    test_parsing();
    test_update_save();
    test_unknown_key();
    test_types();

    std::cout << "All Params tests passed!" << std::endl;
    return 0;
}
