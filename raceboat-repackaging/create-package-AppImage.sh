#!/bin/bash
set -e

REPO_URL="https://github.com/tst-race/raceboat"
REPO_NAME="raceboat"
ARCH=$(uname -m)
CLONE_DIR="./$REPO_NAME"
WORK_DIR="$(pwd)"
DOCKER_IMAGE="ghcr.io/tst-race/raceboat/raceboat-builder:main"
APP_DIR="$WORK_DIR/AppDir"

echo "Checking for Raceboat repo..."
if [ ! -d "$CLONE_DIR" ]; then
  echo "Cloning Raceboat repository..."
  git clone --recurse-submodules "$REPO_URL" "$CLONE_DIR"
else
  echo "Repo already exists."
fi

# Get the version AFTER git clone
VERSION=$(git -C "$CLONE_DIR" describe --tags --always || echo "dev")

# Build the version dependent dirs now
PACKAGE_DIR="$WORK_DIR/raceboat_package_${VERSION}_${ARCH}"
ZIPFILE="$WORK_DIR/raceboat_package_${VERSION}_${ARCH}.zip"

if [ "$SKIP_DOCKER_BUILD" != "1" ]; then
  echo "Starting build in Docker container..."

  if [ ! -f "$CLONE_DIR/build.sh" ]; then
    echo "Error: build.sh not found in $CLONE_DIR. Check if the repo cloned correctly."
    exit 1
  fi

  docker run --rm -e MAKEFLAGS="-j" -v "$CLONE_DIR:/code/" -w /code "$DOCKER_IMAGE" ./build.sh || {
    echo "Build failed. Exiting."
    exit 1
  }
else
  echo "[=] Skipping Docker build (SKIP_DOCKER_BUILD=1)"
fi

echo "Locating built output..."
BUILD_ROOT="$CLONE_DIR/racesdk/package"
echo "Searching for build artifacts matching architecture: $ARCH"
ARTIFACT_PATH=$(find "$BUILD_ROOT" -type d -path "*/app" | grep "$ARCH" | head -n1)

if [ -z "$ARTIFACT_PATH" ]; then
  echo "[!] No exact match for architecture '$ARCH'. Trying fallback match (arm64/aarch64/x86_64)..."
  ARTIFACT_PATH=$(find "$BUILD_ROOT" -type d -path "*/app" | grep -iE "aarch64|arm64|x86_64" | head -n1)

  if [ -n "$ARTIFACT_PATH" ]; then
    echo "[!] Warning: Found app directory that may not match host architecture: $ARTIFACT_PATH"
    echo "    -> Expected architecture: $ARCH"
    echo "    -> Continuing anyway, but this may fail at runtime."
    # Optional: ask for confirmation here with read -p or bail
    # read -p "Continue anyway? [y/N] " resp
    # [[ "$resp" =~ ^[Yy]$ ]] || exit 1
  else
    echo "[-] No suitable build artifacts found."
    exit 1
  fi
fi

echo "Found artifacts under: $ARTIFACT_PATH"

# Create AppDir structure
echo "Creating AppDir structure..."
rm -rf "$APP_DIR"
mkdir -p "$APP_DIR/usr/bin" "$APP_DIR/usr/lib" "$APP_DIR/usr/share/applications" "$APP_DIR/usr/share/icons/hicolor/256x256/apps"

# Copy application files to AppDir
cp -r "$ARTIFACT_PATH"/* "$APP_DIR/usr/bin/"
cp -r "$(dirname "$ARTIFACT_PATH")/lib"/* "$APP_DIR/usr/lib/" 2>/dev/null || true
cp -r "$(dirname "$ARTIFACT_PATH")/go"/* "$APP_DIR/usr/go/" 2>/dev/null || true

# Copy additional files
cp "$WORK_DIR/setup.sh" "$APP_DIR/"
chmod +x "$APP_DIR/setup.sh"

# Generate AppRun script
echo "Generating AppRun script..."
cat > "$APP_DIR/AppRun" <<'EOF'
#!/bin/bash
set -e

# Set up Python environment
export PATH="$APPDIR/usr/python/bin:$PATH"
export LD_LIBRARY_PATH="$APPDIR/usr/lib:$APPDIR/usr/python/lib:$LD_LIBRARY_PATH"

# Run the application
exec "$APPDIR/usr/bin/race-cli" "$@"
EOF
chmod +x "$APP_DIR/AppRun"

# Copy the icon
cp "$WORK_DIR/boat.png" "$APP_DIR/usr/share/icons/hicolor/256x256/apps/"
cp "$WORK_DIR/boat.png" "$APP_DIR"

# No .png extension on the file name in this .Desktop file
# .desktop file has overloaded version definition

# Copy the .desktop file
echo "Creating Raceboat.desktop file..."
cat > "$APP_DIR/Raceboat.desktop" <<EOF
[Desktop Entry]
Version=1.0
Name=Raceboat
Comment=Raceboat Application
Exec=AppRun %U
Icon=boat
Terminal=true
Type=Application
Categories=Utility;
X-AppImage-Version=$VERSION
X-AppImage-Integrate=false
EOF

# Run the Python build script
echo "[+] Building portable Python 3.12..."
./build-python3.12-portable.sh

echo "[+] Patching RPATH for all Python binaries..."
for bin in "$APP_DIR/usr/python/bin/"*; do
  if file "$bin" | grep -q "ELF.*executable"; then
    echo "  -> Patching $bin"
    patchelf --set-rpath '$ORIGIN/../lib:$ORIGIN/../python/lib' "$bin" || echo "  Failed to patch $bin"
  fi
done

# Check if the file already exists in the destination before copying
if [ ! -e "$APP_DIR/usr/lib/libpython3.12.so.1.0" ]; then
  cp "$APP_DIR/usr/python/lib/libpython3.12.so.1.0" "$APP_DIR/usr/lib/"
else
  echo "[*] Library libpython3.12.so.1.0 already exists in $APP_DIR/usr/lib/; skipping copy."
fi

# Step 2: Check for symlink for Python library (create if needed)
if [ ! -e "$APP_DIR/usr/lib/libpython3.12.so.1.0" ]; then
  echo "[*] Creating symlink for libpython3.12.so.1.0"
  ln -s "$APP_DIR/usr/python/lib/libpython3.12.so.1.0" "$APP_DIR/usr/lib/libpython3.12.so.1.0"
else
  echo "[*] Symlink for libpython3.12.so.1.0 already exists, skipping creation."
fi


# Step 3: Ensure Python executables are symlinked correctly

# Check if python3 symlink exists, and create it if not
if [ ! -e "$APP_DIR/usr/bin/python3" ]; then
  echo "[*] Creating symlink for python3"
  ln -s "$APP_DIR/usr/python/bin/python3.12" "$APP_DIR/usr/bin/python3"
else
  echo "[*] Symlink for python3 already exists, skipping creation."
fi

# Check if pip3 symlink exists, and create it if not
if [ ! -e "$APP_DIR/usr/bin/pip3" ]; then
  echo "[*] Creating symlink for pip3"
  ln -s "$APP_DIR/usr/python/bin/pip3.12" "$APP_DIR/usr/bin/pip3"
else
  echo "[*] Symlink for pip3 already exists, skipping creation."
fi

# Link the names python3 -> python and pip3 -> pip if they don't already exist
if [ ! -e "$APP_DIR/usr/bin/python" ]; then
  echo "[*] Creating symlink python -> python3"
  ln -s python3 "$APP_DIR/usr/bin/python"
else
  echo "[*] Symlink for python already exists, skipping creation."
fi

if [ ! -e "$APP_DIR/usr/bin/pip" ]; then
  echo "[*] Creating symlink pip -> pip3"
  ln -s pip3 "$APP_DIR/usr/bin/pip"
else
  echo "[*] Symlink for pip already exists, skipping creation."
fi

# ---- SAFE SHARED LIB DETECTION ----
if [[ "$OSTYPE" == "darwin"* ]]; then
  echo "Skipping shared library detection — not supported on macOS."
else
  echo "Detecting and copying shared libraries (using readelf)..."

  BINARIES=$(find "$APP_DIR/usr/bin" -type f -executable)

  for bin in $BINARIES; do
    echo "Checking $bin for shared library dependencies..."

    LIBS=$(readelf -d "$bin" 2>/dev/null | grep NEEDED | awk '{print $5}' | sed 's/\[//;s/\]//')

    for lib in $LIBS; do
      if [[ "$lib" =~ ^(libc\.so|ld-linux|libm\.so|libpthread\.so) ]]; then
        echo "  Skipping common system library: $lib"
        continue
      fi

      # Find actual path using ldconfig
      LIB_PATH=$(ldconfig -p 2>/dev/null | grep -w "$lib" | head -n1 | awk '{print $NF}')

      if [ -z "$LIB_PATH" ]; then
        echo "  Could not locate path for $lib"
      else
        echo "  Copying $lib from $LIB_PATH"
        cp -u "$LIB_PATH" "$APP_DIR/usr/lib/" || echo " Failed to copy $LIB_PATH"
      fi
    done
  done
fi
# ---- END SHARED LIB DETECTION ----

# Create the AppImage
echo "Creating AppImage..."

# Ensure appimagetool is installed dynamically based on architecture
command -v appimagetool >/dev/null 2>&1 || {
  echo "Error: appimagetool not found in PATH. Attempting to download and install."

  # Check the architecture and choose the corresponding appimagetool version
  case "$ARCH" in
    "x86_64")
      TOOL_URL="https://github.com/AppImage/AppImageKit/releases/download/continuous/appimagetool-x86_64.AppImage"
      ;;
    "aarch64"|"arm64")
      TOOL_URL="https://github.com/AppImage/AppImageKit/releases/download/continuous/appimagetool-aarch64.AppImage"
      ;;
    *)
      echo "Error: Unsupported architecture '$ARCH' for appimagetool. Supported: x86_64, aarch64/arm64."
      exit 1
      ;;
  esac

  # Download and make it executable
  wget "$TOOL_URL" -O appimagetool.AppImage
  chmod +x appimagetool.AppImage
  mv appimagetool.AppImage /usr/local/bin/appimagetool
  echo "appimagetool installed successfully."
}

# Check if appimagetool is now available
command -v appimagetool >/dev/null 2>&1 || {
  echo "Error: appimagetool installation failed or is not in PATH."
  exit 1
}

# Check if FUSE is installed
if ! command -v fuse >/dev/null 2>&1; then
  echo "Error: FUSE is not installed. Attempting to install."

  # For Debian-based systems (Ubuntu, etc.)
  if [ -f /etc/debian_version ]; then
    apt update && apt install -y fuse kmod
  else
    echo "Error: Unsupported Linux distribution or missing FUSE. Please install FUSE and kmod manually."
    exit 1
  fi

  echo "FUSE and kmod installed successfully."
else
  echo "FUSE is already installed."
fi

# Ensure FUSE module is loaded
if ! lsmod | grep fuse >/dev/null 2>&1; then
  echo "FUSE module not loaded, attempting to load it."
  modprobe fuse
  echo "FUSE module loaded."
else
  echo "FUSE module is already loaded."
fi

# Proceed with creating the AppImage
APPIMAGE_NAME="Raceboat-${VERSION}-${ARCH}.AppImage"

# Adjust ARCH variable for ARM
if [[ "$ARCH" == "aarch64" || "$ARCH" == "arm64" ]]; then
  TEMP_ARCH="arm_aarch64"
else
  TEMP_ARCH="$ARCH"
fi

# Create the AppImage with error handling
if ! ARCH=$TEMP_ARCH appimagetool "$APP_DIR" "$APPIMAGE_NAME"; then
    echo "Error: Failed to create AppImage."
    exit 1
fi
chmod +x "$APPIMAGE_NAME"

# Create launch.sh for double-click convenience
LAUNCH_SCRIPT_NAME="launch-${VERSION}-${ARCH}.sh"
echo "Creating $LAUNCH_SCRIPT_NAME..."

cat > "$WORK_DIR/$LAUNCH_SCRIPT_NAME" <<EOF
#!/bin/bash
chmod +x "./$APPIMAGE_NAME"
./$APPIMAGE_NAME "\$@"
EOF
chmod +x "$WORK_DIR/$LAUNCH_SCRIPT_NAME"

# Create a user-friendly README
echo "Creating README.txt..."
cat > "$WORK_DIR/README.txt" <<EOF
Raceboat Portable AppImage (${ARCH})

To run:
1. Unzip this archive.
2. Double-click ${APPIMAGE_NAME}
   - OR -
   Double-click ${LAUNCH_SCRIPT_NAME} if your system doesn't recognize .AppImage files.

This AppImage contains a fully self-contained build of Raceboat,
including Python 3.12 and required development headers.

No installation required. Enjoy!

EOF

mkdir -p "$PACKAGE_DIR"
mv "$APPIMAGE_NAME" "$PACKAGE_DIR/"
mv "$LAUNCH_SCRIPT_NAME" README.txt "$PACKAGE_DIR/"

# Clean up
rm -rf "$APP_DIR"

# Create a zip file of the package
echo "Creating zip file..."
cd "$WORK_DIR"
rm -f "$ZIPFILE"
zip -r "$(basename "$ZIPFILE")" "$(basename "$PACKAGE_DIR")"

echo "Done. Created:"
echo "  - Directory: $PACKAGE_DIR"
echo "  - Archive:   $ZIPFILE"
echo "  - AppImage:  $APPIMAGE_NAME"
