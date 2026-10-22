#include "bruce/mod/runtime_registry.hpp"
#include "bruce/mod/logic_mod_registry_v1.hpp"
#include <iostream>

// Linked to the two frozen V1 implementations, never to their V2 replacements.
int main(int argc, char** argv) {
    if (argc != 4) return 2;
    try {
        if (std::string_view(argv[1]) == "data") {
            auto registry = bruce::RuntimeModRegistryV1::open_existing(argv[2]);
            if (std::string_view(argv[3]) == "mutate") registry.disable("user.power");
            else (void)registry.snapshot();
        } else {
            auto registry = bruce::DurableLogicModRegistryV1::open_existing(argv[2]);
            if (std::string_view(argv[3]) == "mutate") registry.disable("user.power");
            else (void)registry.snapshot();
        }
        return 0;
    } catch (const std::exception& e) { std::cerr << e.what() << '\n'; return 1; }
}
