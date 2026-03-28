#!/bin/sh

set -eu

REPO_OWNER="${REPO_OWNER:-deepsoftworks}"
REPO_NAME="${REPO_NAME:-santana}"
REPO_REF="${REPO_REF:-main}"

install_pkg() {
  pkg="$1"
  if command -v brew >/dev/null 2>&1; then
    brew install "$pkg"
  elif command -v apt-get >/dev/null 2>&1; then
    sudo apt-get install -y "$pkg"
  elif command -v dnf >/dev/null 2>&1; then
    sudo dnf install -y "$pkg"
  elif command -v yum >/dev/null 2>&1; then
    sudo yum install -y "$pkg"
  elif command -v pacman >/dev/null 2>&1; then
    sudo pacman -S --noconfirm "$pkg"
  elif command -v apk >/dev/null 2>&1; then
    sudo apk add "$pkg"
  else
    printf 'Error: no supported package manager found to install: %s\n' "$pkg" >&2
    exit 1
  fi
}

ensure_cmd() {
  if ! command -v "$1" >/dev/null 2>&1; then
    printf 'Installing missing dependency: %s\n' "$1"
    install_pkg "$1"
    if ! command -v "$1" >/dev/null 2>&1; then
      printf 'Error: failed to install: %s\n' "$1" >&2
      exit 1
    fi
  fi
}

ensure_cmd curl
ensure_cmd tar
ensure_cmd cmake

if command -v nproc >/dev/null 2>&1; then
  JOBS="$(nproc)"
elif command -v getconf >/dev/null 2>&1; then
  JOBS="$(getconf _NPROCESSORS_ONLN 2>/dev/null || printf '1')"
elif command -v sysctl >/dev/null 2>&1; then
  JOBS="$(sysctl -n hw.logicalcpu 2>/dev/null || printf '1')"
else
  JOBS=1
fi

TMP_DIR="$(mktemp -d "${TMPDIR:-/tmp}/santana-install.XXXXXX")"
cleanup() {
  rm -rf "$TMP_DIR"
}
trap cleanup EXIT INT TERM

ARCHIVE_URL="https://github.com/${REPO_OWNER}/${REPO_NAME}/archive/refs/heads/${REPO_REF}.tar.gz"
SRC_DIR="${TMP_DIR}/${REPO_NAME}-${REPO_REF}"
BUILD_DIR="${SRC_DIR}/build"

printf 'Downloading %s...\n' "$ARCHIVE_URL"
curl -fsSL "$ARCHIVE_URL" | tar -xz -C "$TMP_DIR"

if [ ! -d "$SRC_DIR" ]; then
  printf 'Error: expected source directory not found: %s\n' "$SRC_DIR" >&2
  exit 1
fi

printf 'Configuring build...\n'
cmake -S "$SRC_DIR" -B "$BUILD_DIR" -DCMAKE_BUILD_TYPE=Release

printf 'Building (jobs=%s)...\n' "$JOBS"
cmake --build "$BUILD_DIR" -j"$JOBS"

printf 'Installing santana...\n'
if [ "$(id -u)" -eq 0 ]; then
  cmake --install "$BUILD_DIR"
else
  if command -v sudo >/dev/null 2>&1; then
    sudo cmake --install "$BUILD_DIR"
  else
    printf 'Error: sudo is required for system install. Run as root or install manually.\n' >&2
    exit 1
  fi
fi

printf 'Installed successfully. Run: santana --help\n'
