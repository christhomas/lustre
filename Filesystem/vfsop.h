//
//  vfsop.h
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

#ifndef lustre_vfsop_h
#define lustre_vfsop_h

errno_t lustre_vfsop_mount(mount_t mp, vnode_t devvp, user_addr_t data, vfs_context_t context);
errno_t lustre_vfsop_start(mount_t mp, int flags, vfs_context_t context);
errno_t lustre_vfsop_root(mount_t mp, struct vnode **vpp, vfs_context_t context);
errno_t lustre_vfsop_getattr(mount_t mp, struct vfs_attr *attr, vfs_context_t context);
errno_t lustre_vfsop_unmount(mount_t mp, int mntflags, vfs_context_t context);
errno_t lustre_vfsop_sync(struct mount *mp, int flags, vfs_context_t context);

#endif /* lustre_vfsop_h */
