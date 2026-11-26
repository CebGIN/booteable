#include <iostream>
#include <string>
#include <vector>

#include "TestHarness.hpp"

void run_validation_tests();
void run_fake_controller_tests();
void run_core_tests();
void run_layout_tests();
void run_gpt_tests();
void run_fat32_tests();
void run_image_roundtrip_tests();
void run_safety_policy_tests();
void run_sysfs_source_tests();
void run_mount_manager_tests();
void run_device_writer_tests();
void run_flash_tests();
void run_prep_controller_tests();
void run_ui_flow_tests();
void run_ui_scene_tests();

namespace booteable::test {
int failures = 0;

void report_failure(const char* file, int line, const std::string& message) {
    ++failures;
    std::cerr << file << ':' << line << ": " << message << '\n';
}
} // namespace booteable::test

namespace {

struct Suite {
    const char* name;
    void (*run)();
};

const Suite kSuites[] = {
    {"validation", &run_validation_tests},
    {"fake_controller", &run_fake_controller_tests},
    {"core", &run_core_tests},
    {"layout", &run_layout_tests},
    {"gpt", &run_gpt_tests},
    {"fat32", &run_fat32_tests},
    {"image_roundtrip", &run_image_roundtrip_tests},
    {"safety_policy", &run_safety_policy_tests},
    {"sysfs_source", &run_sysfs_source_tests},
    {"mount_manager", &run_mount_manager_tests},
    {"device_writer", &run_device_writer_tests},
    {"flash", &run_flash_tests},
    {"prep_controller", &run_prep_controller_tests},
    {"ui_flow", &run_ui_flow_tests},
    {"ui_scene", &run_ui_scene_tests},
};

void print_suites() {
    std::cerr << "Suites disponibles:\n";
    for (const Suite& suite : kSuites) {
        std::cerr << "  " << suite.name << '\n';
    }
}

const Suite* find_suite(const std::string& name) {
    for (const Suite& suite : kSuites) {
        if (name == suite.name) {
            return &suite;
        }
    }
    return nullptr;
}

} // namespace

int main(int argc, char** argv) {
    std::vector<const Suite*> selected;

    if (argc == 1) {
        for (const Suite& suite : kSuites) {
            selected.push_back(&suite);
        }
    } else {
        for (int index = 1; index < argc; ++index) {
            const Suite* suite = find_suite(argv[index]);
            if (suite == nullptr) {
                std::cerr << "Suite desconocida: " << argv[index] << '\n';
                print_suites();
                return 2;
            }
            selected.push_back(suite);
        }
    }

    for (const Suite* suite : selected) {
        suite->run();
    }

    if (booteable::test::failures != 0) {
        std::cerr << booteable::test::failures << " comprobación(es) fallaron.\n";
        return 1;
    }
    std::cout << "Todas las comprobaciones pasaron.\n";
    return 0;
}
