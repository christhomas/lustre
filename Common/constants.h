//
//  constants.h
//  Filesystem
//
//  Lustre Filesystem For macOS
//  Copyright (C) 2016 Cider Apps, LLC.
//
//  This program is free software: you can redistribute it and/or modify
//  it under the terms of the GNU General Public License as published by
//  the Free Software Foundation, either version 3 of the License, or
//  (at your option) any later version.
//
//  This program is distributed in the hope that it will be useful,
//  but WITHOUT ANY WARRANTY; without even the implied warranty of
//  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
//  GNU General Public License for more details.
//
//  You should have received a copy of the GNU General Public License
//  along with this program.  If not, see <http://www.gnu.org/licenses/>.
//

#ifndef lustre_constants_h
#define lustre_constants_h

#include <stdint.h>

static const char *     kLustreFilesystemName               = "lustre";
static const uint16_t   kLustreFilesystemSignature          = 26;
static const uint16_t   kLustreVolumeLabelSize              = 256;
static const uint8_t    kLustreIdentityKeyLengthMax         = 16;
static const uint16_t   kLustreFilesystemRootParentFileId   = 1;
static const uint64_t   kLustreFilesystemRootFileId         = 2;
static const int32_t    kLustreFilesystemNobodyOwnerId      = -2;
static const int32_t    kLustreFilesystemNobodyGroupId      = -2;

#endif /* lustre_constants_h */
