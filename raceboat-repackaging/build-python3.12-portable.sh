#!/bin/bash
set -e

# Define Python version
PYTHON_VERSION="3.12.3"
PYTHON_SRC_DIR="Python-$PYTHON_VERSION"
INSTALL_DIR="$PWD/AppDir/usr/python"
ARCH=$(uname -m)
OS=$(uname -s)

echo "[+] Starting portable Python $PYTHON_VERSION build..."

# Ensure we're on a supported platform
if [ "$OS" != "Linux" ]; then
  echo "[-] This script currently supports Linux only."
  exit 1
fi

# If Python already exists, skip rebuild
if [ -x "$INSTALL_DIR/bin/python3.12" ]; then
  echo "[=] Python $PYTHON_VERSION already built at $INSTALL_DIR"
  exit 0
fi

# Install build dependencies
echo "[+] Installing build dependencies..."
apt-get update
apt-get install -y \
  build-essential \
  file \
  zlib1g-dev \
  libncurses5-dev \
  libgdbm-dev \
  libssl-dev \
  libsqlite3-dev \
  tk-dev \
  libreadline-dev \
  libbz2-dev \
  liblzma-dev \
  libffi-dev \
  xz-utils \
  wget \
  curl \
  gnupg2 \
  software-properties-common \
  xdg-utils \
  patchelf

apt-get update

mkdir -p "$INSTALL_DIR"

# Download and extract Python source
echo "[+] Downloading Python $PYTHON_VERSION source..."
wget -q https://www.python.org/ftp/python/${PYTHON_VERSION}/${PYTHON_SRC_DIR}.tgz
tar -xf ${PYTHON_SRC_DIR}.tgz
cd ${PYTHON_SRC_DIR}

# Configure and build
echo "[+] Configuring Python build..."
./configure --prefix=${INSTALL_DIR} --enable-shared --with-ensurepip=install

echo "[+] Building Python..."
make -j$(nproc)

echo "[+] Installing Python to $INSTALL_DIR..."
make altinstall

cd ..

# Clean up source files
rm -rf ${PYTHON_SRC_DIR} ${PYTHON_SRC_DIR}.tgz

# Ensure shared library is in the right place
PYTHON_LIB="$INSTALL_DIR/lib/libpython3.12.so.1.0"
if [ ! -f "$PYTHON_LIB" ]; then
  echo "[-] ERROR: Python shared library not found at $PYTHON_LIB"
  exit 1
fi

# Ensure shared library is in the right place
PY_SO_SRC="$INSTALL_DIR/lib/libpython3.12.so.1.0"
PY_SO_DEST="$PWD/AppDir/usr/lib/libpython3.12.so.1.0"

# Ensure lib dir exists
mkdir -p "$(dirname "$PY_SO_DEST")"

# Replace if not a symlink or symlink is broken
if [ -e "$PY_SO_DEST" ] && [ ! -L "$PY_SO_DEST" ]; then
  echo "[~] Replacing existing non-symlinked $PY_SO_DEST"
  rm -f "$PY_SO_DEST"
fi

# Create symlink (do not copy)
ln -sf "$PY_SO_SRC" "$PY_SO_DEST"

# Patch RPATH on binaries inside python/bin
echo "[+] Patching RPATH on binaries in $INSTALL_DIR/bin..."
for bin in "$INSTALL_DIR/bin/"*; do
  if command -v file >/dev/null && file "$bin" | grep -q "ELF.*executable"; then
    echo "  -> Patching $bin"
    patchelf --set-rpath '$ORIGIN/../lib:$ORIGIN/../python/lib' "$bin" || echo "  [!] Failed to patch $bin"
  fi
done

# Create symlinks
echo "[+] Creating symlinks for python3 and pip3..."
ln -sf "$INSTALL_DIR/bin/python3.12" "$INSTALL_DIR/bin/python3"
ln -sf "$INSTALL_DIR/bin/pip3.12" "$INSTALL_DIR/bin/pip3"

# Verify using embedded library path
echo "[+] Verifying Python installation..."
LD_LIBRARY_PATH="$INSTALL_DIR/lib:$LD_LIBRARY_PATH" "$INSTALL_DIR/bin/python3" --version
LD_LIBRARY_PATH="$INSTALL_DIR/lib:$LD_LIBRARY_PATH" "$INSTALL_DIR/bin/pip3" --version

echo "[+] Portable Python $PYTHON_VERSION successfully built."
