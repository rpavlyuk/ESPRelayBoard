#!/usr/bin/env python3
"""
Script to generate version information for ESPRelayBoard project
Usage: python make_version.py <source_folder_root_path> <major_version>
"""

import sys
import os
from datetime import datetime

if len(sys.argv) != 3:
    print(f"Usage: {sys.argv[0]} <source_folder_root_path> <major_version>")
    sys.exit(1)

SOURCE_FOLDER_ROOT_PATH = sys.argv[1]
MAJOR_VERSION = sys.argv[2]

VERSION_H_FILE = os.path.join(SOURCE_FOLDER_ROOT_PATH, 'main', 'version.h')

# Generate the build number based on the current date and time
DEVICE_SW_BUILD_NUM = datetime.now().strftime("%Y%m%d%H%M%S")
DEVICE_SW_VERSION_NUM = MAJOR_VERSION
DEVICE_SW_VERSION = f'{DEVICE_SW_VERSION_NUM} build {DEVICE_SW_BUILD_NUM}'

# Create the content for version.h
version_content = f"""#ifndef VERSION_H
#define VERSION_H

#define DEVICE_SW_BUILD_NUM "{DEVICE_SW_BUILD_NUM}"
#define DEVICE_SW_VERSION_NUM "{DEVICE_SW_VERSION_NUM}"
#define DEVICE_SW_VERSION DEVICE_SW_VERSION_NUM " build " DEVICE_SW_BUILD_NUM

#endif // VERSION_H
"""

# Write the content to version.h
os.makedirs(os.path.dirname(VERSION_H_FILE), exist_ok=True)
with open(VERSION_H_FILE, 'w') as file:
    file.write(version_content)

print(f"Version header file generated at {VERSION_H_FILE}")