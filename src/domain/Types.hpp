#ifndef BOOTEABLE_DOMAIN_TYPES_HPP
#define BOOTEABLE_DOMAIN_TYPES_HPP

#include <cstdint>
#include <string>
#include <vector>

namespace booteable {

// Tamaños y límites compartidos por el dominio, la imagen y la UI. Son
// constantes de formato: no dependen de Linux ni de UEFI.
constexpr std::uint32_t kMinLogicalSector = 512;
constexpr std::uint32_t kMaxLogicalSector = 4096;
constexpr std::uint64_t kMib = 1024ull * 1024ull;
constexpr std::uint64_t kMinEspBytes512 = 48ull * kMib;
constexpr std::uint64_t kMinEspBytes4Kn = 260ull * kMib;
constexpr std::uint64_t kDefaultEspBytes512 = 64ull * kMib;
constexpr std::uint64_t kDefaultEspBytes4Kn = 260ull * kMib;
constexpr std::uint64_t kMaxEspBytes = 2048ull * kMib;
constexpr std::uint32_t kMaxVolumeLabelLength = 11;

enum class FirmwareTarget { Uefi };
enum class PartitionScheme { Gpt };
enum class FileSystemKind { Fat32 };

enum class DeviceTransport { Usb, Sata, Nvme, Mmc, Virtual, Unknown };

enum class PrepStage {
    Idle,
    Enumerating,
    Validating,
    Confirming,
    Building,
    Unmounting,
    Opening,
    Writing,
    Syncing,
    Verifying,
    Done,
    Failed,
    Cancelled,
    Incomplete
};

enum class PrepOutcome {
    None,
    Verified,
    VerificationFailed,
    Cancelled,
    Incomplete,
    PermissionDenied,
    InvalidTarget,
    Disconnected,
    InsufficientSpace,
    BuildError,
    DeviceError,
    UnmountError
};

enum class SafetyVerdict { Allowed, AllowedWithWarning, Rejected };

struct DevicePartition {
    std::string name;          // sdb1
    std::string mount_point;   // vacío si no está montada
    std::uint64_t size_bytes = 0;
    std::string dev_id;        // major:minor de sysfs, para mountinfo
};

struct DeviceInfo {
    std::string path;          // /dev/sdb
    std::string by_id;         // /dev/disk/by-id/usb-...
    std::string model;
    std::string serial;
    std::uint64_t size_bytes = 0;
    std::uint32_t logical_sector = 512;
    bool removable = false;
    DeviceTransport transport = DeviceTransport::Unknown;
    bool read_only = false;
    std::vector<DevicePartition> partitions;
    std::string dev_id;                 // major:minor del disco
    std::vector<std::string> holders;   // dm/md/LVM que retienen el disco
};

struct SystemContext {
    std::string root_device;       // sda
    std::string boot_device;       // puede ser igual a root_device
    std::vector<std::string> mounted_device_names;
};

struct SafetyReport {
    SafetyVerdict verdict = SafetyVerdict::Rejected;
    std::vector<std::string> reasons;
    std::vector<std::string> warnings;
};

struct BootImageSpec {
    std::uint64_t disk_bytes = 0;                 // tamaño exacto del destino
    std::uint64_t esp_bytes = 0;                  // 0 = automático según sector
    std::string volume_label = "BOOTEABLE";
    std::uint32_t logical_sector = 512;
};

struct ImageLayout {
    std::uint32_t sector_size = 512;
    std::uint64_t total_sectors = 0;
    std::uint64_t esp_first_lba = 2048;
    std::uint64_t esp_sectors = 0;
    std::uint64_t backup_header_lba = 0;
    std::uint64_t backup_entries_lba = 0;
    std::uint64_t first_usable_lba = 0;
    std::uint64_t last_usable_lba = 0;
    std::uint32_t entry_sectors = 32;
    std::uint32_t fat_sectors = 0;
    std::uint32_t cluster_count = 0;
    std::string efi_path = "EFI/BOOT/BOOTX64.EFI";
};

struct ValidationIssue {
    std::string field;
    std::string message;
};

struct ValidationReport {
    std::vector<ValidationIssue> issues;
    bool ok() const { return issues.empty(); }
};

struct ImageRegion {
    std::uint64_t offset = 0;
    std::uint64_t length = 0;
    std::string sha256;         // hash del contenido de la región
};

struct ImageManifest {
    std::uint64_t disk_bytes = 0;
    std::vector<ImageRegion> regions;   // MBR, GPT, ESP, GPT de respaldo
    std::string manifest_sha256;        // hash de la lista canónica
};

struct PrepConfig {
    DeviceInfo device;
    std::string image_path;     // vacío = archivo temporal disperso
    std::uint64_t esp_bytes = 0;                    // 0 = automático
    std::string volume_label = "BOOTEABLE";         // configurable
    bool overwrite_confirmed = false;               // casilla + Enter
};

struct PrepProgress {
    PrepStage stage = PrepStage::Idle;
    std::uint64_t bytes_done = 0;
    std::uint64_t bytes_total = 0;
    int percent = 0;
    std::string detail;
};

struct PrepResult {
    PrepOutcome outcome = PrepOutcome::None;
    PrepStage failed_stage = PrepStage::Idle;
    std::string message;
    std::string image_sha256;   // hash del manifiesto de la imagen
    std::string device_sha256;  // hash del manifiesto releído del dispositivo
    std::uint64_t bytes_written = 0;
    bool synchronized = false;
    bool verified = false;
};

} // namespace booteable

#endif // BOOTEABLE_DOMAIN_TYPES_HPP
