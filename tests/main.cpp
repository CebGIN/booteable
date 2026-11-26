#include <iostream>

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
void run_ui_flow_tests();

namespace booteable::test {
int failures = 0;

void report_failure(const char* file, int line, const std::string& message) {
    ++failures;
    std::cerr << file << ':' << line << ": " << message << '\n';
}
} // namespace booteable::test

int main() {
    run_validation_tests();
    run_fake_controller_tests();
    run_core_tests();
    run_layout_tests();
    run_gpt_tests();
    run_fat32_tests();
    run_image_roundtrip_tests();
    run_safety_policy_tests();
    run_sysfs_source_tests();
    run_mount_manager_tests();
    run_device_writer_tests();
    run_flash_tests();
    run_ui_flow_tests();

    if (booteable::test::failures != 0) {
        std::cerr << booteable::test::failures << " comprobación(es) fallaron.\n";
        return 1;
    }
    std::cout << "Todas las comprobaciones pasaron.\n";
    return 0;
}
