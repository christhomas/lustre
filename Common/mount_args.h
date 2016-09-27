//
//  mount_args.h
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

#ifndef lustre_mount_args_h
#define lustre_mount_args_h

#include <stdint.h>

#include <stdint.h>
#include <netinet/in.h>
#include "constants.h"

static const uint16_t kLustreMountArgsHostMax   = 255;
static const uint16_t kLustreMountArgsLabelMax  = 255;

struct lustre_mount_args {
    char                        host[kLustreMountArgsHostMax];          // string representation of ip4/ip6 address since the kernel doesn't have the routines
    struct sockaddr_in          address_ip4;                            // ipv4 address to use (if present)
    struct sockaddr_in6         address_ip6;                            // ipv6 address to use (if present)
    uint16_t                    port;                                   // port to connect on
    char                        label[kLustreMountArgsLabelMax];        // label for the volume
    uint64_t                    identity;                               // user to connect as
    uint8_t                     identity_key[kLustreIdentityKeyLengthMax]; // user key
    uint8_t                     identity_key_length;                    // length of above key
    uint32_t                    force_failure;                          // if non-zero, mount will always fail
    int32_t                     flags;                                  // mount flags
};

#endif /* lustre_mount_args_h */
