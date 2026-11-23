#ifndef BOOTEABLE_BOOT_EFI_H
#define BOOTEABLE_BOOT_EFI_H

/*
    Tipos y protocolos UEFI mínimos para el programa de arranque. No se usa
    EDK2 ni gnu-efi: solo lo que el binario necesita para hablar con el
    firmware. La app es x86_64 y usa la convención de llamada de Microsoft.
*/

#include <stddef.h>
#include <stdint.h>

#define EFIAPI __attribute__((ms_abi))

#define EFI_SUCCESS 0ull
#define EFI_LOAD_ERROR 1ull
#define EFI_INVALID_PARAMETER 2ull
#define EFI_UNSUPPORTED 3ull
#define EFI_DEVICE_ERROR 7ull

typedef uint64_t EFI_STATUS;
typedef void* EFI_HANDLE;
typedef uint16_t CHAR16;
typedef uint8_t BOOLEAN;
typedef uint64_t UINTN;
typedef int64_t INTN;
typedef uint32_t UINT32;
typedef uint64_t UINT64;

typedef struct {
    uint64_t Signature;
    uint32_t Revision;
    uint32_t HeaderSize;
    uint32_t CRC32;
    uint32_t Reserved;
} EFI_TABLE_HEADER;

typedef struct {
    int32_t MaxMode;
    int32_t Mode;
    int32_t Attribute;
    int32_t CursorColumn;
    int32_t CursorRow;
    BOOLEAN CursorVisible;
} EFI_SIMPLE_TEXT_OUTPUT_MODE;

typedef struct EFI_SIMPLE_TEXT_OUTPUT_PROTOCOL EFI_SIMPLE_TEXT_OUTPUT_PROTOCOL;

typedef EFI_STATUS(EFIAPI* EFI_TEXT_RESET)(
    EFI_SIMPLE_TEXT_OUTPUT_PROTOCOL* self, BOOLEAN extended_verification);
typedef EFI_STATUS(EFIAPI* EFI_TEXT_STRING)(
    EFI_SIMPLE_TEXT_OUTPUT_PROTOCOL* self, CHAR16* string);
typedef EFI_STATUS(EFIAPI* EFI_TEXT_TEST_STRING)(
    EFI_SIMPLE_TEXT_OUTPUT_PROTOCOL* self, CHAR16* string);
typedef EFI_STATUS(EFIAPI* EFI_TEXT_QUERY_MODE)(
    EFI_SIMPLE_TEXT_OUTPUT_PROTOCOL* self, UINTN mode_number,
    UINTN* columns, UINTN* rows);
typedef EFI_STATUS(EFIAPI* EFI_TEXT_SET_MODE)(
    EFI_SIMPLE_TEXT_OUTPUT_PROTOCOL* self, UINTN mode_number);
typedef EFI_STATUS(EFIAPI* EFI_TEXT_SET_ATTRIBUTE)(
    EFI_SIMPLE_TEXT_OUTPUT_PROTOCOL* self, UINTN attribute);
typedef EFI_STATUS(EFIAPI* EFI_TEXT_CLEAR_SCREEN)(
    EFI_SIMPLE_TEXT_OUTPUT_PROTOCOL* self);
typedef EFI_STATUS(EFIAPI* EFI_TEXT_SET_CURSOR_POSITION)(
    EFI_SIMPLE_TEXT_OUTPUT_PROTOCOL* self, UINTN column, UINTN row);
typedef EFI_STATUS(EFIAPI* EFI_TEXT_ENABLE_CURSOR)(
    EFI_SIMPLE_TEXT_OUTPUT_PROTOCOL* self, BOOLEAN visible);

struct EFI_SIMPLE_TEXT_OUTPUT_PROTOCOL {
    EFI_TEXT_RESET Reset;
    EFI_TEXT_STRING OutputString;
    EFI_TEXT_TEST_STRING TestString;
    EFI_TEXT_QUERY_MODE QueryMode;
    EFI_TEXT_SET_MODE SetMode;
    EFI_TEXT_SET_ATTRIBUTE SetAttribute;
    EFI_TEXT_CLEAR_SCREEN ClearScreen;
    EFI_TEXT_SET_CURSOR_POSITION SetCursorPosition;
    EFI_TEXT_ENABLE_CURSOR EnableCursor;
    EFI_SIMPLE_TEXT_OUTPUT_MODE* Mode;
};

typedef struct {
    uint16_t ScanCode;
    CHAR16 UnicodeChar;
} EFI_INPUT_KEY;

typedef struct EFI_SIMPLE_TEXT_INPUT_PROTOCOL EFI_SIMPLE_TEXT_INPUT_PROTOCOL;

typedef EFI_STATUS(EFIAPI* EFI_INPUT_RESET)(
    EFI_SIMPLE_TEXT_INPUT_PROTOCOL* self, BOOLEAN extended_verification);
typedef EFI_STATUS(EFIAPI* EFI_INPUT_READ_KEY)(
    EFI_SIMPLE_TEXT_INPUT_PROTOCOL* self, EFI_INPUT_KEY* key);

struct EFI_SIMPLE_TEXT_INPUT_PROTOCOL {
    EFI_INPUT_RESET Reset;
    EFI_INPUT_READ_KEY ReadKeyStroke;
    EFI_HANDLE WaitForKey;
};

typedef struct {
    EFI_TABLE_HEADER Hdr;
    CHAR16* FirmwareVendor;
    uint32_t FirmwareRevision;
    EFI_HANDLE ConsoleInHandle;
    EFI_SIMPLE_TEXT_INPUT_PROTOCOL* ConIn;
    EFI_HANDLE ConsoleOutHandle;
    EFI_SIMPLE_TEXT_OUTPUT_PROTOCOL* ConOut;
    EFI_HANDLE StandardErrorHandle;
    EFI_SIMPLE_TEXT_OUTPUT_PROTOCOL* StdErr;
    void* RuntimeServices;
    void* BootServices;
    UINTN NumberOfTableEntries;
    void* ConfigurationTable;
} EFI_SYSTEM_TABLE;

#endif // BOOTEABLE_BOOT_EFI_H
