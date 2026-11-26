cmake_minimum_required(VERSION 3.20)

project(booteable
    VERSION 0.1.0
    DESCRIPTION "Preparador UEFI de USB con programa de arranque propio"
    LANGUAGES CXX
)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

if(WIN32)
    message(FATAL_ERROR
        "El preparador solo se compila en Linux: usa sysfs, ioctl de "
        "bloque y umount2.")
endif()

# --- Herramientas obligatorias ---------------------------------------------------
find_program(EFI_CXX
    NAMES x86_64-w64-mingw32-g++
    DOC "Cross-compiler de MinGW-w64 para la aplicación EFI x86_64")

if(NOT EFI_CXX)
    message(FATAL_ERROR
        "No se encontró x86_64-w64-mingw32-g++.\n"
        "Instalá MinGW-w64 para compilar BOOTX64.EFI, por ejemplo:\n"
        "  sudo apt install g++-mingw-w64-x86-64")
endif()

find_package(Python3 3.8 COMPONENTS Interpreter REQUIRED)

# --- GuayabaTUIEngine ------------------------------------------------------------
set(GUAYABA_ENGINE_DIR
    "${CMAKE_CURRENT_SOURCE_DIR}/external/GuayabaTUIEngine"
    CACHE PATH "Ruta al árbol de fuentes de GuayabaTUIEngine")

if(NOT EXISTS "${GUAYABA_ENGINE_DIR}/CMakeLists.txt")
    message(FATAL_ERROR
        "No se encontró GuayabaTUIEngine en '${GUAYABA_ENGINE_DIR}'.\n"
        "Inicializá el submódulo con:\n"
        "  git submodule update --init --recursive")
endif()

add_subdirectory(
    "${GUAYABA_ENGINE_DIR}"
    "${CMAKE_BINARY_DIR}/_deps/GuayabaTUIEngine")

# --- Avisos de compilación -------------------------------------------------------
function(booteable_warnings target)
    if(MSVC)
        target_compile_options(${target} PRIVATE /utf8 /W4)
    else()
        target_compile_options(${target} PRIVATE -Wall -Wextra -Wpedantic)
    endif()
endfunction()

# --- Aplicación EFI --------------------------------------------------------------
set(EFI_OUTPUT_DIR "${CMAKE_BINARY_DIR}/efi")
set(EFI_BINARY "${EFI_OUTPUT_DIR}/BOOTX64.EFI")

add_custom_command(
    OUTPUT "${EFI_BINARY}"
    COMMAND "${CMAKE_COMMAND}" -E env
            "EFI_CXX=${EFI_CXX}"
            "EFI_OUTPUT=${EFI_BINARY}"
            bash "${CMAKE_CURRENT_SOURCE_DIR}/tools/build-efi.sh"
    DEPENDS
        "${CMAKE_CURRENT_SOURCE_DIR}/boot/main.cpp"
        "${CMAKE_CURRENT_SOURCE_DIR}/boot/efi.cpp"
        "${CMAKE_CURRENT_SOURCE_DIR}/boot/efi.h"
        "${CMAKE_CURRENT_SOURCE_DIR}/tools/build-efi.sh"
    COMMENT "Compilando BOOTX64.EFI con MinGW-w64"
    VERBATIM)

# --- EFI incrustado --------------------------------------------------------------
set(GENERATED_DIR "${CMAKE_BINARY_DIR}/generated")
set(EMBEDDED_HPP "${GENERATED_DIR}/EmbeddedBootImage.hpp")
set(EMBEDDED_CPP "${GENERATED_DIR}/EmbeddedBootImage.cpp")

add_custom_command(
    OUTPUT "${EMBEDDED_HPP}" "${EMBEDDED_CPP}"
    COMMAND "${Python3_EXECUTABLE}"
            "${CMAKE_CURRENT_SOURCE_DIR}/tools/embed-efi.py"
            --input "${EFI_BINARY}"
            --output-dir "${GENERATED_DIR}"
    DEPENDS
        "${EFI_BINARY}"
        "${CMAKE_CURRENT_SOURCE_DIR}/tools/embed-efi.py"
    COMMENT "Incrustando BOOTX64.EFI y calculando su SHA-256"
    VERBATIM)

add_custom_target(booteable_embedded DEPENDS "${EMBEDDED_CPP}")

# El target público depende del incrustado para que el comando EFI se ejecute
# una sola vez, aunque haya varios consumidores en paralelo.
add_custom_target(bootx64_efi ALL)
add_dependencies(bootx64_efi booteable_embedded)

# --- Núcleo headless -------------------------------------------------------------
# Sin Guayaba ni UI: dominio, core, imagen, fake y el EFI incrustado.
add_library(booteable_core STATIC
    src/domain/Validation.cpp
    src/core/Crc32.cpp
    src/core/Sha256.cpp
    src/image/ImageLayout.cpp
    src/image/GptWriter.cpp
    src/image/ImageFile.cpp
    src/image/Fat32Writer.cpp
    src/image/BootImageBuilder.cpp
    src/image/ImageVerifier.cpp
    src/device/DeviceIdentity.cpp
    src/device/SystemContext.cpp
    src/device/SysfsBlockDeviceSource.cpp
    src/device/SafetyPolicy.cpp
    src/device/MountManager.cpp
    src/device/PosixDeviceWriter.cpp
    src/device/FileDeviceWriter.cpp
    src/device/FaultyDeviceWriter.cpp
    src/device/DeviceFlasher.cpp
    src/app/FakePrepController.cpp
    src/app/PrepController.cpp
    src/app/PrepRunner.cpp
    "${EMBEDDED_CPP}")
add_dependencies(booteable_core booteable_embedded)
target_include_directories(booteable_core PUBLIC
    "${CMAKE_CURRENT_SOURCE_DIR}/src"
    "${GENERATED_DIR}")
booteable_warnings(booteable_core)

# --- UI --------------------------------------------------------------------------
add_library(booteable_ui STATIC
    src/ui/AppScene.cpp
    src/ui/MainScene.cpp
    src/ui/ConfirmDialog.cpp
    src/ui/ProgressScene.cpp
    src/ui/ResultScene.cpp
    src/ui/UiFormat.cpp)
target_link_libraries(booteable_ui PUBLIC
    booteable_core
    GuayabaTUIEngine::GuayabaTUIEngine)
booteable_warnings(booteable_ui)

# --- Aplicación TUI --------------------------------------------------------------
add_executable(booteable src/main.cpp)
target_link_libraries(booteable PRIVATE booteable_ui)
booteable_warnings(booteable)

# --- CLI headless ----------------------------------------------------------------
add_executable(booteable-image src/cli/main.cpp)
target_link_libraries(booteable-image PRIVATE booteable_core)
target_compile_definitions(booteable-image PRIVATE
    BOOTEABLE_VERSION="${PROJECT_VERSION}-wp4")
booteable_warnings(booteable-image)

# --- Pruebas ---------------------------------------------------------------------
enable_testing()

add_executable(booteable_tests
    tests/main.cpp
    tests/test_validation.cpp
    tests/test_fake_controller.cpp
    tests/test_core.cpp
    tests/test_layout.cpp
    tests/test_gpt.cpp
    tests/test_fat32.cpp
    tests/test_image_roundtrip.cpp
    tests/test_safety_policy.cpp
    tests/test_sysfs_source.cpp
    tests/test_mount_manager.cpp
    tests/test_device_writer.cpp
    tests/test_flash.cpp
    tests/test_prep_controller.cpp
    tests/test_ui_flow.cpp)
target_include_directories(booteable_tests PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/tests")
target_link_libraries(booteable_tests PRIVATE booteable_core booteable_ui)
booteable_warnings(booteable_tests)

add_test(NAME unit COMMAND booteable_tests)

# Cabecera PE32+, subsistema EFI y ausencia de dependencias de Windows.
add_test(NAME efi_binary
    COMMAND "${Python3_EXECUTABLE}"
            "${CMAKE_CURRENT_SOURCE_DIR}/tools/check-efi.py"
            "${EFI_BINARY}")

# La prueba virtual se salta con código 77 si faltan QEMU u OVMF.
add_test(NAME qemu_boot
    COMMAND bash "${CMAKE_CURRENT_SOURCE_DIR}/tools/run-qemu.sh" --headless)
set_tests_properties(qemu_boot PROPERTIES
    SKIP_RETURN_CODE 77
    TIMEOUT 90)

# Prueba negativa: con Secure Boot activo el PE sin firma no debe arrancar.
add_test(NAME qemu_secure_boot
    COMMAND bash "${CMAKE_CURRENT_SOURCE_DIR}/tools/run-qemu.sh"
            --headless --secure-boot)
set_tests_properties(qemu_secure_boot PROPERTIES
    SKIP_RETURN_CODE 77
    TIMEOUT 90)

# Prueba funcional de la CLI con imágenes de 512 y 4096.
add_test(NAME image_cli
    COMMAND bash "${CMAKE_CURRENT_SOURCE_DIR}/tools/test-image-cli.sh"
            "$<TARGET_FILE:booteable-image>")
set_tests_properties(image_cli PROPERTIES TIMEOUT 180)

# Prueba funcional de flash sobre un archivo regular, sin root ni hardware.
add_test(NAME flash_cli
    COMMAND bash "${CMAKE_CURRENT_SOURCE_DIR}/tools/test-cli-flash.sh"
            "$<TARGET_FILE:booteable-image>")
set_tests_properties(flash_cli PROPERTIES TIMEOUT 240)

# Arranque real desde la imagen GPT/FAT32 construida por el preparador.
add_test(NAME qemu_boot_image
    COMMAND bash "${CMAKE_CURRENT_SOURCE_DIR}/tools/test-qemu-image.sh"
            "$<TARGET_FILE:booteable-image>")
set_tests_properties(qemu_boot_image PROPERTIES
    SKIP_RETURN_CODE 77
    TIMEOUT 120)

# La misma prueba con sector lógico de 4096 y ESP de 260 MiB.
add_test(NAME qemu_boot_4kn
    COMMAND bash "${CMAKE_CURRENT_SOURCE_DIR}/tools/test-qemu-image.sh"
            "$<TARGET_FILE:booteable-image>"
            "${CMAKE_BINARY_DIR}/qemu-image-4kn.img" 4096)
set_tests_properties(qemu_boot_4kn PROPERTIES
    SKIP_RETURN_CODE 77
    TIMEOUT 150)
