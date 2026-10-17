#!/bin/bash
# Stages a relocatable macOS editor preview from Binaries/Mac/Distribution and archives it.
#
#   Tools/Scripts/BuildHyperion.sh distribution ninja regenerate
#   Tools/Scripts/PackageEditorMacOS.sh [output dir]
#
# Homebrew and SDK libraries the binaries link against are copied in and their load paths rewritten, so the
# result runs on a Mac without Homebrew or the Vulkan SDK. The archive is a .tar.gz because zip-based
# transports (GitHub artifacts) drop the executable bits.
set -euo pipefail
shopt -s nullglob

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
ROOT_DIR="$(cd "$SCRIPT_DIR/../.." && pwd)"
BIN_DIR="$ROOT_DIR/Binaries/Mac/Distribution"
OUTPUT_DIR="${1:-$ROOT_DIR/PackagedBuilds/EditorPreview}"

step() { printf '==> %s\n' "$1"; }
warn() { printf 'WARNING: %s\n' "$1" >&2; }

version_component() {
    sed -nE "s/^set\(HYP_VERSION_$1[[:space:]]+([0-9]+)\).*/\1/p" "$ROOT_DIR/Source/CMakeLists.txt" | head -n 1
}

VERSION="$(version_component MAJOR).$(version_component MINOR).$(version_component PATCH)"
STAGE_NAME="Hyperion-Editor-$VERSION-macos-arm64"
STAGE_DIR="$OUTPUT_DIR/$STAGE_NAME"

if [[ ! -e "$BIN_DIR/Hyperion.Editor" ]]; then
    echo "Hyperion.Editor not found in $BIN_DIR. Run 'Tools/Scripts/BuildHyperion.sh distribution ninja regenerate' first." >&2
    exit 1
fi

step "Packaging Hyperion editor preview $VERSION (macOS arm64)"

rm -rf "$STAGE_DIR"
mkdir -p "$STAGE_DIR"

# --- Managed editor ---------------------------------------------------------------------------------------------------

# self-contained, so running the editor doesn't need a .NET install (same as the Windows package)
step "Publishing self-contained editor"

EDITOR_PROJECT="$ROOT_DIR/Build/CSharpProjects-Distribution/Hyperion.Editor/Hyperion.Editor.csproj"
if [[ ! -f "$EDITOR_PROJECT" ]]; then
    echo "Editor project not found at $EDITOR_PROJECT" >&2
    exit 1
fi

PUBLISH_ROOT="$OUTPUT_DIR/publish-intermediate"
rm -rf "$PUBLISH_ROOT"

env -u MSBuildSdksPath dotnet publish "$EDITOR_PROJECT" --configuration Release --runtime osx-arm64 --self-contained true --disable-build-servers \
    -p:DebugType=none "-p:OutputPath=$PUBLISH_ROOT/build/" --output "$PUBLISH_ROOT/publish"

cp -R "$PUBLISH_ROOT/publish/." "$STAGE_DIR/"
rm -rf "$PUBLISH_ROOT"

# script projects compile against these; at runtime the published copies are used
mkdir -p "$STAGE_DIR/ref"
for name in Hyperion.NET.Shared.dll Hyperion.NET.Runtime.dll Hyperion.NET.Interop.dll; do
    cp "$BIN_DIR/$name" "$STAGE_DIR/ref/"
done

# --- Native binaries --------------------------------------------------------------------------------------------------

step "Staging native libraries"

is_macho() { file -b "$1" | grep -q "Mach-O"; }

for path in "$BIN_DIR"/*.dylib; do
    cp "$path" "$STAGE_DIR/"
    # the debug map names the build machine's object files
    strip -S "$STAGE_DIR/$(basename "$path")"
done

# --- Bundle non-system libraries --------------------------------------------------------------------------------------

step "Bundling library dependencies"

macho_files() {
    find "$STAGE_DIR" -type f \( -name "*.dylib" -o -perm -u+x \) -not -path "*/ref/*" | while read -r candidate; do
        if is_macho "$candidate"; then echo "$candidate"; fi
    done
}

# install_name_tool warns on every edit that the signature is now invalid; everything is re-signed below
relink() {
    install_name_tool "$@" 2> >(grep -v "invalidate the code signature" >&2)
}

# otool prints a header line per architecture of a universal binary, and lists a dylib's own install name first
linked_libraries() {
    local own_id
    own_id="$(otool -D "$1" | grep -v ':$' | head -n 1)"
    otool -L "$1" | grep -v ':$' | awk '{print $1}' | sort -u | grep -v -x -F "${own_id:-/dev/null}" || true
}

external_dependencies() {
    linked_libraries "$1" | grep -Ev '^(/usr/lib/|/System/|@rpath/|@loader_path/|@executable_path/)' || true
}

rpath_dependencies() {
    linked_libraries "$1" | sed -n 's#^@rpath/\([^/]*\)$#\1#p'
}

all_rpaths() {
    otool -l "$1" | awk '/LC_RPATH/ { getline; getline; print $2 }'
}

absolute_rpaths() {
    all_rpaths "$1" | grep -v '^@' || true
}

# where each bundled library was copied from, so its @loader_path rpaths (common in Homebrew) can be followed
ORIGINS_DIR="$(mktemp -d)"
trap 'rm -rf "$ORIGINS_DIR"' EXIT

original_dir() {
    local origin_file="$ORIGINS_DIR/$(basename "$1")"
    if [[ -f "$origin_file" ]]; then cat "$origin_file"; else echo "$BIN_DIR"; fi
}

rpath_search_dirs() {
    local rpath
    for rpath in $(all_rpaths "$1"); do
        case "$rpath" in
            @loader_path*) echo "$(original_dir "$1")${rpath#@loader_path}" ;;
            @*) ;;
            *) echo "$rpath" ;;
        esac
    done
}

stage_library() {
    echo "    $2  (from $1)"
    cp -L "$1" "$STAGE_DIR/$2"
    chmod u+w "$STAGE_DIR/$2"
    dirname "$1" > "$ORIGINS_DIR/$2"
}

# copying a library in can pull in more, so repeat until nothing external is left
for pass in 1 2 3 4 5 6; do
    changed=0

    while read -r binary; do
        for dependency in $(external_dependencies "$binary"); do
            dependency_name="$(basename "$dependency")"

            if [[ ! -e "$STAGE_DIR/$dependency_name" ]]; then
                if [[ ! -e "$dependency" ]]; then
                    warn "$(basename "$binary") links $dependency, which does not exist on this machine"
                    continue
                fi
                stage_library "$dependency" "$dependency_name"
                changed=1
            fi

            relink -change "$dependency" "@rpath/$dependency_name" "$binary"
        done

        # @rpath/ libraries already resolve next to the binary, but have to be found through the build machine's rpaths first
        for dependency_name in $(rpath_dependencies "$binary"); do
            [[ -e "$STAGE_DIR/$dependency_name" ]] && continue

            for rpath in $(rpath_search_dirs "$binary"); do
                if [[ -e "$rpath/$dependency_name" ]]; then
                    stage_library "$rpath/$dependency_name" "$dependency_name"
                    changed=1
                    break
                fi
            done

            [[ -e "$STAGE_DIR/$dependency_name" ]] || warn "$(basename "$binary") links @rpath/$dependency_name, which is not in any of its rpaths"
        done
    done < <(macho_files)

    [[ $changed -eq 1 ]] || break
done

while read -r binary; do
    # the package is arm64 only, so drop the x86_64 half of universal libraries
    if [[ "$(lipo -archs "$binary")" == *" "* ]]; then
        lipo -thin arm64 "$binary" -output "$binary.thin" && mv "$binary.thin" "$binary"
    fi

    if [[ "$binary" == *.dylib ]]; then
        relink -id "@rpath/$(basename "$binary")" "$binary"
    fi

    # resolve @rpath/ next to the binary itself, and drop the build machine's absolute search paths
    for rpath in $(absolute_rpaths "$binary"); do
        relink -delete_rpath "$rpath" "$binary" 2>/dev/null || true
    done
    relink -add_rpath "@loader_path" "$binary" 2>/dev/null || true
done < <(macho_files)

# --- Vulkan driver ----------------------------------------------------------------------------------------------------

# The Vulkan loader finds MoltenVK through a driver manifest; point it at the bundled copy from the launcher.
# prefer the MoltenVK that ships with the Vulkan install the engine was built against; the checked-in copy lags behind
MOLTENVK_SOURCE=""
for candidate in "$STAGE_DIR/libMoltenVK.dylib" "${VULKAN_SDK:-/nonexistent}/lib/libMoltenVK.dylib" /usr/local/lib/libMoltenVK.dylib /opt/homebrew/lib/libMoltenVK.dylib "$ROOT_DIR/External/ThirdParty/Binaries/Mac/Release/libMoltenVK.dylib"; do
    if [[ -e "$candidate" ]]; then MOLTENVK_SOURCE="$candidate"; break; fi
done

if [[ -n "$MOLTENVK_SOURCE" ]]; then
    [[ -e "$STAGE_DIR/libMoltenVK.dylib" ]] || cp -L "$MOLTENVK_SOURCE" "$STAGE_DIR/libMoltenVK.dylib"
    mkdir -p "$STAGE_DIR/vulkan/icd.d"
    cat > "$STAGE_DIR/vulkan/icd.d/MoltenVK_icd.json" <<'EOF'
{
    "file_format_version": "1.0.0",
    "ICD": {
        "library_path": "../../libMoltenVK.dylib",
        "api_version": "1.2.0",
        "is_portability_driver": true
    }
}
EOF
else
    warn "libMoltenVK.dylib not found; the package will rely on a Vulkan driver installed on the user's machine"
fi

cat > "$STAGE_DIR/Hyperion Editor.command" <<'EOF'
#!/bin/bash
# Double-click to start the editor.
DIR="$(cd "$(dirname "$0")" && pwd)"
if [[ -f "$DIR/vulkan/icd.d/MoltenVK_icd.json" ]]; then
    export VK_DRIVER_FILES="$DIR/vulkan/icd.d/MoltenVK_icd.json"
    export VK_ICD_FILENAMES="$VK_DRIVER_FILES"
fi
cd "$DIR"
exec "$DIR/Hyperion.Editor" "$@"
EOF
chmod +x "$STAGE_DIR/Hyperion Editor.command"

# --- Re-sign ----------------------------------------------------------------------------------------------------------

# Apple Silicon refuses to run a binary whose signature no longer matches, and install_name_tool invalidates it.
# This is an ad-hoc signature: enough to run, but Gatekeeper still treats the download as unidentified.
step "Ad-hoc signing"
while read -r binary; do
    codesign --force --sign - "$binary" 2>/dev/null || warn "codesign failed for $(basename "$binary")"
done < <(macho_files)

# --- Config -----------------------------------------------------------------------------------------------------------

step "Staging config"

mkdir -p "$STAGE_DIR/Config"

# mirrors Source/Sample/CopyPlatformConfigs.cmake for PLATFORM=Mac
for config_file in "$ROOT_DIR"/Config/*; do
    [[ -f "$config_file" ]] || continue
    name="$(basename "$config_file")"

    case "$name" in
        *.IOS.json|*.Android.json|*.Windows.json|*.Mac.json|*.Linux.json|*.Web.json) continue ;;
        *.json)
            mac_variant="$ROOT_DIR/Config/${name%.json}.Mac.json"
            if [[ -f "$mac_variant" ]]; then cp "$mac_variant" "$STAGE_DIR/Config/$name"; else cp "$config_file" "$STAGE_DIR/Config/$name"; fi
            ;;
        *) cp "$config_file" "$STAGE_DIR/Config/$name" ;;
    esac
done

# basedir points at the source tree, trace/cache servers are dev-only localhost endpoints
GLOBAL_CONFIG="$STAGE_DIR/Config/GlobalConfig.json"
if [[ -f "$GLOBAL_CONFIG" ]]; then
    sed -i '' -E 's# ?--(basedir|traceserver|cacheserver)=[^ "]*##g; s#("Args"[[:space:]]*:[[:space:]]*")#\1--basedir=./ #' "$GLOBAL_CONFIG"
fi

# --- Content, shaders, scripts ----------------------------------------------------------------------------------------

copy_tracked() {
    local count=0
    while IFS= read -r -d '' relative_path; do
        [[ -f "$ROOT_DIR/$relative_path" ]] || continue
        mkdir -p "$STAGE_DIR/$(dirname "$relative_path")"
        cp "$ROOT_DIR/$relative_path" "$STAGE_DIR/$relative_path"
        count=$((count + 1))
    done < <(cd "$ROOT_DIR" && git -c core.quotepath=off ls-files -z -- "$@")
    echo "    $count files"
}

step "Staging engine and editor content"
copy_tracked Content/Engine Content/Editor

step "Staging shader sources"
copy_tracked Source/Shaders

step "Staging project templates"
copy_tracked Source/Templates Source/Engine/DotNET/Sdk

if [[ -d "$ROOT_DIR/Data/Scripts/Strata" ]]; then
    step "Staging generated Strata bindings"
    mkdir -p "$STAGE_DIR/Data/Scripts"
    cp -R "$ROOT_DIR/Data/Scripts/Strata" "$STAGE_DIR/Data/Scripts/"
else
    warn "Data/Scripts/Strata not found; Strata scripts won't resolve engine modules"
fi

# --- Legal ------------------------------------------------------------------------------------------------------------

cp "$ROOT_DIR/LICENSE" "$STAGE_DIR/LICENSE"
[[ -f "$ROOT_DIR/THIRD_PARTY_NOTICES.md" ]] && cp "$ROOT_DIR/THIRD_PARTY_NOTICES.md" "$STAGE_DIR/"
[[ -d "$ROOT_DIR/Documentation/ThirdPartyLicenses" ]] && cp -R "$ROOT_DIR/Documentation/ThirdPartyLicenses" "$STAGE_DIR/Licenses"

# --- Checks and archive -----------------------------------------------------------------------------------------------

step "Checking for paths that only exist on the build machine"
while read -r binary; do
    leftover="$(external_dependencies "$binary")"
    if [[ -n "$leftover" ]]; then
        warn "$(basename "$binary") still links: $(echo $leftover)"
    fi

    for dependency_name in $(rpath_dependencies "$binary"); do
        [[ -e "$STAGE_DIR/$dependency_name" ]] || warn "$(basename "$binary") links @rpath/$dependency_name, which is not in the package"
    done
done < <(macho_files)

if grep -q -a -F "$ROOT_DIR" "$STAGE_DIR"/libhyperion*.dylib 2>/dev/null; then
    warn "the engine library has the source tree path ($ROOT_DIR) baked in; build with the 'distribution' argument"
fi

step "Creating $STAGE_NAME.tar.gz"
tar -czf "$OUTPUT_DIR/$STAGE_NAME.tar.gz" -C "$OUTPUT_DIR" "$STAGE_NAME"

echo
echo "Staged $STAGE_NAME ($(du -sh "$STAGE_DIR" | cut -f1)) at $STAGE_DIR"
