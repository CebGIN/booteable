#ifndef BOOTEABLE_DEVICE_DEVICEIDENTITY_HPP
#define BOOTEABLE_DEVICE_DEVICEIDENTITY_HPP

#include <filesystem>
#include <map>
#include <string>
#include <vector>

#include "domain/Types.hpp"

namespace booteable {

// Una línea de /proc/self/mountinfo. Solo se conservan los campos que usa el
// preparador: punto de montaje, major:minor y tipo de sistema de archivos.
struct MountEntry {
    std::string mount_point;
    std::string major_minor;
    std::string fs_type;
    std::string source;
};

// Lee y desescapa /proc/self/mountinfo. El punto de montaje puede contener
// secuencias octales como \040; el resto de campos se mantiene tal cual.
std::vector<MountEntry> read_mountinfo(const std::filesystem::path& path);

// Desescapa las secuencias octales que usa mountinfo (espacio, tab, salto y
// barra invertida).
std::string unescape_mountinfo_field(const std::string& field);

// Agrupa los puntos de montaje por major:minor. Se usa para asociar las
// particiones de sysfs con los montajes reales.
std::map<std::string, std::vector<std::string>> mount_points_by_device(
    const std::vector<MountEntry>& entries);

// Devuelve los montajes que pertenecen al disco o a una de sus particiones.
std::vector<MountEntry> mounts_for_device(
    const std::vector<MountEntry>& entries, const DeviceInfo& device);

// Busca el enlace /dev/disk/by-id/usb-* cuyo destino es el mismo nodo que
// /dev/<name>. Devuelve una cadena vacía si no existe una identidad estable.
std::string find_usb_by_id(const std::filesystem::path& disk_by_id_root,
                           const std::filesystem::path& dev_root,
                           const std::string& device_name);

// Resuelve el major:minor de mountinfo al nombre del disco que lo contiene
// (sda, nvme0n1, mmcblk0...). Para una partición devuelve el disco padre.
std::string disk_name_for_major_minor(const std::filesystem::path& sysfs_root,
                                      const std::string& major_minor);

// Nombre base de una ruta de dispositivo (/dev/sdb -> sdb).
std::string device_basename(const std::string& path);

} // namespace booteable

#endif // BOOTEABLE_DEVICE_DEVICEIDENTITY_HPP
