#!/bin/bash
# Native macOS workflow. Compatible with the Bash 3.2 shipped by Apple.
set -euo pipefail

root="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
command_name="${1:-help}"
if [ "$#" -gt 0 ]; then shift; fi

fail() { printf 'ERROR: %s\n' "$*" >&2; exit 1; }
require() { command -v "$1" >/dev/null 2>&1 || fail "Missing $1. Run: bash scripts/macos.sh setup"; }
if [ -x "$root/.venv/bin/clang-format" ]; then
    PATH="$root/.venv/bin:$PATH"
    export PATH
fi

if [ "$command_name" = help ] || [ "$command_name" = --help ]; then
    cat <<'HELP'
Usage: bash scripts/macos.sh <command> [arguments]
  setup                  Install dependencies, Git LFS assets, and repository hooks
  doctor                 Check required tools, Vulkan, Git LFS assets, and hooks
  build [target ...]     Configure and build (all targets by default)
  test [ctest regex]    Run registered unit tests (requires a build)
  smoke [name glob]     Quick startup/render/exit checks for built apps
  run <target> [args]   Build and run an example, game, or tool
  lint                   Check changed C++ formatting, shaders, and shell scripts
  verify                 Build, unit tests, quick smoke checks, then lint; save logs

Environment:
  VDE_CONFIG=Debug|Release (default Debug)
  VDE_JOBS=<positive integer> (default logical CPU count)
  VDE_SMOKE_TIMEOUT=<seconds> (default 45)
  VDE_LLDB_ON_FAILURE=1 (test: rerun up to 3 failed tests under lldb for backtraces)
  VULKAN_SDK=<SDK's macOS directory> (optional; otherwise use Homebrew)
Build output: build_ninja. Windows PowerShell workflows are unchanged.
HELP
    exit 0
fi

[ "$(uname -s)" = Darwin ] || fail "This workflow is for macOS. Use the existing PowerShell scripts on Windows."
case "$command_name" in setup|doctor|build|test|smoke|run|lint|verify) ;; *) fail "Unknown command: $command_name (use help)" ;; esac

if [ "$command_name" = setup ]; then
    require brew
    xcode-select -p >/dev/null || fail "Install Apple command-line tools with: xcode-select --install"
    brew bundle --file="$root/scripts/Brewfile.macos"
    PATH="$(brew --prefix)/bin:$PATH"
    export PATH
    python3 -m venv "$root/.venv"
    "$root/.venv/bin/python" -m pip install --disable-pip-version-check "clang-format==21.1.8"
    git -C "$root" lfs install --local --skip-repo
    git -C "$root" lfs pull
    git -C "$root" config --local core.hooksPath .githooks
    printf 'PASS: macOS dependencies, clang-format, assets, and git hooks installed.\n'
    exit 0
fi

config="${VDE_CONFIG:-Debug}"
case "$config" in Debug|Release) ;; *) fail "VDE_CONFIG must be Debug or Release" ;; esac
jobs="${VDE_JOBS:-$(sysctl -n hw.logicalcpu)}"
case "$jobs" in ''|*[!0-9]*|0) fail "VDE_JOBS must be a positive integer" ;; esac
build_dir="$root/build_ninja"

# Keep SDK selection local to this process and its children, not shell profiles.
if [ -n "${VULKAN_SDK:-}" ]; then
    [ -d "$VULKAN_SDK" ] || fail "VULKAN_SDK is not a directory: $VULKAN_SDK"
    prefix="$VULKAN_SDK"
else
    require brew
    prefix="$(brew --prefix)"
fi
export CMAKE_PREFIX_PATH="$prefix${CMAKE_PREFIX_PATH:+:$CMAKE_PREFIX_PATH}"
export PATH="$root/.venv/bin:$prefix/bin:$PATH"
# Use the fallback path so leaf-name dlopen() calls (GLFW's libvulkan.1.dylib) find
# SDK libraries without overriding system dylibs. DYLD_LIBRARY_PATH is searched
# before install names, so on case-insensitive APFS Homebrew's libpng/libjpeg/libgif
# would replace ImageIO's private libPng/libJPEG/libGIF used to draw Apple emoji.
export DYLD_FALLBACK_LIBRARY_PATH="$prefix/lib:${DYLD_FALLBACK_LIBRARY_PATH:-$HOME/lib:/usr/local/lib:/usr/lib}"
if [ -z "${VK_DRIVER_FILES:-}" ] && [ -z "${VK_ICD_FILENAMES:-}" ]; then
    for manifest in "$prefix/share/vulkan/icd.d/MoltenVK_icd.json" \
                    "$prefix/etc/vulkan/icd.d/MoltenVK_icd.json"; do
        if [ -f "$manifest" ]; then export VK_DRIVER_FILES="$manifest"; break; fi
    done
fi
if [ -z "${VK_LAYER_PATH:-}" ] && [ -d "$prefix/share/vulkan/explicit_layer.d" ]; then
    export VK_LAYER_PATH="$prefix/share/vulkan/explicit_layer.d"
fi

doctor() {
    local failures=0 version loader_prefix molten_prefix manifest hooks_path override lfs_listing
    printf 'VDE macOS preflight\n'

    if [ -n "${VULKAN_SDK:-}" ]; then
        printf 'PASS: Using Vulkan SDK (%s).\n' "$VULKAN_SDK"
    elif brew bundle check --file="$root/scripts/Brewfile.macos" >/dev/null 2>&1; then
        printf 'PASS: Homebrew dependencies are installed.\n'
    else
        printf 'FAIL: Homebrew dependencies are missing. Fix: bash scripts/macos.sh setup\n'
        failures=$((failures + 1))
    fi

    for tool in cmake ninja git git-lfs python3 glslangValidator; do
        if command -v "$tool" >/dev/null 2>&1; then
            case "$tool" in
                cmake) version="$(cmake --version | head -n 1)" ;;
                ninja) version="$(ninja --version)" ;;
                git) version="$(git --version)" ;;
                git-lfs) version="$(git-lfs version)" ;;
                python3) version="$(python3 --version)" ;;
                glslangValidator) version="$(glslangValidator --version | head -n 1)" ;;
            esac
            printf 'PASS: %s (%s).\n' "$tool" "$version"
        else
            printf 'FAIL: %s is missing. Fix: bash scripts/macos.sh setup\n' "$tool"
            failures=$((failures + 1))
        fi
    done

    if command -v clang-format >/dev/null 2>&1; then
        version="$(clang-format --version)"
        if printf '%s\n' "$version" | grep -Eq '(^|[^0-9])21\.1\.8([^0-9]|$)'; then
            printf 'PASS: clang-format is pinned (%s).\n' "$version"
        else
            printf 'FAIL: clang-format must be version 21.1.8 (found %s). Fix: bash scripts/macos.sh setup\n' "$version"
            failures=$((failures + 1))
        fi
    else
        printf 'FAIL: clang-format 21.1.8 is missing. Fix: bash scripts/macos.sh setup\n'
        failures=$((failures + 1))
    fi

    if [ -n "${VULKAN_SDK:-}" ]; then
        loader_prefix="$VULKAN_SDK"
        if [ -f "$loader_prefix/lib/libvulkan.1.dylib" ]; then
            printf 'PASS: Vulkan loader found (%s).\n' "$loader_prefix/lib/libvulkan.1.dylib"
        else
            printf 'FAIL: Vulkan loader is missing from VULKAN_SDK. Fix: install or select a complete LunarG Vulkan SDK.\n'
            failures=$((failures + 1))
        fi
    else
        loader_prefix="$(brew --prefix vulkan-loader 2>/dev/null || true)"
        if [ -n "$loader_prefix" ] && [ -d "$loader_prefix/lib" ]; then
            printf 'PASS: Vulkan loader found (%s).\n' "$loader_prefix"
        else
            printf 'FAIL: Vulkan loader is missing. Fix: brew bundle --file=scripts/Brewfile.macos\n'
            failures=$((failures + 1))
        fi
    fi

    if [ -n "${VULKAN_SDK:-}" ]; then
        molten_prefix="$VULKAN_SDK"
    else
        molten_prefix="$(brew --prefix molten-vk 2>/dev/null || true)"
    fi
    manifest=""
    for candidate in "$molten_prefix/share/vulkan/icd.d/MoltenVK_icd.json" \
                     "$molten_prefix/etc/vulkan/icd.d/MoltenVK_icd.json"; do
        if [ -f "$candidate" ]; then manifest="$candidate"; break; fi
    done
    override="${VK_DRIVER_FILES:-${VK_ICD_FILENAMES:-}}"
    if [ -z "$manifest" ] && [ -n "$override" ]; then
        case "$override" in *:*) override="${override%%:*}" ;; esac
        [ -f "$override" ] && manifest="$override"
    fi
    if [ -n "$manifest" ]; then
        printf 'PASS: Vulkan ICD is available%s.\n' "${manifest:+ ($manifest)}"
    else
        printf 'FAIL: MoltenVK ICD manifest is missing. Fix: brew bundle --file=scripts/Brewfile.macos\n'
        failures=$((failures + 1))
    fi

    if git -C "$root" lfs fsck >/dev/null 2>&1; then
        if lfs_listing="$(git -C "$root" lfs ls-files --long 2>/dev/null)"; then
            if printf '%s\n' "$lfs_listing" | grep -Eq '^[0-9a-f]{64} - '; then
                printf 'FAIL: Git LFS assets are still pointer files. Fix: git -C "%s" lfs pull && git -C "%s" lfs checkout\n' "$root" "$root"
                failures=$((failures + 1))
            else
                printf 'PASS: Git LFS assets are present in the working tree.\n'
            fi
        else
            printf 'FAIL: Could not verify Git LFS working-tree assets. Fix: git -C "%s" lfs pull\n' "$root"
            failures=$((failures + 1))
        fi
    else
        printf 'FAIL: Git LFS objects are missing or invalid. Fix: git -C "%s" lfs pull\n' "$root"
        failures=$((failures + 1))
    fi

    hooks_path="$(git -C "$root" config --local --get core.hooksPath 2>/dev/null || true)"
    if [ "$hooks_path" = ".githooks" ]; then
        printf 'PASS: Repository hooks are enabled.\n'
    else
        printf 'FAIL: Repository hooks are not enabled. Fix: bash scripts/macos.sh setup\n'
        failures=$((failures + 1))
    fi

    [ "$failures" -eq 0 ] || fail "Preflight found $failures problem(s)"
    printf 'PASS: macOS preflight checks.\n'
}

build() {
    require cmake
    require ninja
    cmake -S "$root" -B "$build_dir" -G Ninja \
        "-DCMAKE_BUILD_TYPE=$config" -DCMAKE_EXPORT_COMPILE_COMMANDS=ON \
        -DVDE_BUILD_EXAMPLES=ON -DVDE_BUILD_GAMES=ON -DVDE_BUILD_TOOLS=ON -DVDE_BUILD_TESTS=ON \
        -DCMAKE_POLICY_VERSION_MINIMUM=3.5 || return $?
    if [ "$#" -gt 0 ]; then
        cmake --build "$build_dir" --parallel "$jobs" --target "$@" || return $?
    else
        cmake --build "$build_dir" --parallel "$jobs" || return $?
    fi
    printf 'PASS: macOS %s build.\n' "$config"
}

check_build() {
    [ -f "$build_dir/CMakeCache.txt" ] || fail "Build first: bash scripts/macos.sh build"
    grep -q "^CMAKE_BUILD_TYPE:STRING=$config\$" "$build_dir/CMakeCache.txt" ||
        fail "Build configuration differs. Run build with VDE_CONFIG=$config first."
}

# Rerun failed tests under lldb so crashes (e.g. SIGBUS) produce native backtraces.
crash_backtraces() {
    local failed="$build_dir/Testing/Temporary/LastTestsFailed.log" count=0 line name log
    local -a env_args=()
    [ -f "$failed" ] || return 0
    if ! command -v lldb >/dev/null 2>&1; then
        printf 'WARNING: lldb not found; failed-test backtraces skipped.\n'
        return 0
    fi
    # /usr/bin/lldb is SIP-protected, so DYLD_* variables must be forwarded explicitly.
    for name in DYLD_LIBRARY_PATH DYLD_FALLBACK_LIBRARY_PATH; do
        if [ -n "${!name:-}" ]; then
            env_args+=(-O "settings set target.env-vars $name=${!name}")
        fi
    done
    mkdir -p "$root/logs"
    while IFS= read -r line && [ "$count" -lt 3 ]; do
        name="${line#*:}"
        case "$name" in ''|*[!A-Za-z0-9_./-]*) continue ;; esac
        count=$((count + 1))
        log="$root/logs/macos-lldb-${name//\//_}.log"
        printf '==> lldb backtrace for %s (%s)\n' "$name" "$log"
        lldb --batch ${env_args[@]+"${env_args[@]}"} -o run \
            -k 'thread backtrace all' -k 'image list' -k quit \
            -- "$build_dir/tests/vde_tests" "--gtest_filter=$name" </dev/null >"$log" 2>&1 || true
        sed -n '/stop reason/,/image list/p' "$log" | head -n 120 || true
        grep -Ei 'homebrew|/usr/local/|libpng|libjpeg|libgif|libtiff' "$log" | grep -v 'gtest_filter' || true
    done <"$failed"
}

unit_tests() {
    require ctest
    check_build
    local status=0
    ctest --test-dir "$build_dir" --output-on-failure --no-tests=error \
        --parallel "$jobs" -R "${1:-.}" || status=$?
    if [ "$status" -ne 0 ]; then
        if [ "${VDE_LLDB_ON_FAILURE:-0}" = 1 ]; then crash_backtraces; fi
        fail "macOS unit tests failed (ctest exit $status)"
    fi
    printf 'PASS: macOS unit tests.\n'
}

find_app() {
    case "$1" in vde_* ) ;; *) fail "Expected a CMake application target starting with vde_" ;; esac
    case "$1" in *[!a-zA-Z0-9_]* ) fail "Invalid application target: $1" ;; esac
    app=""
    while IFS= read -r -d '' candidate; do
        if [ -x "$candidate" ]; then
            [ -z "$app" ] || fail "Multiple executables found for $1"
            app="$candidate"
        fi
    done < <(find "$build_dir/examples" "$build_dir/games" "$build_dir/tools" \
        -type f -name "$1" -print0)
    [ -n "$app" ] || fail "Executable not found for $1"
}

smoke() {
    check_build
    local timeout="${VDE_SMOKE_TIMEOUT:-45}" count=0 failures=0 exe name log pid watchdog status script
    case "$timeout" in ''|*[!0-9]*|0) fail "VDE_SMOKE_TIMEOUT must be a positive integer" ;; esac
    mkdir -p "$root/logs"
    while IFS= read -r -d '' exe; do
        [ -x "$exe" ] || continue
        name="$(basename "$exe")"
        case "$name" in vde_triangle_example) continue ;; esac
        # shellcheck disable=SC2254 # The argument is intentionally a name glob.
        case "$name" in ${1:-vde_*}) ;; *) continue ;; esac
        count=$((count + 1))
        log="$root/logs/macos-smoke-$name.log"
        script="$root/smoketests/scripts/smoke_quick.vdescript"
        # Audio requires input to start playback; launch-only coverage would be silent.
        if [ "$name" = vde_audio_demo ]; then
            script="$root/smoketests/scripts/smoke_audio.vdescript"
        fi
        (cd "$(dirname "$exe")"; exec "$exe" --input-script \
            "$script") >"$log" 2>&1 &
        pid=$!
        # A bounded watchdog prevents an unresponsive graphics app hanging verification.
        (
            trap 'kill "$sleeper" 2>/dev/null || true' EXIT
            trap 'exit 0' TERM
            sleep "$timeout" &
            sleeper=$!
            wait "$sleeper"
            kill -TERM "$pid" 2>/dev/null
            sleep 2 &
            sleeper=$!
            wait "$sleeper"
            kill -KILL "$pid" 2>/dev/null || true
        ) &
        watchdog=$!
        trap 'kill -KILL "$pid" 2>/dev/null || true; kill "$watchdog" 2>/dev/null || true; wait "$pid" 2>/dev/null || true; wait "$watchdog" 2>/dev/null || true' EXIT
        trap 'exit 130' INT
        trap 'exit 143' TERM
        status=0
        wait "$pid" || status=$?
        kill "$watchdog" 2>/dev/null || true
        wait "$watchdog" 2>/dev/null || true
        trap - EXIT INT TERM
        if [ "$status" -ne 0 ] ||
            ! grep -qF '[VDE:InputScript] exit' "$log" ||
            grep -Eiq '^Validation layer:.*Validation Error|ASSERT FAILED|(^|[[:space:]])error:|failed to|fatal|exception' "$log"; then
            printf 'FAIL: %s (exit %s; %s)\n' "$name" "$status" "$log"
            tail -n 25 "$log"
            failures=$((failures + 1))
        else
            printf 'PASS: %s\n' "$name"
        fi
    done < <(find "$build_dir/examples" "$build_dir/games" "$build_dir/tools" \
        -type f -name 'vde_*' ! -name '*.*' -print0)
    [ "$count" -gt 0 ] || fail "No matching built applications found"
    [ "$failures" -eq 0 ] || fail "$failures of $count macOS smoke checks failed"
    printf 'PASS: All %s macOS quick smoke checks passed.\n' "$count"
}

lint() {
    local file formatter glslang_validator version
    require clang-format
    formatter="$(command -v clang-format)"
    version="$("$formatter" --version)"
    printf '%s\n' "$version" | grep -Eq '(^|[^0-9])21\.1\.8([^0-9]|$)' ||
        fail "clang-format 21.1.8 is required (found $version). Run: bash scripts/macos.sh setup"
    bash -n "$root/scripts/macos.sh"
    if command -v shellcheck >/dev/null 2>&1; then
        shellcheck "$root/scripts/macos.sh"
    else
        printf 'WARNING: shellcheck not installed; shell static analysis skipped.\n'
    fi
    while IFS= read -r -d '' file; do
        case "$file" in
            *.cpp|*.h|*.hpp) "$formatter" --dry-run --Werror "$root/$file" ;;
            *.vert|*.frag)
                require glslangValidator
                glslang_validator="$(command -v glslangValidator)"
                "$glslang_validator" -V "$root/$file" -o /dev/null
                ;;
        esac
    done < <(git -C "$root" diff --name-only -z --diff-filter=ACMR HEAD)
    printf 'PASS: macOS changed-file lint checks.\n'
}

case "$command_name" in
    doctor) doctor ;;
    build) build "$@" ;;
    test) unit_tests "$@" ;;
    smoke) smoke "$@" ;;
    lint) lint ;;
    run)
        [ "$#" -gt 0 ] || fail "Usage: bash scripts/macos.sh run <target> [args]"
        target="$1"; shift
        build "$target"
        find_app "$target"
        cd "$(dirname "$app")"
        exec "$app" "$@"
        ;;
    verify)
        mkdir -p "$root/logs"
        (
            doctor
            status=$?
            [ "$status" -eq 0 ] || exit "$status"
            build
            status=$?
            [ "$status" -eq 0 ] || exit "$status"
            unit_tests
            status=$?
            [ "$status" -eq 0 ] || exit "$status"
            smoke
            status=$?
            [ "$status" -eq 0 ] || exit "$status"
            lint
            status=$?
            [ "$status" -eq 0 ] || exit "$status"
        ) 2>&1 | tee "$root/logs/verify-macos-latest.log"
        ;;
esac
