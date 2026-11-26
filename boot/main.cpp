#include "efi.h"

#ifndef BOOTEABLE_BUILD_ID
#define BOOTEABLE_BUILD_ID "dev"
#endif

namespace {

/*
    GUID de EFI_SERIAL_IO_PROTOCOL. Se define en .rodata: no hay inicialización
    dinámica porque el binario no enlaza la libc.
*/
EFI_GUID g_serial_guid = EFI_SERIAL_IO_PROTOCOL_GUID;

constexpr UINTN kConsoleAttribute = EFI_LIGHTGRAY | EFI_BACKGROUND_BLUE;
constexpr UINTN kNormalAttribute = EFI_LIGHTGRAY | EFI_BACKGROUND_BLACK;
constexpr int kWaitSeconds = 10;

const char* const kMessage[] = {
    "Universidad de la República",
    "Facultad de Ingeniería — Sistemas Operativos",
    "",
    "        U S B   B O O T E A B L E",
    "        ARRANQUE UEFI PROPIO",
    "",
    "Grupo: booteable",
    "Programa: BOOTX64.EFI",
    "Build: " BOOTEABLE_BUILD_ID,
    "",
    "BOOTEABLE-BOOT-OK",
};

void show_message(EFI_SYSTEM_TABLE* system_table,
                  EFI_SERIAL_IO_PROTOCOL* serial) {
    EFI_SIMPLE_TEXT_OUTPUT_PROTOCOL* console = system_table->ConOut;

    if (console != nullptr) {
        if (console->SetAttribute != nullptr) {
            console->SetAttribute(console, kConsoleAttribute);
        }
        if (console->ClearScreen != nullptr) {
            console->ClearScreen(console);
        }
    }

    for (const char* line : kMessage) {
        efi_console_line(console, line);
        efi_serial_line(serial, line);
    }

    efi_console_line(console, "");
    efi_console_line(console,
                     "Pulsá una tecla para apagar; si no, se apaga en 10 s.");
    efi_serial_line(serial, "Esperando tecla o apagado automático (10 s).");

    if (console != nullptr && console->SetAttribute != nullptr) {
        console->SetAttribute(console, kNormalAttribute);
    }
}

// Devuelve true si hay una tecla disponible. No bloquea.
bool key_available(EFI_SYSTEM_TABLE* system_table) {
    EFI_SIMPLE_TEXT_INPUT_PROTOCOL* input = system_table->ConIn;
    if (input == nullptr || input->ReadKeyStroke == nullptr) {
        return false;
    }

    EFI_INPUT_KEY key;
    return input->ReadKeyStroke(input, &key) == EFI_SUCCESS;
}

// Espera una tecla durante kWaitSeconds segundos. El sondeo con Stall no usa
// reloj ni temporizadores del firmware y funciona igual en consola y en serie.
void wait_for_key(EFI_SYSTEM_TABLE* system_table) {
    EFI_BOOT_SERVICES* boot_services = system_table->BootServices;
    if (boot_services == nullptr || boot_services->Stall == nullptr) {
        return;
    }

    for (int tick = 0; tick < kWaitSeconds * 10; ++tick) {
        if (key_available(system_table)) {
            return;
        }
        boot_services->Stall(100000); // 100 ms
    }
}

void shutdown(EFI_SYSTEM_TABLE* system_table,
              EFI_SERIAL_IO_PROTOCOL* serial) {
    efi_console_line(system_table->ConOut, "Apagando el equipo.");
    efi_serial_line(serial, "Apagando el equipo.");

    EFI_RUNTIME_SERVICES* runtime = system_table->RuntimeServices;
    if (runtime != nullptr && runtime->ResetSystem != nullptr) {
        runtime->ResetSystem(EfiResetShutdown, EFI_SUCCESS, 0, nullptr);
    }

    // Si el firmware no implementa ResetSystem, se informa y se devuelve éxito:
    // el programa no debe dejar el equipo en un bucle.
    efi_console_line(system_table->ConOut,
                     "El firmware no implementó ResetSystem.");
    efi_serial_line(serial, "El firmware no implementó ResetSystem.");
}

} // namespace

/*
    Punto de entrada de la aplicación EFI. El firmware la carga desde
    \EFI\BOOT\BOOTX64.EFI en una ESP FAT32. No abre protocolos de disco, no
    escribe archivos, no lee variables y no modifica el hardware.
*/
extern "C" EFI_STATUS EFIAPI efi_main(EFI_HANDLE image_handle,
                                      EFI_SYSTEM_TABLE* system_table) {
    (void)image_handle;

    if (system_table == nullptr || system_table->ConOut == nullptr) {
        return EFI_DEVICE_ERROR;
    }

    EFI_SERIAL_IO_PROTOCOL* serial = nullptr;
    EFI_BOOT_SERVICES* boot_services = system_table->BootServices;
    if (boot_services != nullptr && boot_services->LocateProtocol != nullptr) {
        void* protocol = nullptr;
        const EFI_STATUS status = boot_services->LocateProtocol(
            &g_serial_guid, nullptr, &protocol);
        if (status == EFI_SUCCESS) {
            serial = static_cast<EFI_SERIAL_IO_PROTOCOL*>(protocol);
        }
    }

    if (serial != nullptr && serial->SetAttributes != nullptr) {
        // 115200 8N1, sin tiempo límite. Si falla, se escribe igual: la consola
        // del firmware sigue siendo la salida principal.
        serial->SetAttributes(serial, 115200, 16, 0, 0, 8, 1);
    }

    show_message(system_table, serial);
    wait_for_key(system_table);
    shutdown(system_table, serial);

    return EFI_SUCCESS;
}
