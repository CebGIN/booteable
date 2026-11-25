#include <iostream>
#include <string>
#include <vector>

#ifndef BOOTEABLE_VERSION
#define BOOTEABLE_VERSION "0.1.0-wp0"
#endif

namespace {

void print_usage() {
    std::cout <<
        "booteable-image " BOOTEABLE_VERSION "\n"
        "\n"
        "Uso:\n"
        "  booteable-image build  --output RUTA [--size-mb N] [--esp-mib N]\n"
        "                         [--label ETIQUETA] [--sector-size 512|4096]\n"
        "  booteable-image verify --image RUTA\n"
        "  booteable-image flash  --device RUTA --image RUTA [--yes]\n"
        "  booteable-image --help | --version\n"
        "\n"
        "WP0 solo deja el esqueleto de la CLI: build, verify y flash se\n"
        "implementan en WP2 y WP3b.\n";
}

} // namespace

int main(int argc, char** argv) {
    const std::vector<std::string> args(argv + 1, argv + argc);

    if (args.empty() || args[0] == "--help" || args[0] == "-h") {
        print_usage();
        return args.empty() ? 2 : 0;
    }

    if (args[0] == "--version") {
        std::cout << "booteable-image " BOOTEABLE_VERSION "\n";
        return 0;
    }

    if (args[0] == "build" || args[0] == "verify" || args[0] == "flash") {
        std::cerr << "El comando '" << args[0]
                  << "' todavía no está implementado (WP0).\n";
        return 2;
    }

    std::cerr << "Comando desconocido: " << args[0] << "\n";
    print_usage();
    return 2;
}
