#!/bin/sh

#  Lustre
#  Package
#
#  Lustre Filesystem For macOS
#  Copyright (C) 2016 Cider Apps, LLC.
#
#  This program is free software: you can redistribute it and/or modify
#  it under the terms of the GNU General Public License as published by
#  the Free Software Foundation, either version 3 of the License, or
#  (at your option) any later version.
#
#  This program is distributed in the hope that it will be useful,
#  but WITHOUT ANY WARRANTY; without even the implied warranty of
#  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
#  GNU General Public License for more details.
#
#  You should have received a copy of the GNU General Public License
#  along with this program.  If not, see <http://www.gnu.org/licenses/>.
#

set

echo ""
echo ""

VERSION=$(defaults read "${ARCHIVE_PRODUCTS_PATH}/Library/Extensions/${CONTENTS_FOLDER_PATH}/Info" CFBundleVersion)

PACKAGE_NAME=`echo "$PRODUCT_NAME" | sed "s/ //g"`
PACKAGE_DIR="${ARCHIVE_PATH}/Package"
ARCHIVE_FILENAME="${PACKAGE_DIR}/Lustre.pkg"
ARCHIVE_PACKAGES="${ARCHIVE_PATH}/Packages"

mkdir -p "${PACKAGE_DIR}"
rm -rf "${ARCHIVE_FILENAME}"
rm -f "${PROJECT_DIR}/Build/Lustre.pkg"

pkgbuild --root "${ARCHIVE_PRODUCTS_PATH}" --component-plist "${PROJECT_DIR}/Package/Extension-Components.plist" --identifier "com.ciderapps.lustre.Extension" --version "$VERSION" --install-location "/" --filter "Library/NetFSPlugins" --filter "Library/Filesystems" --scripts "${PROJECT_DIR}/Package/Scripts/Extension" "${CONFIGURATION_TEMP_DIR}/Extension.pkg"
pkgbuild --root "${ARCHIVE_PRODUCTS_PATH}" --component-plist "${PROJECT_DIR}/Package/Filesystem-Components.plist" --identifier "com.ciderapps.lustre.Filesystem" --version "$VERSION" --install-location "/" --filter "Library/NetFSPlugins" --filter "Library/Extensions" --scripts "${PROJECT_DIR}/Package/Scripts/Filesystem" "${CONFIGURATION_TEMP_DIR}/Filesystem.pkg"
pkgbuild --root "${ARCHIVE_PRODUCTS_PATH}" --component-plist "${PROJECT_DIR}/Package/NetFS-Components.plist" --identifier "com.ciderapps.lustre.NetFS" --version "$VERSION" --install-location "/" --filter "Library/Filesystems/lustre.fs" --filter "Library/Extensions" --scripts "${PROJECT_DIR}/Package/Scripts/NetFS" "${CONFIGURATION_TEMP_DIR}/NetFS.pkg"

productbuild --distribution "${PROJECT_DIR}/Package/Distribution.xml" --package-path "${CONFIGURATION_TEMP_DIR}/" --resources "${PROJECT_DIR}/Package/Resources" --sign "Developer ID Installer: Matthew Bauer (3FSTQKR2FT)" "${ARCHIVE_FILENAME}"

if [ -f "${ARCHIVE_FILENAME}" ]
then
    exit 0
else
    exit 1
fi
