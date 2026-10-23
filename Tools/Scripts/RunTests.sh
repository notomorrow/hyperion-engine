#!/bin/bash

# Runs every test project under Tests/ (any .csproj with <IsTestProject>true</IsTestProject>).
#
#   ./Tools/Scripts/RunTests.sh                      run all tests
#   ./Tools/Scripts/RunTests.sh Native               run tests whose full name contains "Native" (class, method or namespace)
#   ./Tools/Scripts/RunTests.sh "Name=SavedAssets_LoadInNewSession"
#                                                    a filter containing = ~ ! | & is passed to dotnet test --filter as is
#   ./Tools/Scripts/RunTests.sh Native -c Release    anything after the filter goes to dotnet test (use "" for no filter)

SCRIPT_DIR=`dirname -- "$( readlink -f -- "$0"; )"`
TESTS_DIR=`readlink -f -- "$SCRIPT_DIR/../../Tests"`

if [[ -n "$MSBuildSdksPath" && ! -d "$MSBuildSdksPath" ]]; then
    echo "Ignoring MSBuildSdksPath ($MSBuildSdksPath doesn't exist)"
    unset MSBuildSdksPath
fi

FILTER="$1"
shift

TEST_ARGS=()

if [[ -n "$FILTER" ]]; then
    if [[ ! "$FILTER" =~ [=~!\|\&] ]]; then
        FILTER="FullyQualifiedName~$FILTER"
    fi

    TEST_ARGS+=(--filter "$FILTER")
fi

TEST_ARGS+=("$@")

PROJECTS=()

while IFS= read -r -d '' PROJECT; do
    if grep -Eq '<IsTestProject>[[:space:]]*true[[:space:]]*</IsTestProject>' "$PROJECT"; then
        PROJECTS+=("$PROJECT")
    fi
done < <(find "$TESTS_DIR" -name '*.csproj' -not -path '*/bin/*' -not -path '*/obj/*' -not -path '*/Fixtures/*' -print0 | sort -z)

if [[ ${#PROJECTS[@]} -eq 0 ]]; then
    echo "No test projects found under $TESTS_DIR" >&2
    exit 1
fi

FAILED=()

for PROJECT in "${PROJECTS[@]}"; do
    echo "==> ${PROJECT#$TESTS_DIR/}"

    if ! dotnet test "$PROJECT" "${TEST_ARGS[@]}"; then
        FAILED+=("$(basename "$PROJECT")")
    fi
done

if [[ ${#FAILED[@]} -gt 0 ]]; then
    echo "Failed: ${FAILED[*]}" >&2
    exit 1
fi

echo "All test projects passed"
