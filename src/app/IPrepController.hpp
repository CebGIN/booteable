#ifndef BOOTEABLE_APP_IPREPCONTROLLER_HPP
#define BOOTEABLE_APP_IPREPCONTROLLER_HPP

#include <vector>

#include "domain/Types.hpp"

namespace booteable {

// Contrato que consume la UI. No enumera /sys, no abre /dev y no calcula
// geometrías: solo pide datos y dispara la preparación.
class IPrepController {
public:
    virtual ~IPrepController() = default;

    virtual std::vector<DeviceInfo> devices() = 0;
    virtual SafetyReport safety(const DeviceInfo& device) = 0;
    virtual ValidationReport validate(const PrepConfig& config) = 0;
    virtual bool start(const PrepConfig& config) = 0;
    virtual void cancel() = 0;
    virtual PrepProgress progress() const = 0;
    virtual PrepResult result() const = 0;
    virtual bool running() const = 0;
};

} // namespace booteable

#endif // BOOTEABLE_APP_IPREPCONTROLLER_HPP
