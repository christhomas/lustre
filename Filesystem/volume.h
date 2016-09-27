//
//  volume.h
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

#ifndef lustre_volume_h
#define lustre_volume_h

#include <sys/mount.h>
#include <uuid/uuid.h>
#include "constants.h"
#include "mount_args.h"
#include "lustre.h"
#include "rb_tree.h"

static const uint8_t    kLustreVolumeUUIDSize               = 16;

struct lustre_volume {
    mount_t                                         mount_point;                    // back pointer to the mount_t
    struct lustre_mount_args                        mount_args;                     // arguments set on mount
    char                                            volume_name[kLustreVolumeLabelSize];// volume name (UTF-8)
    struct vfs_attr                                 attr;                           // pre-calculate volume attributes
    
    lck_mtx_t *                                     lock;                           // protects following fields
    uint8_t                                         ready;                          // all initialized flag
    uint32_t                                        block_size;                     // default size for blocks
    
    uint8_t                                         uuid[kLustreVolumeUUIDSize];
    fsid_t                                          fsid;
    
    lck_mtx_t *                                     root_lock;                      // protects following fields
    boolean_t                                       root_attaching;                 // true if someone is attaching a root vnode
    boolean_t                                       root_waiting;                   // true if someone is waiting for such an attach to complete
    vnode_t                                         root_vnode;                     // the root vnode; we hold /no/ proper references to this, and must reconfirm its existance each time
    
    lck_spin_t *                                    stats_lock;                     // protect the following fields
    struct timespec                                 create_time;                    // time of volume creation
    struct timespec                                 modify_time;                    // time of last modification
    struct timespec                                 access_time;                    // time of last access
    struct timespec                                 backup_time;                    // time of last backup
    struct timespec                                 checked_time;                   // time of last disk check
    
    int32_t                                         ref_count;                      // keep track of the number of references
};

struct lustre_volume *      lustre_volume_alloc(void);

void                        lustre_volume_ref_count_inc(struct lustre_volume * volume);
void                        lustre_volume_ref_count_dec(struct lustre_volume * volume);

void                        lustre_volume_set_ready(struct lustre_volume * volume);
uint8_t                     lustre_volume_is_ready(struct lustre_volume * volume);

struct lustre_volume *      lustre_volume_peek(mount_t mount);


void                        lustre_volume_set_mount_args(struct lustre_volume * volume, struct lustre_mount_args mount_args);
struct lustre_mount_args    lustre_volume_mount_args(struct lustre_volume * volume);

mount_t                     lustre_volume_mount_point(const struct lustre_volume * volume);

const char *                lustre_volume_url(const struct lustre_volume * volume);

errno_t                     lustre_volume_setup(struct lustre_volume * volume);
errno_t                     lustre_volume_teardown(struct lustre_volume * volume);

uid_t                       lustre_volume_uid_from_owner_identity(const struct lustre_volume * volume, uint64_t identity);
gid_t                       lustre_volume_gid_from_group_identity(const struct lustre_volume * volume, uint64_t identity);

uint16_t                    lustre_volume_signature(const struct lustre_volume * volume);
void                        lustre_volume_uuid(const struct lustre_volume * volume, uuid_t uuid);
fsid_t                      lustre_volume_fsid(const struct lustre_volume * volume);
const char *                lustre_volume_label(const struct lustre_volume * volume);
uint32_t                    lustre_volume_block_size(const struct lustre_volume * volume);
uint64_t                    lustre_volume_blocks_total(const struct lustre_volume * volume);
uint64_t                    lustre_volume_blocks_free(const struct lustre_volume * volume);
uint64_t                    lustre_volume_blocks_available(const struct lustre_volume * volume);

uint64_t                    lustre_volume_file_count(const struct lustre_volume * volume);
uint64_t                    lustre_volume_folder_count(const struct lustre_volume * volume);
struct timespec             lustre_volume_create_time(const struct lustre_volume * volume);
struct timespec             lustre_volume_modify_time(const struct lustre_volume * volume);
struct timespec             lustre_volume_access_time(const struct lustre_volume * volume);
struct timespec             lustre_volume_backup_time(const struct lustre_volume * volume);

#endif /* lustre_volume_h */
