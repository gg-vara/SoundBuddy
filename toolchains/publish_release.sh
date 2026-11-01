#!/bin/bash

# Builds SoundBuddy in Release, packages it with Velopack, and publishes it as a GitHub release,
# from which installed SoundBuddy instances update themselves.
#
# Requirements :
#   - the build prerequisites (see README.md), run from the MSYS2 CLANG64 shell on Windows
#   - .NET SDK and Velopack CLI, same version as VELOPACK_VERSION in CMakeLists.txt :
#       dotnet tool install -g vpk --version <VELOPACK_VERSION>
#   - GitHub CLI, logged in (gh auth login)
#
# Usage : toolchains/publish_release.sh [--draft]
#   Windows and macOS packages are published into the same release (vX.Y.Z) : run the script on both.

set -euo pipefail

cd "$(dirname "$0")/.."

publish_flag="--publish"
if [[ "${1:-}" == "--draft" ]]; then
    publish_flag=""
fi

#
# release informations, from CMakeLists.txt
#

cmake_value() {
    grep -o "$1 [^)]*" CMakeLists.txt | head -1 | cut -d' ' -f2- | tr -d '"'
}

app_name=$(cmake_value 'SET(GITHUB_REPO_NAME')
repo_owner=$(cmake_value 'SET(GITHUB_REPO_OWNER')
publisher=$(cmake_value 'SET(APP_PUBLISHER_ORG')
velopack_version=$(cmake_value 'SET(VELOPACK_VERSION')
app_version=$(grep -o 'project([^)]*' CMakeLists.txt | grep -o '[0-9]\+\.[0-9]\+\.[0-9]\+' | head -1)
repo_url="https://github.com/$repo_owner/$app_name"
tag="v$app_version"

if [[ -z "$app_name" || -z "$app_version" || -z "$velopack_version" ]]; then
    echo "Error: could not read app name / version / Velopack version from CMakeLists.txt"
    exit 1
fi

echo "Publishing $app_name $tag to $repo_url"

#
# tools
#

if ! command -v vpk &> /dev/null; then
    echo "Error: Velopack CLI not found. Install it with : dotnet tool install -g vpk --version $velopack_version"
    exit 1
fi

if ! vpk -h | grep -q "Velopack CLI $velopack_version"; then
    echo "Error: Velopack CLI version differs from the library version ($velopack_version) used by $app_name"
    exit 1
fi

if ! gh auth status &> /dev/null; then
    echo "Error: not logged in to GitHub CLI. Please run : gh auth login"
    exit 1
fi

token=$(gh auth token)

#
# platform
#

build_dir=build_release
releases_dir=$build_dir/releases
pack_args=()

case "$(uname -s)" in
    MINGW*|MSYS*)
        toolchain=toolchains/msys2.cmake
        channel=win
        pack_dir=$build_dir/out
        main_exe=$app_name.exe
        pack_args+=(--runtime win-x64 --icon resources/icons/package.ico)

        # sign installer and updater too, with the same certificate as the executable
        if [[ -f certs/all.pfx ]]; then
            pack_args+=(--signParams "/f certs/all.pfx /tr http://timestamp.acs.microsoft.com /td SHA256 /fd SHA256")
        fi
        ;;
    Darwin)
        toolchain=toolchains/osx.cmake
        channel=osx
        pack_dir=$build_dir/out/$app_name.app
        main_exe=$app_name
        mac_arch=$(uname -m)
        pack_args+=(--runtime "osx-${mac_arch/x86_64/x64}" --icon resources/icons/package.icns)

        if [[ -n "${APPLE_CERT_APP_CODE_SIGN_IDENTITY:-}" ]]; then
            pack_args+=(--signAppIdentity "$APPLE_CERT_APP_CODE_SIGN_IDENTITY")
        fi
        ;;
    *)
        echo "Error: unsupported platform $(uname -s)"
        exit 1
        ;;
esac

#
# build
#

cmake -GNinja -B $build_dir -S . -DCMAKE_BUILD_TYPE=Release -DCMAKE_TOOLCHAIN_FILE=$toolchain
cmake --build $build_dir

#
# package
#

# previous release, to generate delta updates (none for the very first release)
rm -rf "$releases_dir"
vpk download github --repoUrl "$repo_url" --token "$token" --channel $channel --outputDir "$releases_dir" \
    || echo "No previous release found, only full packages will be generated"

vpk pack \
    --packId "$app_name" \
    --packVersion "$app_version" \
    --packTitle "$app_name" \
    --packAuthors "$publisher" \
    --packDir "$pack_dir" \
    --mainExe "$main_exe" \
    --channel $channel \
    --outputDir "$releases_dir" \
    "${pack_args[@]}"

#
# publish
#

# --merge : the other platform may already have created the release
vpk upload github \
    --repoUrl "$repo_url" \
    --token "$token" \
    --channel $channel \
    --outputDir "$releases_dir" \
    --tag "$tag" \
    --releaseName "$app_name $tag" \
    --merge \
    $publish_flag

echo "Published $app_name $tag ($channel)"
