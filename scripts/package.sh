#!/usr/bin/env bash
set -euo pipefail

usage() {
    printf 'Usage: %s [linux|macos|windows|all]\n' "${0##*/}" >&2
}

fail() {
    printf 'package.sh: %s\n' "$1" >&2
    exit 1
}

requested_platform="${1:-all}"
case "$requested_platform" in
    linux|macos|windows|all) ;;
    *) usage; fail "unsupported platform '$requested_platform'" ;;
esac
if [[ $# -gt 1 ]]; then
    usage
    fail "expected at most one platform argument"
fi

script_dir="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)"
root_dir="$(cd -- "$script_dir/.." && pwd)"
version="$(sed -n 's/^project(StrataLauncher VERSION \([^ )]*\).*/\1/p' "$root_dir/CMakeLists.txt")"
[[ -n "$version" ]] || fail "could not read project version from CMakeLists.txt"

case "$(uname -s)" in
    Linux) host_platform=linux ;;
    Darwin) host_platform=macos ;;
    MINGW*|MSYS*|CYGWIN*) host_platform=windows ;;
    *) fail "unsupported build host '$(uname -s)'";;
esac

platform="$requested_platform"
[[ "$platform" != all ]] || platform="$host_platform"
[[ "$platform" == "$host_platform" ]] ||
    fail "cannot build '$platform' artifacts on '$host_platform'; run this script on the target OS"

arch="$(uname -m)"
case "$arch" in
    x86_64|amd64|AMD64) arch=x86_64 ;;
    aarch64|arm64) arch=arm64 ;;
    *) fail "unsupported architecture '$arch'" ;;
esac

build_dir="${BUILD_DIR:-$root_dir/build/package}"
output_dir="${OUTPUT_DIR:-$root_dir/dist}"
mkdir -p "$build_dir" "$output_dir"

cmake -S "$root_dir" -B "$build_dir" -DCMAKE_BUILD_TYPE=Release
cmake --build "$build_dir" --parallel

case "$platform" in
    linux)
        command -v linuxdeploy >/dev/null 2>&1 ||
            fail "linuxdeploy is required for Linux packages (including its Qt plugin)"
        available_plugins="$(linuxdeploy --list-plugins)"
        grep -q '^[[:space:]]*qt:' <<<"$available_plugins" ||
            fail "linuxdeploy-plugin-qt must be installed alongside linuxdeploy"
        grep -q '^[[:space:]]*appimage:' <<<"$available_plugins" ||
            fail "linuxdeploy's AppImage output plugin is required"
        qmake_path="$(command -v qmake6 || command -v qmake || true)"
        [[ -n "$qmake_path" ]] || fail "Qt 6 qmake is required to deploy QML imports"
        qmake_version="$("$qmake_path" -query QT_VERSION)"
        [[ "$qmake_version" == 6.* ]] ||
            fail "linuxdeploy must use Qt 6 qmake, found Qt $qmake_version"
        qt_plugins="$("$qmake_path" -query QT_INSTALL_PLUGINS)"
        [[ -f "$qt_plugins/platforms/libqoffscreen.so" ]] ||
            fail "Qt's offscreen QPA plugin is required for headless smoke tests"
        [[ -f "$qt_plugins/imageformats/libqsvg.so" ]] ||
            fail "Qt's SVG image plugin is required for launcher icons"
        wayland_plugins=()
        for plugin in libqwayland.so libqwayland-egl.so libqwayland-generic.so; do
            if [[ -f "$qt_plugins/platforms/$plugin" ]]; then
                wayland_plugins+=("$plugin")
            fi
        done
        (( ${#wayland_plugins[@]} > 0 )) ||
            fail "a Qt Wayland QPA plugin is required for native Wayland support"
        wayland_plugins_csv="$(IFS=';'; printf '%s' "${wayland_plugins[*]}")"
        appdir="$(mktemp -d "$build_dir/AppDir.XXXXXX")"
        appimage_dir="$(mktemp -d "$build_dir/AppImage.XXXXXX")"
        mkdir -p "$appdir/usr/share/applications" "$appdir/usr/share/icons/hicolor/scalable/apps"
        cmake --install "$build_dir" --prefix "$appdir/usr"
        cp "$root_dir/packaging/stratalauncher.desktop" "$appdir/usr/share/applications/"
        cp "$root_dir/packaging/stratalauncher.svg" \
            "$appdir/usr/share/icons/hicolor/scalable/apps/"

        (
            cd "$appimage_dir"
            APPIMAGE_EXTRACT_AND_RUN=1 \
            NO_STRIP=1 \
            QMAKE="$qmake_path" \
            EXTRA_QT_MODULES=svg \
            EXTRA_PLATFORM_PLUGINS="$wayland_plugins_csv;libqoffscreen.so" \
            QML_SOURCES_PATHS="$root_dir/cpp/app" \
                linuxdeploy --appdir "$appdir" \
                -e "$appdir/usr/bin/stratalauncher" \
                --desktop-file "$root_dir/packaging/stratalauncher.desktop" \
                --icon-file "$root_dir/packaging/stratalauncher.svg" \
                --custom-apprun "$root_dir/packaging/AppRun" \
                --plugin qt --output appimage
        )
        tar -C "$appdir/usr" -czf \
            "$output_dir/stratalauncher-${version}-linux-${arch}.tar.gz" .
        generated_appimage="$(find "$appimage_dir" -maxdepth 1 -type f -name '*.AppImage' -print -quit)"
        [[ -n "$generated_appimage" ]] || fail "linuxdeploy did not produce an AppImage"
        mv "$generated_appimage" "$output_dir/stratalauncher-${version}-linux-${arch}.AppImage"
        ;;
    macos)
        stage_dir="$(mktemp -d "$build_dir/stage.XXXXXX")"
        cmake --install "$build_dir" --prefix "$stage_dir"
        app_bundle="$stage_dir/StrataLauncher.app"
        [[ -d "$app_bundle" ]] || fail "CMake install did not produce StrataLauncher.app"
        ditto -c -k --sequesterRsrc --keepParent "$app_bundle" \
            "$output_dir/stratalauncher-${version}-macos-${arch}.zip"
        ;;
    windows)
        command -v makensis >/dev/null 2>&1 ||
            fail "NSIS (makensis) is required to build the Windows installer"
        package_dir="$(mktemp -d "$build_dir/installer.XXXXXX")"
        cpack --config "$build_dir/CPackConfig.cmake" -G NSIS -B "$package_dir"
        installer="$(find "$package_dir" -maxdepth 1 -type f -name '*.exe' -print -quit)"
        [[ -n "$installer" ]] || fail "CPack did not produce a Windows installer"
        mv "$installer" "$output_dir/stratalauncher-${version}-windows-${arch}.exe"
        ;;
esac

shopt -s nullglob
artifacts=("$output_dir"/stratalauncher-"$version"-"$platform"-"$arch".*)
package_count=0
if command -v sha256sum >/dev/null 2>&1; then
    for artifact in "${artifacts[@]}"; do
        [[ "$artifact" != *.sha256 ]] || continue
        sha256sum "$artifact" > "${artifact}.sha256"
        ((package_count += 1))
    done
elif command -v shasum >/dev/null 2>&1; then
    for artifact in "${artifacts[@]}"; do
        [[ "$artifact" != *.sha256 ]] || continue
        shasum -a 256 "$artifact" > "${artifact}.sha256"
        ((package_count += 1))
    done
else
    fail "sha256sum or shasum is required to checksum package artifacts"
fi
(( package_count > 0 )) || fail "no artifacts were created"

printf 'Package artifacts written to %s\n' "$output_dir"
