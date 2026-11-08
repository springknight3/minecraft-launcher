#!/usr/bin/env bash
set -euo pipefail

if [[ $# -ne 2 ]]; then
    printf 'Usage: %s <work-directory> <install-prefix>\n' "${0##*/}" >&2
    exit 2
fi

work_dir="$1"
install_prefix="$2"
mkdir -p "$work_dir" "$install_prefix"

if command -v qmake6 >/dev/null 2>&1; then
    qmake_path="$(command -v qmake6)"
elif command -v qmake >/dev/null 2>&1; then
    qmake_path="$(command -v qmake)"
else
    printf 'Qt 6 qmake was not found on PATH\n' >&2
    exit 1
fi

qt_version="$("$qmake_path" -query QT_VERSION)"
if [[ "$qt_version" != 6.* ]]; then
    printf 'Qt 6 is required, but %s reports Qt %s\n' "$qmake_path" "$qt_version" >&2
    exit 1
fi
qt_prefix="$("$qmake_path" -query QT_INSTALL_PREFIX)"

ecm_source="$work_dir/extra-cmake-modules"
keychain_source="$work_dir/qtkeychain"
if [[ -e "$ecm_source" || -e "$keychain_source" ]]; then
    printf 'Dependency source directory already exists under %s\n' "$work_dir" >&2
    exit 1
fi

git clone --depth 1 --branch v6.8.0 \
    https://github.com/KDE/extra-cmake-modules.git "$ecm_source"
git clone --depth 1 --branch 0.14.3 \
    https://github.com/frankosterfeld/qtkeychain.git "$keychain_source"

ecm_prefix="$install_prefix/ecm"
cmake -S "$ecm_source" -B "$work_dir/build-ecm" \
    -DCMAKE_BUILD_TYPE=Release \
    -DCMAKE_INSTALL_PREFIX="$ecm_prefix" \
    -DBUILD_TESTING=OFF \
    -DBUILD_HTML_DOCS=OFF \
    -DBUILD_MAN_DOCS=OFF \
    -DBUILD_QTHELP_DOCS=OFF
cmake --build "$work_dir/build-ecm" --parallel
cmake --install "$work_dir/build-ecm"

cmake -S "$keychain_source" -B "$work_dir/build-qtkeychain" \
    -DCMAKE_BUILD_TYPE=Release \
    -DCMAKE_INSTALL_PREFIX="$install_prefix" \
    -DCMAKE_PREFIX_PATH="$qt_prefix;$ecm_prefix" \
    -DBUILD_WITH_QT6=ON \
    -DBUILD_TRANSLATIONS=OFF \
    -DBUILD_TESTING=OFF \
    -DBUILD_TEST_APPLICATION=OFF
cmake --build "$work_dir/build-qtkeychain" --parallel
cmake --install "$work_dir/build-qtkeychain"

printf 'QtKeychain installed under %s\n' "$install_prefix"
