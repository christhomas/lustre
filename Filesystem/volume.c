//
//  volume.c
//  Lustre
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

#include <libkern/OSAtomic.h>
#include <libkern/libkern.h>
#include <sys/buf.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <string.h>

#include "volume.h"
#include "logging.h"
#include "assert.h"
#include "constants.h"

extern char * itoa(int, char *);

#pragma mark - Internal Functions

#pragma mark - External Functions

struct lustre_volume * lustre_volume_alloc(void)
{
    struct lustre_volume *  volume;
    errno_t                 error;
    
    error = 0;
    
    volume = (struct lustre_volume *)OSMalloc(sizeof(struct lustre_volume), lustre_os_malloc_tag);
    if (!volume) {
        error = ENOMEM;
        os_log_error(lustre_logger_default, "Couldn't allocate volume");
        goto end;
    }
    
    bzero(volume, sizeof(struct lustre_volume));
    
    volume->ref_count = 1;
    
    volume->lock = lck_mtx_alloc_init(lustre_lock_group, NULL);
    if (volume->lock == NULL) {
        error = ENOMEM;
        os_log_error(lustre_logger_default, "Couldn't allocate volume lock");
        goto end;
    }
    
    volume->root_lock = lck_mtx_alloc_init(lustre_lock_group, NULL);
    if (volume->root_lock == NULL) {
        error = ENOMEM;
        os_log_error(lustre_logger_default, "Couldn't allocate volume root lock");
        goto end;
    }
    
    volume->stats_lock = lck_spin_alloc_init(lustre_lock_group, NULL);
    if (volume->stats_lock == NULL) {
        error = ENOMEM;
        os_log_error(lustre_logger_default, "Couldn't allocate volume stats lock");
        goto end;
    }
    
end:
    if (error != 0) {
        if (volume->stats_lock) {
            lck_spin_free(volume->stats_lock, lustre_lock_group);
        }
        if (volume->root_lock) {
            lck_mtx_free(volume->root_lock, lustre_lock_group);
        }
        if (volume->lock) {
            lck_mtx_free(volume->lock, lustre_lock_group);
        }
        if (volume) {
            OSFree(volume, sizeof(struct lustre_volume), lustre_os_malloc_tag);
        }
        volume = NULL;
    }
    
    return volume;
}

void lustre_volume_free(struct lustre_volume * volume)
{
    LUSTRE_BUG_ON(!volume);
    LUSTRE_BUG_ON(volume->ref_count == 0);
    
    if (volume->stats_lock) {
        lck_spin_free(volume->stats_lock, lustre_lock_group);
    }
    if (volume->root_lock) {
        lck_mtx_free(volume->root_lock, lustre_lock_group);
    }
    
    volume = NULL;
}

void lustre_volume_ref_count_inc(struct lustre_volume * volume)
{
    LUSTRE_BUG_ON(!volume);
    
    OSIncrementAtomic(&volume->ref_count);
}

void lustre_volume_ref_count_dec(struct lustre_volume * volume)
{
    int32_t count;
    
    LUSTRE_BUG_ON(!volume);
    
    count = OSDecrementAtomic(&volume->ref_count);
    
    if (count == 0) {
        lustre_volume_free(volume);
    }
}

void lustre_volume_set_ready(struct lustre_volume * volume)
{
    LUSTRE_BUG_ON(!volume);
    OSCompareAndSwap(0, 1, &volume->ready);
}

uint8_t lustre_volume_is_ready(struct lustre_volume * volume)
{
    LUSTRE_BUG_ON(!volume);
    
    return volume->ready;
}

struct lustre_volume * lustre_volume_peek(mount_t mount)
{
    struct lustre_volume *result;
    
    LUSTRE_BUG_ON(!mount);
    
    result = vfs_fsprivate(mount);
    
    LUSTRE_BUG_ON(!result);
    LUSTRE_BUG_ON(result->mount_point != mount);
    
    return result;
}

struct lustre_mount_args lustre_volume_mount_args(struct lustre_volume * volume)
{
    LUSTRE_BUG_ON(!volume);
    return volume->mount_args;
}

void lustre_volume_set_mount_args(struct lustre_volume * volume, struct lustre_mount_args mount_args)
{
    LUSTRE_BUG_ON(!volume);
    
    lck_mtx_lock(volume->lock);
    
    volume->mount_args = mount_args;
    strlcpy(volume->volume_name, mount_args.label, kLustreVolumeLabelSize);
    
    lck_mtx_unlock(volume->lock);
}

mount_t lustre_volume_mount_point(const struct lustre_volume * volume)
{
    LUSTRE_BUG_ON(!volume);
    
    return volume->mount_point;
}

const char * lustre_volume_url(const struct lustre_volume * volume)
{
    static char url[9+8+1+kLustreMountArgsHostMax+1+MAXPATHLEN] = "?"; // lustre://user@host/path
    
    const char * identity;
    
    LUSTRE_BUG_ON(!volume);
    
    if (url[0] == '?') {
        identity = (const char *)&volume->mount_args.identity;
        snprintf(url, sizeof(url), "lustre://%s/%s", volume->mount_args.host, volume->mount_args.label);
    }
    
    return url;
}

errno_t lustre_volume_setup(struct lustre_volume * volume)
{
    errno_t         error;
    struct timespec now_spec;
    
    LUSTRE_BUG_ON(!volume);
    
    error = 0;
    
    uuid_generate(volume->uuid);
    nanotime(&now_spec);
    
    volume->fsid.val[0]     = *((uint32_t *)volume->uuid);
    volume->fsid.val[1]     = 0;
    volume->create_time     = (struct timespec){ 0, 0 };
    volume->modify_time     = (struct timespec){ 0, 0 };
    volume->access_time     = (struct timespec){ 0, 0 };
    volume->backup_time     = (struct timespec){ 0, 0 };
    volume->checked_time    = (struct timespec){ 0, 0 };

    return error;
}

errno_t lustre_volume_teardown(struct lustre_volume * volume)
{
    errno_t error;
    
    LUSTRE_BUG_ON(!volume);
    
    error = 0;
    
    return error;
    
}

uid_t lustre_volume_uid_from_owner_identity(const struct lustre_volume * volume, uint64_t identity)
{
    uid_t result;
    
    LUSTRE_BUG_ON(!volume);
    
    result = kLustreFilesystemNobodyOwnerId;
    
    return result;
}

gid_t lustre_volume_gid_from_group_identity(const struct lustre_volume * volume, uint64_t identity)
{
    gid_t result;
    
    LUSTRE_BUG_ON(!volume);
    
    result = kLustreFilesystemNobodyGroupId;
    
    return result;
}

uint16_t lustre_volume_signature(const struct lustre_volume * volume)
{
    return kLustreFilesystemSignature;
}

void lustre_volume_uuid(const struct lustre_volume * volume, uuid_t uuid)
{
    LUSTRE_BUG_ON(!volume);
    LUSTRE_BUG_ON(!uuid);
    
    lck_mtx_lock(volume->lock);
    uuid_copy(uuid, volume->uuid);
    lck_mtx_unlock(volume->lock);
}

fsid_t lustre_volume_fsid(const struct lustre_volume * volume)
{
    LUSTRE_BUG_ON(!volume);
    
    return volume->fsid;
}

const char * lustre_volume_label(const struct lustre_volume * volume)
{
    const char * label;
    
    LUSTRE_BUG_ON(!volume);
    
    lck_mtx_lock(volume->lock);
    label = volume->volume_name;
    lck_mtx_unlock(volume->lock);
    
    return label;
}

uint32_t lustre_volume_block_size(const struct lustre_volume * volume)
{
    uint32_t block_size;
    
    LUSTRE_BUG_ON(!volume);
    
    lck_mtx_lock(volume->lock);
    block_size = volume->block_size;
    lck_mtx_unlock(volume->lock);
    
    return block_size;
}

uint64_t lustre_volume_blocks_total(const struct lustre_volume * volume)
{
    uint64_t    blocks_total;
    
    LUSTRE_BUG_ON(!volume);
    
    blocks_total = 200;
    
    return blocks_total;
}

uint64_t lustre_volume_blocks_free(const struct lustre_volume * volume)
{
    uint64_t    blocks_free;
    
    LUSTRE_BUG_ON(!volume);
    
    blocks_free = 0;
    
    return blocks_free;
}

uint64_t lustre_volume_blocks_available(const struct lustre_volume * volume)
{
    return lustre_volume_blocks_free(volume);
}

uint64_t lustre_volume_file_count(const struct lustre_volume * volume)
{
    uint64_t    count;
    
    LUSTRE_BUG_ON(!volume);
    
    count = 0;
    
    return count;
}

uint64_t lustre_volume_folder_count(const struct lustre_volume * volume)
{
    uint64_t    count;
    
    LUSTRE_BUG_ON(!volume);
    
    count = 0;
    
    return count;
}

struct timespec lustre_volume_create_time(const struct lustre_volume * volume)
{
    struct timespec create_time;
    
    LUSTRE_BUG_ON(!volume);
    
    lck_mtx_lock(volume->lock);
    create_time = volume->create_time;
    lck_mtx_unlock(volume->lock);
    
    return create_time;
}

struct timespec lustre_volume_modify_time(const struct lustre_volume * volume)
{
    struct timespec modify_time;
    
    LUSTRE_BUG_ON(!volume);
    
    lck_mtx_lock(volume->lock);
    modify_time = volume->modify_time;
    lck_mtx_unlock(volume->lock);
    
    return modify_time;
}

struct timespec lustre_volume_access_time(const struct lustre_volume * volume)
{
    struct timespec access_time;
    
    LUSTRE_BUG_ON(!volume);
    
    lck_mtx_lock(volume->lock);
    access_time = volume->access_time;
    lck_mtx_unlock(volume->lock);
    
    return access_time;
}

struct timespec lustre_volume_backup_time(const struct lustre_volume * volume)
{
    struct timespec backup_time;
    
    LUSTRE_BUG_ON(!volume);
    
    lck_mtx_lock(volume->lock);
    backup_time = volume->backup_time;
    lck_mtx_unlock(volume->lock);
    
    return backup_time;
}
