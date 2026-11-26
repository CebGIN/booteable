#include "efi.h"

/*
    El binario se enlaza con -nostdlib, así que el compilador no puede llamar a
    la libc. Estas son las únicas rutinas que GCC puede emitir por su cuenta,
    más las ayudas de consola y puerto serie que usa la aplicación.
*/

extern "C" void* memcpy(void* destination, const void* source, size_t count) {
    auto* out = static_cast<unsigned char*>(destination);
    const auto* in = static_cast<const unsigned char*>(source);
    for (size_t index = 0; index < count; ++index) {
        out[index] = in[index];
    }
    return destination;
}

extern "C" void* memmove(void* destination, const void* source, size_t count) {
    auto* out = static_cast<unsigned char*>(destination);
    const auto* in = static_cast<const unsigned char*>(source);
    if (out < in) {
        for (size_t index = 0; index < count; ++index) {
            out[index] = in[index];
        }
    } else {
        for (size_t index = count; index > 0; --index) {
            out[index - 1] = in[index - 1];
        }
    }
    return destination;
}

extern "C" void* memset(void* destination, int value, size_t count) {
    auto* out = static_cast<unsigned char*>(destination);
    for (size_t index = 0; index < count; ++index) {
        out[index] = static_cast<unsigned char>(value);
    }
    return destination;
}

extern "C" int memcmp(const void* left, const void* right, size_t count) {
    const auto* a = static_cast<const unsigned char*>(left);
    const auto* b = static_cast<const unsigned char*>(right);
    for (size_t index = 0; index < count; ++index) {
        if (a[index] != b[index]) {
            return a[index] < b[index] ? -1 : 1;
        }
    }
    return 0;
}

namespace {

constexpr CHAR16 kReplacementCharacter = 0xFFFDu;

size_t text_length(const char* text) {
    size_t length = 0;
    while (text[length] != '\0') {
        ++length;
    }
    return length;
}

// Decodifica un carácter UTF-8. Devuelve la cantidad de bytes consumidos y
// escribe uno o dos CHAR16 (para caracteres fuera del plano básico).
size_t decode_utf8(const char* text, size_t remaining, CHAR16* first,
                   CHAR16* second, bool* has_second) {
    const auto byte = [text](size_t index) {
        return static_cast<unsigned char>(text[index]);
    };

    if (remaining == 0) {
        *first = 0;
        *has_second = false;
        return 0;
    }

    const unsigned char lead = byte(0);
    if (lead < 0x80) {
        *first = static_cast<CHAR16>(lead);
        *has_second = false;
        return 1;
    }

    size_t needed = 0;
    unsigned int code_point = 0;
    if ((lead & 0xE0u) == 0xC0u) {
        needed = 1;
        code_point = lead & 0x1Fu;
    } else if ((lead & 0xF0u) == 0xE0u) {
        needed = 2;
        code_point = lead & 0x0Fu;
    } else if ((lead & 0xF8u) == 0xF0u) {
        needed = 3;
        code_point = lead & 0x07u;
    } else {
        *first = kReplacementCharacter;
        *has_second = false;
        return 1;
    }

    if (remaining < needed + 1) {
        *first = kReplacementCharacter;
        *has_second = false;
        return 1;
    }

    for (size_t index = 1; index <= needed; ++index) {
        const unsigned char continuation = byte(index);
        if ((continuation & 0xC0u) != 0x80u) {
            *first = kReplacementCharacter;
            *has_second = false;
            return 1;
        }
        code_point = (code_point << 6) | (continuation & 0x3Fu);
    }

    if (code_point <= 0xFFFFu) {
        *first = static_cast<CHAR16>(code_point);
        *has_second = false;
    } else {
        const unsigned int adjusted = code_point - 0x10000u;
        *first = static_cast<CHAR16>(0xD800u + (adjusted >> 10));
        *second = static_cast<CHAR16>(0xDC00u + (adjusted & 0x3FFu));
        *has_second = true;
    }
    return needed + 1;
}

} // namespace

bool efi_utf8_to_utf16(const char* text, CHAR16* output, size_t capacity,
                       size_t* written) {
    if (text == nullptr || output == nullptr || capacity == 0) {
        return false;
    }

    const size_t length = text_length(text);
    size_t source = 0;
    size_t target = 0;
    bool fits = true;

    while (source < length) {
        CHAR16 first = 0;
        CHAR16 second = 0;
        bool has_second = false;
        const size_t consumed =
            decode_utf8(text + source, length - source, &first, &second,
                        &has_second);
        if (consumed == 0) {
            break;
        }
        source += consumed;

        if (target + (has_second ? 2u : 1u) + 1u > capacity) {
            fits = false;
            break;
        }
        output[target++] = first;
        if (has_second) {
            output[target++] = second;
        }
    }

    output[target] = 0;
    if (written != nullptr) {
        *written = target;
    }
    return fits && source == length;
}

void efi_console_text(EFI_SIMPLE_TEXT_OUTPUT_PROTOCOL* console,
                      const char* text) {
    if (console == nullptr || console->OutputString == nullptr ||
        text == nullptr) {
        return;
    }

    CHAR16 buffer[512];
    if (!efi_utf8_to_utf16(text, buffer, sizeof(buffer) / sizeof(buffer[0]),
                           nullptr)) {
        // El texto se truncó, pero se muestra lo que entró.
    }
    console->OutputString(console, buffer);
}

void efi_console_line(EFI_SIMPLE_TEXT_OUTPUT_PROTOCOL* console,
                      const char* text) {
    efi_console_text(console, text);
    efi_console_text(console, "\r\n");
}

void efi_serial_write(EFI_SERIAL_IO_PROTOCOL* serial, const char* text,
                      size_t length) {
    if (serial == nullptr || serial->Write == nullptr || text == nullptr) {
        return;
    }

    size_t written = 0;
    while (written < length) {
        UINTN pending = static_cast<UINTN>(length - written);
        const EFI_STATUS status = serial->Write(
            serial, &pending,
            const_cast<char*>(text + written));
        if (status != EFI_SUCCESS || pending == 0) {
            return;
        }
        written += static_cast<size_t>(pending);
    }
}

void efi_serial_line(EFI_SERIAL_IO_PROTOCOL* serial, const char* text) {
    if (serial == nullptr || text == nullptr) {
        return;
    }
    const size_t length = text_length(text);
    efi_serial_write(serial, text, length);
    efi_serial_write(serial, "\r\n", 2);
}
