#ifndef BOOTEABLE_BOOT_EFI_H
#define BOOTEABLE_BOOT_EFI_H

/*
    Tipos y protocolos UEFI mínimos para el programa de arranque. No se usa
    EDK2 ni gnu-efi: solo lo que el binario necesita para hablar con el
    firmware. La app es x86_64 y usa la convención de llamada de Microsoft.

    Las estructuras de tablas replican el orden y el tamaño de la especificación
    UEFI: un campo de más o de menos desplazaría todos los punteros siguientes.
    Los miembros que no se usan quedan como `void*` para no arrastrar tipos.
*/

#include <stddef.h>
#include <stdint.h>

#define EFIAPI __attribute__((ms_abi))

#define EFI_SUCCESS 0ull
#define EFI_LOAD_ERROR 1ull
#define EFI_INVALID_PARAMETER 2ull
#define EFI_UNSUPPORTED 3ull
#define EFI_BAD_BUFFER_SIZE 4ull
#define EFI_BUFFER_TOO_SMALL 5ull
#define EFI_DEVICE_ERROR 7ull
#define EFI_WRITE_PROTECTED 8ull
#define EFI_NOT_FOUND 14ull
#define EFI_TIMEOUT 18ull

typedef uint64_t EFI_STATUS;
typedef void* EFI_HANDLE;
typedef void* EFI_EVENT;
typedef uint16_t CHAR16;
typedef uint8_t BOOLEAN;
typedef uint64_t UINTN;
typedef int64_t INTN;
typedef uint32_t UINT32;
typedef uint64_t UINT64;
typedef uint8_t UINT8;

typedef struct {
    uint32_t Data1;
    uint16_t Data2;
    uint16_t Data3;
    uint8_t Data4[8];
} EFI_GUID;

typedef struct {
    uint64_t Signature;
    uint32_t Revision;
    uint32_t HeaderSize;
    uint32_t CRC32;
    uint32_t Reserved;
} EFI_TABLE_HEADER;

/* --- Consola de texto ---------------------------------------------------- */

/* Atributos de EFI_SIMPLE_TEXT_OUTPUT_PROTOCOL (fondo en los bits altos). */
#define EFI_BLACK 0x00u
#define EFI_BLUE 0x01u
#define EFI_GREEN 0x02u
#define EFI_CYAN 0x03u
#define EFI_RED 0x04u
#define EFI_MAGENTA 0x05u
#define EFI_BROWN 0x06u
#define EFI_LIGHTGRAY 0x07u
#define EFI_DARKGRAY 0x08u
#define EFI_LIGHTBLUE 0x09u
#define EFI_LIGHTGREEN 0x0Au
#define EFI_LIGHTCYAN 0x0Bu
#define EFI_LIGHTRED 0x0Cu
#define EFI_LIGHTMAGENTA 0x0Du
#define EFI_YELLOW 0x0Eu
#define EFI_WHITE 0x0Fu
#define EFI_BACKGROUND_BLUE 0x10u
#define EFI_BACKGROUND_BLACK 0x00u

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
    EFI_EVENT WaitForKey;
};

/* --- Puerto serie -------------------------------------------------------- */

typedef struct EFI_SERIAL_IO_MODE EFI_SERIAL_IO_MODE;
typedef struct EFI_SERIAL_IO_PROTOCOL EFI_SERIAL_IO_PROTOCOL;

typedef EFI_STATUS(EFIAPI* EFI_SERIAL_RESET)(
    EFI_SERIAL_IO_PROTOCOL* self);
typedef EFI_STATUS(EFIAPI* EFI_SERIAL_SET_ATTRIBUTES)(
    EFI_SERIAL_IO_PROTOCOL* self, uint64_t baud_rate, uint32_t receive_fifo_depth,
    uint32_t timeout, uint32_t parity, uint8_t data_bits, uint8_t stop_bits);
typedef EFI_STATUS(EFIAPI* EFI_SERIAL_CLEAR_CONTROL)(
    EFI_SERIAL_IO_PROTOCOL* self, uint32_t control);
typedef EFI_STATUS(EFIAPI* EFI_SERIAL_SET_CONTROL)(
    EFI_SERIAL_IO_PROTOCOL* self, uint32_t control);
typedef EFI_STATUS(EFIAPI* EFI_SERIAL_GET_CONTROL)(
    EFI_SERIAL_IO_PROTOCOL* self, uint32_t* control);
typedef EFI_STATUS(EFIAPI* EFI_SERIAL_WRITE)(
    EFI_SERIAL_IO_PROTOCOL* self, UINTN* buffer_size, void* buffer);
typedef EFI_STATUS(EFIAPI* EFI_SERIAL_READ)(
    EFI_SERIAL_IO_PROTOCOL* self, UINTN* buffer_size, void* buffer);

struct EFI_SERIAL_IO_PROTOCOL {
    uint32_t Revision;
    EFI_SERIAL_RESET Reset;
    EFI_SERIAL_SET_ATTRIBUTES SetAttributes;
    EFI_SERIAL_CLEAR_CONTROL ClearControl;
    EFI_SERIAL_SET_CONTROL SetControl;
    EFI_SERIAL_GET_CONTROL GetControl;
    EFI_SERIAL_WRITE Write;
    EFI_SERIAL_READ Read;
    EFI_SERIAL_IO_MODE* Mode;
};

/* --- Servicios de arranque y de ejecución -------------------------------- */

typedef EFI_STATUS(EFIAPI* EFI_WAIT_FOR_EVENT)(
    UINTN number_of_events, EFI_EVENT* event, UINTN* index);
typedef EFI_STATUS(EFIAPI* EFI_STALL)(UINTN microseconds);
typedef EFI_STATUS(EFIAPI* EFI_LOCATE_PROTOCOL)(
    EFI_GUID* protocol, void* registration, void** interface);
typedef void(EFIAPI* EFI_COPY_MEM)(void* destination, const void* source,
                                   UINTN length);
typedef void(EFIAPI* EFI_SET_MEM)(void* buffer, UINTN size, uint8_t value);

typedef struct {
    EFI_TABLE_HEADER Hdr;

    void* RaiseTPL;
    void* RestoreTPL;
    void* AllocatePages;
    void* FreePages;
    void* GetMemoryMap;
    void* AllocatePool;
    void* FreePool;
    void* CreateEvent;
    void* SetTimer;
    EFI_WAIT_FOR_EVENT WaitForEvent;
    void* SignalEvent;
    void* CloseEvent;
    void* CheckEvent;
    void* InstallProtocolInterface;
    void* ReinstallProtocolInterface;
    void* UninstallProtocolInterface;
    void* HandleProtocol;
    void* Reserved;
    void* RegisterProtocolNotify;
    void* LocateHandle;
    void* LocateDevicePath;
    void* InstallConfigurationTable;
    void* LoadImage;
    void* StartImage;
    void* Exit;
    void* UnloadImage;
    void* ExitBootServices;
    void* GetNextMonotonicCount;
    EFI_STALL Stall;
    void* SetWatchdogTimer;
    void* ConnectController;
    void* DisconnectController;
    void* OpenProtocol;
    void* CloseProtocol;
    void* OpenProtocolInformation;
    void* ProtocolsPerHandle;
    void* LocateHandleBuffer;
    EFI_LOCATE_PROTOCOL LocateProtocol;
    void* InstallMultipleProtocolInterfaces;
    void* UninstallMultipleProtocolInterfaces;
    void* CalculateCrc32;
    EFI_COPY_MEM CopyMem;
    EFI_SET_MEM SetMem;
} EFI_BOOT_SERVICES;

typedef enum {
    EfiResetCold = 0,
    EfiResetWarm = 1,
    EfiResetShutdown = 2,
    EfiResetPlatformSpecific = 3
} EFI_RESET_TYPE;

typedef EFI_STATUS(EFIAPI* EFI_RESET_SYSTEM)(
    EFI_RESET_TYPE reset_type, EFI_STATUS reset_status, UINTN data_size,
    void* reset_data);

typedef struct {
    EFI_TABLE_HEADER Hdr;

    void* GetTime;
    void* SetTime;
    void* GetWakeupTime;
    void* SetWakeupTime;
    void* SetVirtualAddressMap;
    void* ConvertPointer;
    void* GetVariable;
    void* GetNextVariableName;
    void* SetVariable;
    void* GetNextHighMonotonicCount;
    EFI_RESET_SYSTEM ResetSystem;
    void* UpdateCapsule;
    void* QueryCapsuleCapabilities;
    void* QueryVariableInfo;
} EFI_RUNTIME_SERVICES;

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
    EFI_RUNTIME_SERVICES* RuntimeServices;
    EFI_BOOT_SERVICES* BootServices;
    UINTN NumberOfTableEntries;
    void* ConfigurationTable;
} EFI_SYSTEM_TABLE;

/* GUID de EFI_SERIAL_IO_PROTOCOL: BB25CF6F-F1D4-11D2-9A0C-0090273FC1FD. */
#define EFI_SERIAL_IO_PROTOCOL_GUID                                          \
    {                                                                        \
        0xBB25CF6F, 0xF1D4, 0x11D2,                                          \
        { 0x9A, 0x0C, 0x00, 0x90, 0x27, 0x3F, 0xC1, 0xFD }                   \
    }

/* --- Ayudas mínimas (boot/efi.cpp) --------------------------------------- */

/*
    El binario se enlaza con -nostdlib. Estas rutinas son las que el compilador
    puede emitir por su cuenta y las que usa la aplicación para consola y serie.
*/

// Convierte UTF-8 a UTF-16LE. Devuelve false si el texto no entra.
bool efi_utf8_to_utf16(const char* text, CHAR16* output, size_t capacity,
                       size_t* written);

// Escribe una cadena UTF-8 en la consola de texto y agrega CR LF.
void efi_console_line(EFI_SIMPLE_TEXT_OUTPUT_PROTOCOL* console,
                      const char* text);

// Escribe una cadena UTF-8 en la consola de texto sin agregar CR LF.
void efi_console_text(EFI_SIMPLE_TEXT_OUTPUT_PROTOCOL* console,
                      const char* text);

// Escribe una cadena UTF-8 y CR LF en el puerto serie, si existe.
void efi_serial_line(EFI_SERIAL_IO_PROTOCOL* serial, const char* text);

// Escribe bytes crudos en el puerto serie respetando escrituras cortas.
void efi_serial_write(EFI_SERIAL_IO_PROTOCOL* serial, const char* text,
                      size_t length);

#endif // BOOTEABLE_BOOT_EFI_H
