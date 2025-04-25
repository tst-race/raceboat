#!/bin/bash
set -e

REPO_URL="https://github.com/tst-race/raceboat"
REPO_NAME="raceboat"
ARCH=$(uname -m)
CLONE_DIR="./$REPO_NAME"
WORK_DIR="$(pwd)"
DOCKER_IMAGE="ghcr.io/tst-race/raceboat/raceboat-builder:main"

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

echo "Starting build in Docker container..."
docker run --rm -e MAKEFLAGS="-j" -v "$CLONE_DIR:/code/" -w /code "$DOCKER_IMAGE" ./build.sh || {
  echo "Build failed. Exiting."
  exit 1
}

echo "Locating built output..."
BUILD_ROOT="$CLONE_DIR/racesdk/package"
ARTIFACT_PATH=$(find "$BUILD_ROOT" -type d -path "*/app" | grep "$ARCH" | head -n1)

if [ -z "$ARTIFACT_PATH" ]; then
  echo "Could not find a valid app/ directory for architecture: $ARCH"
  exit 1
fi

echo "Found artifacts under: $ARTIFACT_PATH"

echo "Assembling package in: $PACKAGE_DIR"
rm -rf "$PACKAGE_DIR"
mkdir -p "$PACKAGE_DIR/bin" "$PACKAGE_DIR/lib" "$PACKAGE_DIR/go"

cp -r "$ARTIFACT_PATH"/* "$PACKAGE_DIR/bin/"
cp -r "$(dirname "$ARTIFACT_PATH")/lib"/* "$PACKAGE_DIR/lib/" 2>/dev/null || true
cp -r "$(dirname "$ARTIFACT_PATH")/go"/* "$PACKAGE_DIR/go/" 2>/dev/null || true

# ---- SAFE SHARED LIB DETECTION ----
if [[ "$OSTYPE" == "darwin"* ]]; then
  echo "Skipping shared library detection — not supported on macOS."
else
  echo "Detecting and copying shared libraries (using readelf)..."

  BINARIES=$(find "$PACKAGE_DIR/bin" -type f -executable)

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
        cp -u "$LIB_PATH" "$PACKAGE_DIR/lib/" || echo " Failed to copy $LIB_PATH"
      fi
    done
  done
fi
# ---- END SHARED LIB DETECTION ----

# Copy setup.sh to the package directory
echo "Copying setup.sh to the package directory..."
cp "$WORK_DIR/setup.sh" "$PACKAGE_DIR/"
chmod +x "$PACKAGE_DIR/setup.sh"

# Generate architecture-specific start.sh
echo "Generating start.sh for $VERSION-$ARCH..."

START_SCRIPT_NAME="start-${VERSION}-${ARCH}.sh"

cat > "$PACKAGE_DIR/$START_SCRIPT_NAME" <<'EOF'
#!/bin/bash
set -e

echo "Checking for required dependencies..."

MISSING_DEPENDENCY=false

# Check if Python 3.12 is installed
if ! command -v python3.12 >/dev/null; then
  echo "[-] Python 3.12 is not installed."
  MISSING_DEPENDENCY=true
fi

# Check if Python 3.12 dev headers are available
if [ ! -f "/usr/include/python3.12/Python.h" ]; then
  echo "[-] Python 3.12 dev headers are missing."
  MISSING_DEPENDENCY=true
fi

if [ "$MISSING_DEPENDENCY" = true ]; then
  echo "[+] Missing required dependencies. Running setup.sh to install Python 3.12 and the required development headers..."
  chmod +x "$(dirname "$0")/setup.sh"
  if ! "$(dirname "$0")/setup.sh"; then
    echo "[-] setup.sh failed. Please run setup.sh install the missing dependencies manually."
    exit 1
  fi
  echo "[+] Dependencies finished installing."
fi

echo "[+] All dependencies are installed."

# Now that dependencies are handled, proceed to run the app
echo "Starting Raceboat..."
export LD_LIBRARY_PATH=$(dirname "$0")/lib:$LD_LIBRARY_PATH
exec $(dirname "$0")/bin/race-cli "$@"
EOF
chmod +x "$PACKAGE_DIR/$START_SCRIPT_NAME"

# Copy diagnose.sh to the package directory
echo "Copying diagnose.sh to the package directory..."
cp "$WORK_DIR/diagnose.sh" "$PACKAGE_DIR/"
chmod +x "$PACKAGE_DIR/diagnose.sh"

# Copy the icon to the package directory
echo "Copying icon to the package directory..."
cp "$WORK_DIR/boat.png" "$PACKAGE_DIR/"

# Create a .desktop file in the package directory
echo "Creating Raceboat.desktop file..."
cat > "$PACKAGE_DIR/Raceboat.desktop" <<EOF
[Desktop Entry]
Version=$VERSION
Name=Raceboat
Comment=Raceboat Application
Exec=$(realpath "$PACKAGE_DIR/$START_SCRIPT_NAME") %U
Icon=$(realpath "$PACKAGE_DIR/boat.png")  # Make sure to copy the icon to the package
Terminal=true
Type=Application
Categories=Utility;
X-AppImage-Integrate=false
EOF

chmod +x "$PACKAGE_DIR/Raceboat.desktop"

# Update README.txt with setup script information
echo "Generating README.txt..."
cat > "$PACKAGE_DIR/README.txt" <<EOF
Raceboat Portable Package (arch: $ARCH, version: $VERSION)

To run:
1. Make sure Python 3.12 and its development headers are installed.
2. Double-click ${START_SCRIPT_NAME}
   - OR -
   Run from terminal: ./start-${VERSION}-${ARCH}.sh

This package contains architecture-specific binaries and support files.

To run (chmod calls should be unnecessary):
  chmod +x ${START_SCRIPT_NAME}
  Double-click ${START_SCRIPT_NAME}
   - OR -
   Run from terminal: ./${START_SCRIPT_NAME}

If you're missing dependencies (e.g., Python 3.12 or dev headers), you can run setup.sh:
  chmod +x setup.sh
  ./setup.sh
  This will install the required Python packages.

To check dependencies:
  chmod +x diagnose.sh
  ./diagnose.sh

Structure:
  bin/  -> main executables
  lib/  -> shared libraries
  go/   -> Go sources or assets
  ${START_SCRIPT_NAME} -> launcher
  setup.sh -> setup script for missing dependencies
  diagnose.sh -> optional binary/linker check
  .desktop -> desktop launcher for raceboat
  boat.png -> desktop launcher icon
  
Generated on: $(date)
EOF

echo "Detecting dependency sources for documentation..."
DEPS_FILE="$PACKAGE_DIR/DEPENDENCIES.txt"
touch "$DEPS_FILE"

echo -e "\nDockerfile Dependencies:" >> "$DEPS_FILE"
find "$CLONE_DIR" -name "Dockerfile" | while read -r file; do
  grep -E 'apt(-get)? install|pip install' "$file" >> "$DEPS_FILE" || true
done

echo -e "\nCMakeLists.txt Dependencies:" >> "$DEPS_FILE"
find "$CLONE_DIR" -name "CMakeLists.txt" | while read -r file; do
  grep -E '(find_package|add_subdirectory|target_link_libraries)' "$file" >> "$DEPS_FILE" || true
done

echo -e "\nPython Requirements:" >> "$DEPS_FILE"
find "$CLONE_DIR" -name "requirements*.txt" -exec cat {} \; >> "$DEPS_FILE" 2>/dev/null || true
find "$CLONE_DIR" -name "*.py" | while read -r file; do
  grep -E '^import |^from ' "$file" >> "$DEPS_FILE" || true
done

echo "Creating zip file..."
rm -f "$ZIPFILE"
cd "$WORK_DIR"
zip -r "$(basename "$ZIPFILE")" "$(basename "$PACKAGE_DIR")"

echo "Done. Created:"
echo "  - Directory: $PACKAGE_DIR"
echo "  - Archive:   $ZIPFILE"
