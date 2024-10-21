#!/bin/bash
#===============================================================================
#
#          FILE: make-version.sh
# 
#         USAGE: ./make-version.sh
# 
#   DESCRIPTION: Script to generate version information for ESPRelayBoard project
# 
#===============================================================================

if [ -z "$1" ] || [ -z "$2" ]; then
    echo "Usage: $0 <source_folder_root_path> <major_version>"
    exit 1
fi

SOURCE_FOLDER_ROOT_PATH=$1
MAJOR_VERSION=$2

VERSION_H_FILE="$SOURCE_FOLDER_ROOT_PATH/main/version.h"

cat <<EOL > "$VERSION_H_FILE"
#ifndef VERSION_H
#define VERSION_H

#define DEVICE_SW_BUILD_NUM "$(date +%Y%m%d%H%M%S)"
#define DEVICE_SW_VERSION_NUM "$MAJOR_VERSION"
#define DEVICE_SW_VERSION DEVICE_SW_VERSION_NUM " build " DEVICE_SW_BUILD_NUM

#endif // VERSION_H
EOL

echo "Version header file generated at $VERSION_H_FILE"