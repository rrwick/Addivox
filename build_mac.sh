#!/usr/bin/env bash

set -u
set -o pipefail

# Build Addivox release artifacts for macOS, iOS devices, iOS simulators, Xcode
# archives, and the CLI.
#
# How to run:
#
#   sudo ./build_mac.sh
#   sudo ./build_mac.sh --clean
#   sudo ./build_mac.sh --install
#
# Use sudo because the Xcode archive action currently runs an install-style
# postprocessing step that tries to set archived product ownership to root:admin.
# The non-archive build steps do not need sudo, but this script always includes
# archives so one sudo invocation is the simplest reliable path.
#
# Expect the full build to take a few minutes. All logs go under:
#
#   build/mac-release/logs/
#
# Main outputs are copied into build/dist/full and build/dist/demo:
#
#   build/dist/full/macos/Addivox.app
#     Standalone macOS app bundle.
#
#   build/dist/full/macos/Addivox.component
#     Audio Unit v2 plugin bundle for macOS DAWs.
#
#   build/dist/full/macos/Addivox.vst
#     VST2 plugin bundle for macOS hosts that still support VST2.
#
#   build/dist/full/macos/Addivox.vst3
#     VST3 plugin bundle for macOS hosts.
#
#   build/dist/full/macos/Addivox.clap
#     CLAP plugin bundle for macOS hosts.
#
#   build/dist/full/macos/Addivox.aaxplugin
#     AAX plugin bundle for Pro Tools. This usually needs Avid-specific signing
#     and packaging before it is useful for external distribution.
#
#   build/dist/ios-device/
#     iOS device Release products, including Addivox.app,
#     AddivoxAppExtension.appex, and AUv3Framework.framework where produced by
#     the Xcode schemes.
#
#   build/dist/ios-simulator/
#     iOS simulator Release products for local testing.
#
#   build/dist/archives/macos/macOS-APP with AUv3.xcarchive
#     macOS app archive with the AUv3 extension included, suitable for later Mac
#     App Store export or Developer ID signing/notarization work.
#
#   build/dist/archives/ios/iOS-APP with AUv3.xcarchive
#     iOS app archive with the AUv3 extension included, suitable for later App
#     Store/TestFlight export work.
#
#   build/dist/cli/addivox
#     Release CLI executable.
#
# Customer distribution zips are written to build/:
#
#   build/Addivox_v1.0.0_macOS.zip
#   build/AddivoxDemo_v1.0.0_macOS.zip
#
# Optional local plugin install:
#
#   sudo ./build_mac.sh --install
#
# This copies macOS plugin bundles from build/dist/full/macos and
# build/dist/demo/macos into the per-user plugin
# folders under ~/Library/Audio/Plug-Ins for local DAW testing. AAX is not copied
# because it normally lives under /Library/Application Support/Avid and should be
# installed through a dedicated signed installer.
#
# This intentionally builds unsigned artifacts for now. Distribution TODOs once you have an
# Apple Developer Program account:
#
#   1. Set DEVELOPMENT_TEAM in Addivox/config/*.xcconfig or pass it through xcodebuild.
#   2. Remove CODE_SIGNING_ALLOWED=NO for app/AUv3 archive/export steps.
#   3. Add ExportOptions.plist files for:
#        - macOS direct distribution / Developer ID
#        - iOS App Store / TestFlight
#   4. Run xcodebuild -exportArchive for app archives.
#   5. Sign standalone plugin bundles with Developer ID Application.
#   6. Notarize macOS app/plugin zips with notarytool, then staple tickets where supported.
#   7. Package final customer downloads as signed/notarized .pkg or .zip files.

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_DIR="${ROOT_DIR}/Addivox"
MAC_PROJECT="${PROJECT_DIR}/projects/Addivox-macOS.xcodeproj"
IOS_PROJECT="${PROJECT_DIR}/projects/Addivox-iOS.xcodeproj"
MACOS_INSTALLATION_DOC="${ROOT_DIR}/docs/docs/installation_macos.md"

BUILD_ROOT="${ROOT_DIR}/build"
WORK_ROOT="${BUILD_ROOT}/mac-release"
DIST_ROOT="${BUILD_ROOT}/dist"
ACTIVE_DIST_ROOT="${DIST_ROOT}/full"
LOG_ROOT="${WORK_ROOT}/logs"
ARCHIVE_ROOT="${WORK_ROOT}/archives"
XCODE_DERIVED_ROOT="${WORK_ROOT}/xcode-derived"
CMAKE_BUILD_DIR="${WORK_ROOT}/cli-cmake"
PACKAGE_ROOT="${WORK_ROOT}/packages"

CONFIGURATION="Release"
INSTALL_PLUGINS=0
CLEAN=0
BUILD_VARIANT="full"
BUILD_BINARY_NAME="Addivox"
BUILD_CLI_NAME="addivox"
ADDIVOX_DEMO_VALUE=0
PLUG_VERSION=""

MAC_SCHEMES=(
  "All macOS"
  "macOS-APP"
  "macOS-APP with AUv3"
  "macOS-AUv2"
  "macOS-AUv3"
  "macOS-AUv3Framework"
  "macOS-VST2"
  "macOS-VST3"
  "macOS-CLAP"
  "macOS-AAX"
)

IOS_SCHEMES=(
  "iOS-APP with AUv3"
  "iOS-AUv3"
  "iOS-AUv3Framework"
)

ARCHIVE_SCHEMES_MAC=(
  "macOS-APP with AUv3"
)

ARCHIVE_SCHEMES_IOS=(
  "iOS-APP with AUv3"
)

OK_STEPS=()
FAILED_STEPS=()
COPIED_ARTIFACTS=()
PACKAGED_ARTIFACTS=()

usage() {
  cat <<EOF
Usage: $0 [options]

Options:
  --install        Copy built macOS plugin bundles to local user plugin folders for testing.
  --clean          Remove ${BUILD_ROOT} before building.
  --help           Show this help.

Archives are always built. Run this script with sudo because Xcode's archive
postprocessing currently needs permission to set archived product ownership.
EOF
}

while [[ $# -gt 0 ]]; do
  case "$1" in
    --install)
      INSTALL_PLUGINS=1
      ;;
    --clean)
      CLEAN=1
      ;;
    --help|-h)
      usage
      exit 0
      ;;
    *)
      echo "Unknown option: $1" >&2
      usage >&2
      exit 2
      ;;
  esac
  shift
done

log() {
  printf '\n==> %s\n' "$*"
}

record_ok() {
  OK_STEPS+=("$1")
}

record_fail() {
  FAILED_STEPS+=("$1")
}

run_step() {
  local name="$1"
  local log_file="$2"
  shift 2

  log "${name}"
  mkdir -p "$(dirname "${log_file}")"

  if "$@" 2>&1 | tee "${log_file}"; then
    record_ok "${name}"
    return 0
  fi

  record_fail "${name} (see ${log_file})"
  return 1
}

require_tool() {
  if ! command -v "$1" >/dev/null 2>&1; then
    echo "Required tool not found: $1" >&2
    exit 1
  fi
}

read_plug_version() {
  local config_header="${PROJECT_DIR}/config.h"
  local version

  version="$(sed -E -n 's/^[[:space:]]*#define[[:space:]]+PLUG_VERSION_STR[[:space:]]+"([^"]+)".*$/\1/p' "${config_header}" | head -n 1)"
  if [[ -z "${version}" ]]; then
    echo "Could not read PLUG_VERSION_STR from ${config_header}" >&2
    exit 1
  fi

  PLUG_VERSION="${version}"
}

copy_bundle() {
  local source_path="$1"
  local relative_path="$2"
  local destination_path="${ACTIVE_DIST_ROOT}/${relative_path}"

  mkdir -p "$(dirname "${destination_path}")"
  rm -rf "${destination_path}"
  cp -R "${source_path}" "${destination_path}"
  record_copied_artifact "${destination_path}"
}

copy_named_artifact_from_products_as() {
  local platform_label="$1"
  local products_dir="$2"
  local source_name="$3"
  local destination_name="$4"

  [[ -d "${products_dir}" ]] || return 0

  local artifact_path="${products_dir}/${source_name}"
  if [[ -e "${artifact_path}" ]]; then
    copy_bundle "${artifact_path}" "${platform_label}/${destination_name}"
  else
    record_fail "Copy ${platform_label}/${destination_name} (${artifact_path} not found)"
  fi
}

copy_file() {
  local source_path="$1"
  local relative_path="$2"
  local destination_path="${ACTIVE_DIST_ROOT}/${relative_path}"

  mkdir -p "$(dirname "${destination_path}")"
  rm -f "${destination_path}"
  cp "${source_path}" "${destination_path}"
  record_copied_artifact "${destination_path}"
}

record_copied_artifact() {
  local artifact="$1"

  if [[ "${#COPIED_ARTIFACTS[@]}" -gt 0 ]]; then
    for existing in "${COPIED_ARTIFACTS[@]}"; do
      [[ "${existing}" == "${artifact}" ]] && return 0
    done
  fi

  COPIED_ARTIFACTS+=("${artifact}")
}

record_packaged_artifact() {
  local artifact="$1"

  if [[ "${#PACKAGED_ARTIFACTS[@]}" -gt 0 ]]; then
    for existing in "${PACKAGED_ARTIFACTS[@]}"; do
      [[ "${existing}" == "${artifact}" ]] && return 0
    done
  fi

  PACKAGED_ARTIFACTS+=("${artifact}")
}

copy_packaged_readme() {
  local destination_path="$1"

  sed -E 's/\[([^][]+)\]\([^)]+\)/\1/g' "${MACOS_INSTALLATION_DOC}" > "${destination_path}"
}

copy_named_artifacts_from_products() {
  local platform_label="$1"
  local products_dir="$2"
  shift 2

  [[ -d "${products_dir}" ]] || return 0

  local artifact_name
  for artifact_name in "$@"; do
    local artifact_path="${products_dir}/${artifact_name}"
    if [[ -e "${artifact_path}" ]]; then
      copy_bundle "${artifact_path}" "${platform_label}/${artifact_name}"
    else
      record_fail "Copy ${platform_label}/${artifact_name} (${artifact_path} not found)"
    fi
  done
}

normalize_embedded_macos_auv3() {
  local app_path="${ACTIVE_DIST_ROOT}/macos/${BUILD_BINARY_NAME}.app"
  local plugins_dir="${app_path}/Contents/PlugIns"
  local source_appex="${plugins_dir}/Addivox.appex"
  local destination_appex="${plugins_dir}/${BUILD_BINARY_NAME}.appex"
  local source_executable="${destination_appex}/Contents/MacOS/Addivox"
  local destination_executable="${destination_appex}/Contents/MacOS/${BUILD_BINARY_NAME}"

  if [[ ! -d "${source_appex}" && ! -d "${destination_appex}" ]]; then
    record_fail "Normalize embedded macOS AUv3 (${source_appex} not found)"
    return 0
  fi

  if [[ -d "${source_appex}" && "${source_appex}" != "${destination_appex}" ]]; then
    rm -rf "${destination_appex}"
    mv "${source_appex}" "${destination_appex}"
  fi

  if [[ -f "${source_executable}" && "${source_executable}" != "${destination_executable}" ]]; then
    rm -f "${destination_executable}"
    mv "${source_executable}" "${destination_executable}"
  fi

  if [[ ! -f "${destination_executable}" ]]; then
    record_fail "Normalize embedded macOS AUv3 (${destination_executable} not found)"
  fi
}

copy_macos_scheme_artifacts() {
  local scheme="$1"
  local products_dir="$2"

  case "${scheme}" in
    "macOS-APP")
      copy_named_artifacts_from_products "macos" "${products_dir}" "${BUILD_BINARY_NAME}.app"
      ;;
    "macOS-APP with AUv3")
      copy_named_artifacts_from_products "macos" "${products_dir}" "${BUILD_BINARY_NAME}.app"
      normalize_embedded_macos_auv3
      ;;
    "macOS-AUv2")
      copy_named_artifacts_from_products "macos" "${products_dir}" "${BUILD_BINARY_NAME}.component"
      ;;
    "macOS-VST2")
      copy_named_artifacts_from_products "macos" "${products_dir}" "${BUILD_BINARY_NAME}.vst"
      ;;
    "macOS-VST3")
      copy_named_artifacts_from_products "macos" "${products_dir}" "${BUILD_BINARY_NAME}.vst3"
      ;;
    "macOS-CLAP")
      copy_named_artifacts_from_products "macos" "${products_dir}" "${BUILD_BINARY_NAME}.clap"
      ;;
    "macOS-AAX")
      copy_named_artifacts_from_products "macos" "${products_dir}" "${BUILD_BINARY_NAME}.aaxplugin"
      ;;
  esac
}

copy_ios_scheme_artifacts() {
  local platform_label="$1"
  local scheme="$2"
  local products_dir="$3"

  case "${scheme}" in
    "iOS-APP with AUv3")
      copy_named_artifacts_from_products "${platform_label}" "${products_dir}" "${BUILD_BINARY_NAME}.app" "AUv3Framework.framework"
      copy_named_artifact_from_products_as "${platform_label}" "${products_dir}" "AddivoxAppExtension.appex" "${BUILD_BINARY_NAME}AppExtension.appex"
      ;;
    "iOS-AUv3")
      copy_named_artifact_from_products_as "${platform_label}" "${products_dir}" "AddivoxAppExtension.appex" "${BUILD_BINARY_NAME}AppExtension.appex"
      ;;
    "iOS-AUv3Framework")
      copy_named_artifacts_from_products "${platform_label}" "${products_dir}" "AUv3Framework.framework"
      ;;
  esac
}

install_plugin_bundle() {
  local bundle_path="$1"
  local bundle_name
  bundle_name="$(basename "${bundle_path}")"

  case "${bundle_name}" in
    *.component)
      install_bundle_to "${bundle_path}" "${HOME}/Library/Audio/Plug-Ins/Components/${bundle_name}"
      ;;
    *.vst)
      install_bundle_to "${bundle_path}" "${HOME}/Library/Audio/Plug-Ins/VST/${bundle_name}"
      ;;
    *.vst3)
      install_bundle_to "${bundle_path}" "${HOME}/Library/Audio/Plug-Ins/VST3/${bundle_name}"
      ;;
    *.clap)
      install_bundle_to "${bundle_path}" "${HOME}/Library/Audio/Plug-Ins/CLAP/${bundle_name}"
      ;;
    *.aaxplugin)
      echo "Skipping AAX install for ${bundle_name}; AAX normally installs under /Library/Application Support/Avid/Audio/Plug-Ins and may require a dedicated signed installer."
      ;;
  esac
}

install_bundle_to() {
  local source_path="$1"
  local destination_path="$2"

  mkdir -p "$(dirname "${destination_path}")"
  rm -rf "${destination_path}"
  cp -R "${source_path}" "${destination_path}"
  echo "Installed ${destination_path}"
}

install_plugins() {
  log "Installing macOS plugin bundles for local testing"

  while IFS= read -r -d '' bundle; do
    install_plugin_bundle "${bundle}"
  done < <(find "${DIST_ROOT}/full/macos" "${DIST_ROOT}/demo/macos" -type d \( \
      -name "*.component" -o \
      -name "*.vst" -o \
      -name "*.vst3" -o \
      -name "*.clap" -o \
      -name "*.aaxplugin" \
    \) -prune -print0 2>/dev/null)
}

xcode_build() {
  local project_path="$1"
  local scheme="$2"
  local sdk="$3"
  local destination="$4"
  local derived_data="$5"
  local log_file="$6"
  local name="$7"

  run_step "${name}" "${log_file}" \
    env "ADDIVOX_DEMO=${ADDIVOX_DEMO_VALUE}" \
      xcodebuild \
      -project "${project_path}" \
      -scheme "${scheme}" \
      -configuration "${CONFIGURATION}" \
      -sdk "${sdk}" \
      -destination "${destination}" \
      -derivedDataPath "${derived_data}" \
      SYMROOT="${derived_data}/Products" \
      OBJROOT="${derived_data}/Intermediates" \
      DSTROOT="${derived_data}/DSTROOT" \
      ADDIVOX_DEMO="${ADDIVOX_DEMO_VALUE}" \
      BINARY_NAME="${BUILD_BINARY_NAME}" \
      GCC_PREPROCESSOR_DEFINITIONS="\$(inherited) ADDIVOX_DEMO=${ADDIVOX_DEMO_VALUE}" \
      DEPLOYMENT_LOCATION=NO \
      SKIP_INSTALL=NO \
      CODE_SIGNING_ALLOWED=NO \
      CODE_SIGNING_REQUIRED=NO \
      build
}

xcode_archive() {
  local project_path="$1"
  local scheme="$2"
  local destination="$3"
  local archive_path="$4"
  local derived_data="$5"
  local log_file="$6"
  local name="$7"

  mkdir -p "$(dirname "${archive_path}")"

  run_step "${name}" "${log_file}" \
    env "ADDIVOX_DEMO=${ADDIVOX_DEMO_VALUE}" \
      xcodebuild \
      -project "${project_path}" \
      -scheme "${scheme}" \
      -configuration "${CONFIGURATION}" \
      -destination "${destination}" \
      -derivedDataPath "${derived_data}" \
      -archivePath "${archive_path}" \
      ADDIVOX_DEMO="${ADDIVOX_DEMO_VALUE}" \
      BINARY_NAME="${BUILD_BINARY_NAME}" \
      GCC_PREPROCESSOR_DEFINITIONS="\$(inherited) ADDIVOX_DEMO=${ADDIVOX_DEMO_VALUE}" \
      SKIP_INSTALL=NO \
      DEPLOYMENT_POSTPROCESSING=NO \
      STRIP_INSTALLED_PRODUCT=NO \
      CODE_SIGNING_ALLOWED=NO \
      CODE_SIGNING_REQUIRED=NO \
      archive
}

build_macos() {
  local derived_data="${XCODE_DERIVED_ROOT}/${BUILD_VARIANT}/macos"

  for scheme in "${MAC_SCHEMES[@]}"; do
    local safe_scheme="${scheme// /_}"
    if xcode_build \
      "${MAC_PROJECT}" \
      "${scheme}" \
      "macosx" \
      "generic/platform=macOS" \
      "${derived_data}" \
      "${LOG_ROOT}/${BUILD_VARIANT}-macos-${safe_scheme}.log" \
      "${BUILD_VARIANT} macOS ${scheme}"; then
      copy_macos_scheme_artifacts "${scheme}" "${derived_data}/Products/${CONFIGURATION}"
    fi
  done
}

build_ios_devices() {
  local derived_data="${XCODE_DERIVED_ROOT}/${BUILD_VARIANT}/ios-device"

  for scheme in "${IOS_SCHEMES[@]}"; do
    local safe_scheme="${scheme// /_}"
    if xcode_build \
      "${IOS_PROJECT}" \
      "${scheme}" \
      "iphoneos" \
      "generic/platform=iOS" \
      "${derived_data}" \
      "${LOG_ROOT}/${BUILD_VARIANT}-ios-device-${safe_scheme}.log" \
      "${BUILD_VARIANT} iOS device ${scheme}"; then
      copy_ios_scheme_artifacts "ios-device" "${scheme}" "${derived_data}/Products/${CONFIGURATION}-iphoneos"
    fi
  done
}

build_ios_simulators() {
  local derived_data="${XCODE_DERIVED_ROOT}/${BUILD_VARIANT}/ios-simulator"

  for scheme in "${IOS_SCHEMES[@]}"; do
    local safe_scheme="${scheme// /_}"
    if xcode_build \
      "${IOS_PROJECT}" \
      "${scheme}" \
      "iphonesimulator" \
      "generic/platform=iOS Simulator" \
      "${derived_data}" \
      "${LOG_ROOT}/${BUILD_VARIANT}-ios-simulator-${safe_scheme}.log" \
      "${BUILD_VARIANT} iOS simulator ${scheme}"; then
      copy_ios_scheme_artifacts "ios-simulator" "${scheme}" "${derived_data}/Products/${CONFIGURATION}-iphonesimulator"
    fi
  done
}

build_archives() {
  local mac_derived="${XCODE_DERIVED_ROOT}/${BUILD_VARIANT}/macos-archives"
  local ios_derived="${XCODE_DERIVED_ROOT}/${BUILD_VARIANT}/ios-archives"

  for scheme in "${ARCHIVE_SCHEMES_MAC[@]}"; do
    local safe_scheme="${scheme// /_}"
    local archive_path="${ARCHIVE_ROOT}/${BUILD_VARIANT}/macos/${scheme}.xcarchive"

    if xcode_archive \
      "${MAC_PROJECT}" \
      "${scheme}" \
      "generic/platform=macOS" \
      "${archive_path}" \
      "${mac_derived}" \
      "${LOG_ROOT}/${BUILD_VARIANT}-archive-macos-${safe_scheme}.log" \
      "${BUILD_VARIANT} archive macOS ${scheme}"; then
      copy_bundle "${archive_path}" "archives/macos/${scheme}.xcarchive"
    fi
  done

  for scheme in "${ARCHIVE_SCHEMES_IOS[@]}"; do
    local safe_scheme="${scheme// /_}"
    local archive_path="${ARCHIVE_ROOT}/${BUILD_VARIANT}/ios/${scheme}.xcarchive"

    if xcode_archive \
      "${IOS_PROJECT}" \
      "${scheme}" \
      "generic/platform=iOS" \
      "${archive_path}" \
      "${ios_derived}" \
      "${LOG_ROOT}/${BUILD_VARIANT}-archive-ios-${safe_scheme}.log" \
      "${BUILD_VARIANT} archive iOS ${scheme}"; then
      copy_bundle "${archive_path}" "archives/ios/${scheme}.xcarchive"
    fi
  done
}

build_cli() {
  local log_file="${LOG_ROOT}/${BUILD_VARIANT}-cli-cmake.log"
  local cmake_build_dir="${CMAKE_BUILD_DIR}-${BUILD_VARIANT}"

  run_step "Configure CLI with CMake" "${log_file}" \
    cmake \
      -S "${PROJECT_DIR}" \
      -B "${cmake_build_dir}" \
      -DCMAKE_BUILD_TYPE="${CONFIGURATION}" \
      -DADDIVOX_DEMO="$([[ "${ADDIVOX_DEMO_VALUE}" -eq 1 ]] && printf ON || printf OFF)" \
      -DIPLUG2_DIR="${ROOT_DIR}/iPlug2"

  run_step "Build CLI" "${LOG_ROOT}/${BUILD_VARIANT}-cli-build.log" \
    cmake \
      --build "${cmake_build_dir}" \
      --config "${CONFIGURATION}" \
      --target addivox-cli

  local cli_binary="${cmake_build_dir}/${BUILD_CLI_NAME}"
  if [[ -x "${cli_binary}" ]]; then
    copy_file "${cli_binary}" "cli/${BUILD_CLI_NAME}"
  else
    record_fail "Copy CLI artifact (${cli_binary} not found)"
  fi
}

reset_generated_resource_metadata() {
  log "Resetting generated resource metadata to full build defaults"
  (
    cd "${PROJECT_DIR}/projects" &&
      TARGET_BUILD_DIR="${WORK_ROOT}/resource-reset" UNLOCALIZED_RESOURCES_FOLDER_PATH="Resources" ADDIVOX_DEMO=0 python3 ../scripts/prepare_resources-mac.py &&
      ADDIVOX_DEMO=0 python3 ../scripts/prepare_resources-ios.py
  )
}

build_variant() {
  BUILD_VARIANT="$1"
  ADDIVOX_DEMO_VALUE="$2"

  if [[ "${ADDIVOX_DEMO_VALUE}" -eq 1 ]]; then
    BUILD_BINARY_NAME="AddivoxDemo"
    BUILD_CLI_NAME="addivox-demo"
  else
    BUILD_BINARY_NAME="Addivox"
    BUILD_CLI_NAME="addivox"
  fi

  ACTIVE_DIST_ROOT="${DIST_ROOT}/${BUILD_VARIANT}"
  log "Building ${BUILD_VARIANT} artifacts"
  build_macos
  build_ios_devices
  build_ios_simulators
  build_archives
  build_cli
}

package_macos_variant() {
  local variant="$1"
  local binary_name="$2"
  local source_dir="${DIST_ROOT}/${variant}/macos"
  local package_name="${binary_name}_v${PLUG_VERSION}_macOS.zip"
  local package_path="${BUILD_ROOT}/${package_name}"
  local staging_dir="${PACKAGE_ROOT}/${binary_name}"
  local artifact_names=(
    "${binary_name}.app"
    "${binary_name}.component"
    "${binary_name}.vst"
    "${binary_name}.vst3"
    "${binary_name}.aaxplugin"
    "${binary_name}.clap"
  )
  local package_entries=("README.md" "${artifact_names[@]}")

  log "Packaging ${package_name}"
  mkdir -p "${PACKAGE_ROOT}"
  rm -rf "${staging_dir}" "${package_path}"
  mkdir -p "${staging_dir}"

  local missing=0
  if [[ -f "${MACOS_INSTALLATION_DOC}" ]]; then
    if ! copy_packaged_readme "${staging_dir}/README.md"; then
      record_fail "Package ${package_name} (could not create README.md from ${MACOS_INSTALLATION_DOC})"
      missing=1
    fi
  else
    record_fail "Package ${package_name} (${MACOS_INSTALLATION_DOC} not found)"
    missing=1
  fi

  local artifact_name
  for artifact_name in "${artifact_names[@]}"; do
    local artifact_path="${source_dir}/${artifact_name}"
    if [[ -e "${artifact_path}" ]]; then
      cp -R "${artifact_path}" "${staging_dir}/${artifact_name}"
    else
      record_fail "Package ${package_name} (${artifact_path} not found)"
      missing=1
    fi
  done

  if [[ "${missing}" -ne 0 ]]; then
    return 0
  fi

  (
    cd "${staging_dir}" &&
      zip -qry --symlinks "${package_path}" "${package_entries[@]}"
  )

  if [[ -f "${package_path}" ]]; then
    record_packaged_artifact "${package_path}"
    record_ok "Package ${package_name}"
  else
    record_fail "Package ${package_name} (${package_path} not created)"
  fi
}

package_macos_distributables() {
  package_macos_variant "full" "Addivox"
  package_macos_variant "demo" "AddivoxDemo"
}

print_summary() {
  printf '\n'
  printf '============================================================\n'
  printf 'Addivox build summary\n'
  printf '============================================================\n'
  printf 'Build root: %s\n' "${WORK_ROOT}"
  printf 'Dist root:  %s\n' "${DIST_ROOT}"
  printf 'Logs:       %s\n' "${LOG_ROOT}"

  printf '\nSuccessful steps: %d\n' "${#OK_STEPS[@]}"
  if [[ "${#OK_STEPS[@]}" -gt 0 ]]; then
    for item in "${OK_STEPS[@]}"; do
      printf '  [OK] %s\n' "${item}"
    done
  fi

  printf '\nFailed steps: %d\n' "${#FAILED_STEPS[@]}"
  if [[ "${#FAILED_STEPS[@]}" -gt 0 ]]; then
    for item in "${FAILED_STEPS[@]}"; do
      printf '  [FAIL] %s\n' "${item}"
    done
  fi

  printf '\nCopied artifacts: %d\n' "${#COPIED_ARTIFACTS[@]}"
  if [[ "${#COPIED_ARTIFACTS[@]}" -gt 0 ]]; then
    for item in "${COPIED_ARTIFACTS[@]}"; do
      printf '  %s\n' "${item}"
    done
  fi

  printf '\nPackaged artifacts: %d\n' "${#PACKAGED_ARTIFACTS[@]}"
  if [[ "${#PACKAGED_ARTIFACTS[@]}" -gt 0 ]]; then
    for item in "${PACKAGED_ARTIFACTS[@]}"; do
      printf '  %s\n' "${item}"
    done
  fi

  if [[ "${INSTALL_PLUGINS}" -eq 1 ]]; then
    printf '\nPlugin install requested: yes\n'
  else
    printf '\nPlugin install requested: no. Use --install to copy macOS plugins to ~/Library/Audio/Plug-Ins for local testing.\n'
  fi

  printf '\nSigning/export status: unsigned build artifacts only. See TODO notes at the top of build_mac.sh before customer distribution or App Store upload.\n'
}

main() {
  require_tool xcodebuild
  require_tool cmake
  require_tool zip
  read_plug_version

  if [[ "${CLEAN}" -eq 1 ]]; then
    rm -rf "${BUILD_ROOT}"
  fi

  rm -rf "${DIST_ROOT}" "${XCODE_DERIVED_ROOT}" "${ARCHIVE_ROOT}" "${PACKAGE_ROOT}"
  rm -f "${BUILD_ROOT}"/Addivox_v*_macOS.zip "${BUILD_ROOT}"/AddivoxDemo_v*_macOS.zip
  mkdir -p "${DIST_ROOT}" "${LOG_ROOT}" "${ARCHIVE_ROOT}" "${PACKAGE_ROOT}"

  build_variant "full" 0
  build_variant "demo" 1
  reset_generated_resource_metadata
  package_macos_distributables

  if [[ "${INSTALL_PLUGINS}" -eq 1 ]]; then
    install_plugins
  fi

  print_summary

  if [[ "${#FAILED_STEPS[@]}" -gt 0 ]]; then
    exit 1
  fi
}

main "$@"
