//
//  mount.h
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

#ifndef lustre_mount_h
#define lustre_mount_h

#include <sys/mount.h>
#include "volume.h"

void        lustre_mount_volume_get_attr(const struct lustre_volume * volume, struct vfs_attr * attr);
errno_t     lustre_mount_setup(struct lustre_volume * volume, user_addr_t data, vfs_context_t context);
errno_t     lustre_mount_teardown(struct lustre_volume * volume);
void        lustre_mount_setup_vfs(struct lustre_volume * volume);

#endif /* lustre_mount_h */
