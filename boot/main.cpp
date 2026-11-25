#include "efi.h"

namespace {

constexpr size_t kBufferChars = 512;

CHAR16 g_text_buffer[kBufferChars];

void write_line(EFI_SIMPLE_TEXT_OUTPUT_PROTOCOL* console, const char* text) {
    if (console == nullptr || console->OutputString == nullptr) {
        return;
    }

    size_t index = 0;
    while (text[index] != '\0' && index + 2 < kBufferChars) {
        g_text_buffer[index] = static_cast<CHAR16>(
            static_cast<unsigned char>(text[index]));
        ++index;
    }
    g_text_buffer[index++] = '\r';
    g_text_buffer[index++] = '\n';
    g_text_buffer[index] = 0;

    console->OutputString(console, g_text_buffer);
}

} // namespace

/*
    Stub de WP0: demuestra la cadena de compilación, la incrustación y el
    arranque en QEMU/OVMF. WP1 reemplaza este mensaje por el definitivo, agrega
    la espera de tecla y el apagado.
*/
extern "C" EFI_STATUS EFIAPI efi_main(EFI_HANDLE image_handle,
                                      EFI_SYSTEM_TABLE* system_table) {
    (void)image_handle;

    if (system_table == nullptr || system_table->ConOut == nullptr) {
        return EFI_DEVICE_ERROR;
    }

    EFI_SIMPLE_TEXT_OUTPUT_PROTOCOL* console = system_table->ConOut;
    if (console->ClearScreen != nullptr) {
        console->ClearScreen(console);
    }

    write_line(console, "booteable WP0: hola");
    write_line(console, "UEFI x86_64 - programa propio - hola desde BOOTX64.EFI");
    write_line(console, "");
    write_line(console, "WP0-BOOT-OK");

    return EFI_SUCCESS;
}
