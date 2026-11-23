#include "efi.h"

/*
    El binario se enlaza con -nostdlib, así que el compilador no puede llamar a
    la libc. Estas son las únicas rutinas que GCC puede emitir por su cuenta.
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
