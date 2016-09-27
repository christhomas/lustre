//
//  mount.c
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

#include <kern/locks.h>
#include <sys/vnode.h>
#include <sys/disk.h>
#include <sys/proc.h>
#include <libkern/libkern.h>

#include "mount.h"
#include "volume.h"
#include "logging.h"
#include "assert.h"
#include "mount_args.h"

#pragma mark - Internal

// Some truth here

void lustre_mount_init_get_attr_list_goop(struct vfs_attr * attr)
{
    attr->f_capabilities.capabilities[VOL_CAPABILITIES_FORMAT]      = 0
    | VOL_CAP_FMT_CASE_SENSITIVE
    | VOL_CAP_FMT_FAST_STATFS
    | VOL_CAP_FMT_2TB_FILESIZE
    | VOL_CAP_FMT_HIDDEN_FILES
    | VOL_CAP_FMT_64BIT_OBJECT_IDS
    ;
    attr->f_capabilities.valid[VOL_CAPABILITIES_FORMAT]             = 0
    | VOL_CAP_FMT_PERSISTENTOBJECTIDS
    | VOL_CAP_FMT_SYMBOLICLINKS
    | VOL_CAP_FMT_HARDLINKS
    | VOL_CAP_FMT_JOURNAL
    | VOL_CAP_FMT_JOURNAL_ACTIVE
    | VOL_CAP_FMT_NO_ROOT_TIMES
    | VOL_CAP_FMT_SPARSE_FILES
    | VOL_CAP_FMT_ZERO_RUNS
    | VOL_CAP_FMT_CASE_SENSITIVE
    | VOL_CAP_FMT_CASE_PRESERVING
    | VOL_CAP_FMT_FAST_STATFS
    | VOL_CAP_FMT_2TB_FILESIZE
    | VOL_CAP_FMT_OPENDENYMODES
    | VOL_CAP_FMT_HIDDEN_FILES
    | VOL_CAP_FMT_PATH_FROM_ID
    | VOL_CAP_FMT_NO_VOLUME_SIZES
    | VOL_CAP_FMT_DECMPFS_COMPRESSION
    | VOL_CAP_FMT_64BIT_OBJECT_IDS
    ;
    attr->f_capabilities.capabilities[VOL_CAPABILITIES_INTERFACES]  = 0
    | VOL_CAP_INT_READDIRATTR
    ;
    attr->f_capabilities.valid[VOL_CAPABILITIES_INTERFACES]         = 0
    | VOL_CAP_INT_SEARCHFS
    | VOL_CAP_INT_ATTRLIST
    | VOL_CAP_INT_NFSEXPORT
    | VOL_CAP_INT_READDIRATTR
    | VOL_CAP_INT_EXCHANGEDATA
    | VOL_CAP_INT_COPYFILE
    | VOL_CAP_INT_ALLOCATE
    | VOL_CAP_INT_VOL_RENAME
    | VOL_CAP_INT_ADVLOCK
    | VOL_CAP_INT_FLOCK
    | VOL_CAP_INT_EXTENDED_SECURITY
    | VOL_CAP_INT_USERACCESS
    | VOL_CAP_INT_MANLOCK
    | VOL_CAP_INT_NAMEDSTREAMS
    | VOL_CAP_INT_EXTENDED_ATTR
    ;
    attr->f_attributes.validattr.commonattr                         = 0
    | ATTR_CMN_NAME
    | ATTR_CMN_FSID
    | ATTR_CMN_CRTIME
    | ATTR_CMN_MODTIME
    | ATTR_CMN_CHGTIME
    | ATTR_CMN_ACCTIME
    | ATTR_CMN_BKUPTIME
    | ATTR_CMN_OWNERID
    | ATTR_CMN_GRPID
    | ATTR_CMN_FLAGS
    | ATTR_CMN_FILEID
    | ATTR_CMN_PARENTID
    | ATTR_CMN_FULLPATH
    ;
    attr->f_attributes.validattr.volattr                            = 0
    | ATTR_VOL_FSTYPE
    | ATTR_VOL_SIGNATURE
    | ATTR_VOL_SIZE
    | ATTR_VOL_SPACEFREE
    | ATTR_VOL_SPACEAVAIL
    | ATTR_VOL_IOBLOCKSIZE
    | ATTR_VOL_OBJCOUNT
    | ATTR_VOL_FILECOUNT
    | ATTR_VOL_DIRCOUNT
    | ATTR_VOL_MAXOBJCOUNT
    | ATTR_VOL_MOUNTPOINT
    | ATTR_VOL_NAME
    | ATTR_VOL_MOUNTFLAGS
    | ATTR_VOL_MOUNTEDDEVICE
    | ATTR_VOL_ENCODINGSUSED
    | ATTR_VOL_CAPABILITIES
    | ATTR_VOL_ATTRIBUTES
    | ATTR_VOL_INFO
    ;
    attr->f_attributes.validattr.dirattr                            = 0
    | ATTR_DIR_LINKCOUNT
    | ATTR_DIR_ENTRYCOUNT
    | ATTR_DIR_MOUNTSTATUS
    ;
    attr->f_attributes.validattr.fileattr                           = 0
    | ATTR_FILE_LINKCOUNT
    | ATTR_FILE_TOTALSIZE
    | ATTR_FILE_ALLOCSIZE
    | ATTR_FILE_IOBLOCKSIZE
    | ATTR_FILE_DEVTYPE
    | ATTR_FILE_FORKCOUNT
    | ATTR_FILE_FORKLIST
    | ATTR_FILE_DATALENGTH
    | ATTR_FILE_DATAALLOCSIZE
    | ATTR_FILE_VALIDMASK
    | ATTR_FILE_SETMASK
    | ATTR_FORK_TOTALSIZE
    | ATTR_FORK_ALLOCSIZE
    ;
    attr->f_attributes.validattr.forkattr                           = 0;
    
    // All attributes that we do support, we support natively.
    
    attr->f_attributes.nativeattr.commonattr = attr->f_attributes.validattr.commonattr;
    attr->f_attributes.nativeattr.volattr    = attr->f_attributes.validattr.volattr;
    attr->f_attributes.nativeattr.dirattr    = attr->f_attributes.validattr.dirattr;
    attr->f_attributes.nativeattr.fileattr   = attr->f_attributes.validattr.fileattr;
    attr->f_attributes.nativeattr.forkattr   = attr->f_attributes.validattr.forkattr;
    
    VFSATTR_SET_SUPPORTED(attr, f_capabilities);
    VFSATTR_SET_SUPPORTED(attr, f_attributes);
}

void lustre_mount_volume_get_attr(const struct lustre_volume * volume, struct vfs_attr * attr)
{
    LUSTRE_BUG_ON(!volume);
    LUSTRE_BUG_ON(!attr);
    
    lustre_mount_init_get_attr_list_goop(attr);
    
    VFSATTR_RETURN(attr, f_objcount,    lustre_volume_file_count(volume) + lustre_volume_folder_count(volume) + 1); // + 1 for root directory
    VFSATTR_RETURN(attr, f_filecount,   lustre_volume_file_count(volume));
    VFSATTR_RETURN(attr, f_dircount,    lustre_volume_folder_count(volume));
    VFSATTR_RETURN(attr, f_bsize,       lustre_volume_block_size(volume));
    VFSATTR_RETURN(attr, f_iosize,      lustre_volume_block_size(volume));
    VFSATTR_RETURN(attr, f_blocks,      lustre_volume_blocks_total(volume));
    VFSATTR_RETURN(attr, f_bfree,       lustre_volume_blocks_free(volume));
    VFSATTR_RETURN(attr, f_bavail,      lustre_volume_blocks_available(volume));
    VFSATTR_RETURN(attr, f_bused,       lustre_volume_blocks_total(volume) - lustre_volume_blocks_available(volume));
    VFSATTR_RETURN(attr, f_files,       lustre_volume_file_count(volume) + lustre_volume_folder_count(volume));
    VFSATTR_RETURN(attr, f_ffree,       lustre_volume_blocks_free(volume) * lustre_volume_block_size(volume) / 10 /*sizeof(struct lustre_inode_object)*/); // It's a rough number and good as any
    VFSATTR_RETURN(attr, f_create_time, lustre_volume_create_time(volume));
    VFSATTR_RETURN(attr, f_modify_time, lustre_volume_modify_time(volume));
    VFSATTR_RETURN(attr, f_access_time, lustre_volume_access_time(volume));
    VFSATTR_RETURN(attr, f_backup_time, lustre_volume_backup_time(volume));
    if (VFSATTR_IS_ACTIVE(attr, f_vol_name)) {
        strlcpy(attr->f_vol_name, lustre_volume_label(volume), MAXPATHLEN);
        VFSATTR_SET_SUPPORTED(attr, f_vol_name);
    }
    VFSATTR_RETURN(attr, f_signature,   lustre_volume_signature(volume));
    if (VFSATTR_IS_ACTIVE(attr, f_uuid)) {
        lustre_volume_uuid(volume, attr->f_uuid);
        VFSATTR_SET_SUPPORTED(attr, f_uuid);
    }
    VFSATTR_RETURN(attr, f_fssubtype,   0);
    VFSATTR_RETURN(attr, f_fsid,        lustre_volume_fsid(volume));
}

#pragma mark - External

errno_t lustre_mount_setup(struct lustre_volume * volume, user_addr_t data, vfs_context_t context)
{
    errno_t                     error;
    struct lustre_mount_args    mount_args;
    
    LUSTRE_BUG_ON(!volume);
    LUSTRE_BUG_ON(!context);
    
    error = copyin(data, &mount_args, sizeof(struct lustre_mount_args));
    if (error != 0) {
        os_log_error(lustre_logger_vfs, "Couldn't copyin mount args");
        goto end;
    }
    
    lustre_volume_set_mount_args(volume, mount_args);
    
    error = lustre_volume_setup(volume);
    if (error != 0) {
        os_log_error(lustre_logger_vfs, "Failed to setup volume");
        goto end;
    }
    
    lustre_volume_set_ready(volume);
    
end:
    
    return error;
}

errno_t lustre_mount_teardown(struct lustre_volume * volume)
{
    errno_t error;
    
    LUSTRE_BUG_ON(!volume);
    
    error = 0;
    
    error = lustre_volume_teardown(volume);
    if (error != 0) {
        os_log_info(lustre_logger_vfs, "failed to teardown volume");
        goto end;
    }
    
end:
    return error;
}

void lustre_mount_setup_vfs(struct lustre_volume * volume)
{
    struct vfs_attr     attr;
    struct vfsstatfs *  statfs;
    
    LUSTRE_BUG_ON(!volume);
    LUSTRE_BUG_ON(volume->root_vnode);
    LUSTRE_BUG_ON(!volume->mount_point);
    LUSTRE_BUG_ON(volume->root_attaching);
    LUSTRE_BUG_ON(volume->root_waiting);
    
    // Set up the statfs information.  You can get a pointer to the vfsstatfs
    // that you need to fill out by calling vfs_statfs.  Before calling your
    // mount entry point, VFS has already zeroed the entire structure and set
    // up f_fstypename, f_mntonname, f_mntfromname (if VFC_VFSLOCALARGS was set;
    // in the other case VFS doesn't know this information and you have to set it
    // yourself), and f_owner.  You are responsible for filling out the other fields
    // (except f_reserved1, f_type, and f_flags, which are reserved).  You can also
    // override VFS's settings if need be.
    
    // IMPORTANT:
    // It is vital that you fill out all of these fields (especially the
    // f_bsize, f_bfree, and f_bavail fields) before returning from lustre_vfsop_mount.
    // If you don't, higher-level system components (such as File Manager) can
    // get very confused.  Specifically, File Manager can get and /cache/ these
    // values before calling lustre_vfsop_getattr.  So you can't rely on a call to
    // lustre_vfsop_getattr to set up these fields for the first time.
    
    VFSATTR_INIT(&attr);
    VFSATTR_WANTED(&attr, f_objcount);
    VFSATTR_WANTED(&attr, f_filecount);
    VFSATTR_WANTED(&attr, f_dircount);
    VFSATTR_WANTED(&attr, f_bsize);
    VFSATTR_WANTED(&attr, f_iosize);
    VFSATTR_WANTED(&attr, f_blocks);
    VFSATTR_WANTED(&attr, f_bfree);
    VFSATTR_WANTED(&attr, f_bavail);
    VFSATTR_WANTED(&attr, f_bused);
    VFSATTR_WANTED(&attr, f_files);
    VFSATTR_WANTED(&attr, f_ffree);
    VFSATTR_WANTED(&attr, f_fsid);
    VFSATTR_WANTED(&attr, f_fssubtype);
    
    lustre_mount_volume_get_attr(volume, &attr);
    
    statfs = vfs_statfs(volume->mount_point);
    LUSTRE_BUG_ON(!statfs);
    LUSTRE_BUG_ON(strcmp(statfs->f_fstypename, kLustreFilesystemName) != 0);
    
    LUSTRE_BUG_ON(!VFSATTR_IS_SUPPORTED(&attr, f_bsize));
    statfs->f_bsize     = attr.f_bsize;
    LUSTRE_BUG_ON(!VFSATTR_IS_SUPPORTED(&attr, f_iosize));
    statfs->f_iosize    = attr.f_iosize;
    LUSTRE_BUG_ON(!VFSATTR_IS_SUPPORTED(&attr, f_blocks));
    statfs->f_blocks    = attr.f_blocks;
    LUSTRE_BUG_ON(!VFSATTR_IS_SUPPORTED(&attr, f_bfree));
    statfs->f_bfree     = attr.f_bfree;
    LUSTRE_BUG_ON(!VFSATTR_IS_SUPPORTED(&attr, f_bavail));
    statfs->f_bavail    = attr.f_bavail;
    LUSTRE_BUG_ON(!VFSATTR_IS_SUPPORTED(&attr, f_bused));
    statfs->f_bused     = attr.f_bused;
    LUSTRE_BUG_ON(!VFSATTR_IS_SUPPORTED(&attr, f_files));
    statfs->f_files     = attr.f_files;
    LUSTRE_BUG_ON(!VFSATTR_IS_SUPPORTED(&attr, f_ffree));
    statfs->f_ffree     = attr.f_ffree;
    LUSTRE_BUG_ON(!VFSATTR_IS_SUPPORTED(&attr, f_fsid));
    statfs->f_fsid      = attr.f_fsid;
    LUSTRE_BUG_ON(!VFSATTR_IS_SUPPORTED(&attr, f_fssubtype));
    statfs->f_fssubtype = attr.f_fssubtype;
    
    strlcpy(statfs->f_mntfromname, lustre_volume_url(volume), MAXPATHLEN);
    
    vfs_setflags(volume->mount_point, volume->mount_args.flags);    
}
