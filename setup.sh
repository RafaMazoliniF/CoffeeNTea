#!/bin/bash

set -e

echo "Detecting distribution..."

if [ -f /etc/os-release ]; then
    . /etc/os-release
    DISTRO=$ID
else
    echo "Could not detect the distribution."
    exit 1
fi

KERNEL_VERSION=$(uname -r)

echo "Current kernel: $KERNEL_VERSION"
echo "Detected distribution: $DISTRO"

install_ubuntu_deps() {
    echo "Updating package lists..."
    sudo apt update

    echo "Installing build-essential, kmod, and kernel headers..."
    sudo apt install -y build-essential kmod linux-headers-$(uname -r)
}

install_arch_deps() {
    echo "Updating system packages (this may take a while)..."
    sudo pacman -Syu --noconfirm

    echo "Installing base-devel, kmod, and kernel headers..."
    sudo pacman -S --noconfirm base-devel kmod linux-headers
}

install_fedora_deps() {
    echo "Upgrading system packages..."
    sudo dnf upgrade -y

    echo "Installing development tools, kmod, and kernel headers..."
    sudo dnf groupinstall -y "Development Tools"
    sudo dnf install -y kmod kernel-headers kernel-devel
}

case "$DISTRO" in
    ubuntu|debian)
        install_ubuntu_deps
        ;;
    arch)
        install_arch_deps
        ;;
    fedora)
        install_fedora_deps
        ;;
    *)
        echo "Distribution '$DISTRO' is not supported by this script."
        exit 2
        ;;
esac

echo "✅ Installation completed successfully!"
