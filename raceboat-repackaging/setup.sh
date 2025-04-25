#!/bin/bash
set -e

echo "[+] Running setup.sh to prepare environment..."
ARCH=$(uname -m)
OS=$(uname -s)

if [ "$OS" != "Linux" ]; then
  echo "[-] This setup script currently supports Linux only."
  exit 1
fi

# Detect distro
if [ -f /etc/os-release ]; then
  . /etc/os-release
  DISTRO=$ID
  VERSION=$VERSION_ID
else
  echo "[-] Cannot detect Linux distribution."
  exit 1
fi

if [[ "$DISTRO" == "ubuntu" || "$DISTRO" == "debian" ]]; then
  echo "[+] Detected Ubuntu/Debian ($DISTRO $VERSION)"
  apt-get update
  apt-get install -y software-properties-common xdg-utils

  if ! command -v python3.12 >/dev/null; then
    echo "[+] Adding deadsnakes PPA for Python 3.12..."
    add-apt-repository -y ppa:deadsnakes/ppa
    apt-get update
    apt-get install -y python3.12 python3.12-dev
  else
    echo "[=] Python 3.12 already installed."
  fi

  echo "[+] Installing unzip, pip, and other tools..."
  apt-get install -y unzip python3-pip file strace bash libc-bin
else
  echo "[-] Unsupported distro: $DISTRO. Please manually install Python 3.12 and dependencies."
  exit 1
fi

# Copy the .desktop file to the user's applications directory
DESKTOP_DIR="$HOME/.local/share/applications"
DESKTOP_FILE="$DESKTOP_DIR/Raceboat.desktop"

# Create the directory if it doesn't exist
mkdir -p "$DESKTOP_DIR"

if [ ! -f "$DESKTOP_FILE" ]; then
  echo "[+] Installing desktop entry..."
  cp "$(dirname "$0")/Raceboat.desktop" "$DESKTOP_FILE"
  echo "[+] Desktop entry installed. You can find Raceboat in your application menu."
else
  echo "[=] Desktop entry already exists. Skipping installation."
fi

echo "[+] Setup complete. You can now run:"
echo "    ./start.sh"