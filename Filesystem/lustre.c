//
//  lustre.c
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

#include <sys/mount.h>

#include <sys/vnode.h>
#include "lustre.h"
#include "constants.h"
#include "vnop.h"
#include "vfsop.h"
#include "logging.h"
#include "assert.h"

#pragma mark - Globals

OSMallocTag lustre_os_malloc_tag    = NULL;     // used for all of our allocations.
lck_grp_t * lustre_lock_group       = NULL;     // used for all of our locks.

#pragma mark - Configuration

// vnode_operations is set up when we register the VFS plug-in with vfs_fsadd. It holds a pointer to the array of vnode operation functions for this
// VFS plug-in.  We have to declare it early in this file because it's referenced by the code that creates vnodes.
errno_t (** vnode_operations)(void *);

typedef errno_t (* vnodeop)(void *);

// vnodeop_entries is an array that describes all of the vnode operations supported by vnodes created by our VFS plug-in.  This is, in turn, wrapped up
// by vnodeop_opv_desc and vnodeop_opv_desc_list, and it's this last variable that's referenced by gVFSEntry.
//
// The following is a list of all of the vnode operations supported on Mac OS X 10.4+, with the ones that we support uncommented.
static struct vnodeopv_entry_desc vnodeop_entries[] = {
    //  { &vnop_access_desc,        (vnodeop) lustre_vnop_access       },
    //  { &vnop_advlock_desc,       (vnodeop) lustre_vnop_advlock      },
    //  { &vnop_allocate_desc,      (vnodeop) lustre_vnop_allocate     },
    //  { &vnop_blktooff_desc,      (vnodeop) lustre_vnop_blktooff     },
    //  { &vnop_blockmap_desc,      (vnodeop) lustre_vnop_blockmap     },
    //  { &vnop_bwrite_desc,        (vnodeop) lustre_vnop_bwrite       },
        { &vnop_close_desc,         (vnodeop) lustre_vnop_close        },
    //  { &vnop_copyfile_desc,      (vnodeop) lustre_vnop_copyfile     },
    //  { &vnop_create_desc,        (vnodeop) lustre_vnop_create       },
        { &vnop_default_desc,       (vnodeop) vn_default_error         },
    //  { &vnop_exchange_desc,      (vnodeop) lustre_vnop_exchange     },
    //  { &vnop_fsync_desc,         (vnodeop) lustre_vnop_fsync        },
        { &vnop_getattr_desc,       (vnodeop) lustre_vnop_getattr      },
    //  { &vnop_getattrlist_desc,   (vnodeop) lustre_vnop_getattrlist  },            // not useful, implement getattr instead
    //  { &vnop_getxattr_desc,      (vnodeop) lustre_vnop_getxattr     },
    //  { &vnop_inactive_desc,      (vnodeop) lustre_vnop_inactive     },
    //  { &vnop_ioctl_desc,         (vnodeop) lustre_vnop_ioctl        },
    //  { &vnop_link_desc,          (vnodeop) lustre_vnop_link         },
    //  { &vnop_listxattr_desc,     (vnodeop) lustre_vnop_listxattr    },
        { &vnop_lookup_desc,        (vnodeop) lustre_vnop_lookup       },
    //  { &vnop_mkdir_desc,         (vnodeop) lustre_vnop_mkdir        },
    //  { &vnop_mknod_desc,         (vnodeop) lustre_vnop_mknod        },
    //  { &vnop_mmap_desc,          (vnodeop) lustre_vnop_mmap         },
    //  { &vnop_mnomap_desc,        (vnodeop) lustre_vnop_mnomap       },
    //  { &vnop_offtoblk_desc,      (vnodeop) lustre_vnop_offtoblk     },
        { &vnop_open_desc,          (vnodeop) lustre_vnop_open         },
    //  { &vnop_pagein_desc,        (vnodeop) lustre_vnop_pagein       },
    //  { &vnop_pageout_desc,       (vnodeop) lustre_vnop_pageout      },
    //  { &vnop_pathconf_desc,      (vnodeop) lustre_vnop_pathconf     },
    //  { &vnop_read_desc,          (vnodeop) lustre_vnop_read         },
        { &vnop_readdir_desc,       (vnodeop) lustre_vnop_read_dir     },
    //  { &vnop_readdirattr_desc,   (vnodeop) lustre_vnop_readdirattr  },
    //  { &vnop_readlink_desc,      (vnodeop) lustre_vnop_readlink     },
        { &vnop_reclaim_desc,       (vnodeop) lustre_vnop_reclaim      },
    //  { &vnop_remove_desc,        (vnodeop) lustre_vnop_remove       },
    //  { &vnop_removexattr_desc,   (vnodeop) lustre_vnop_removexattr  },
    //  { &vnop_rename_desc,        (vnodeop) lustre_vnop_rename       },
    //  { &vnop_revoke_desc,        (vnodeop) lustre_vnop_revoke       },
    //  { &vnop_rmdir_desc,         (vnodeop) lustre_vnop_rmdir        },
    //  { &vnop_searchfs_desc,      (vnodeop) lustre_vnop_searchfs     },
    //  { &vnop_select_desc,        (vnodeop) lustre_vnop_select       },
    //  { &vnop_setattr_desc,       (vnodeop) lustre_vnop_setattr      },
    //  { &vnop_setattrlist_desc,   (vnodeop) lustre_vnop_setattrlist  },            // not useful, implement setattr instead
    //  { &vnop_setxattr_desc,      (vnodeop) lustre_vnop_setxattr     },
    //  { &vnop_strategy_desc,      (vnodeop) lustre_vnop_strategy     },
    //  { &vnop_symlink_desc,       (vnodeop) lustre_vnop_symlink      },
    //  { &vnop_whiteout_desc,      (vnodeop) lustre_vnop_whiteout     },
    //  { &vnop_write_desc,         (vnodeop) lustre_vnop_write        },
        { NULL, NULL }
};

// vnodeop_opv_desc points to our vnode operations array and to a place where the
// system, on successful registration, stores a final vnode array that's used to create our vnodes.
static struct vnodeopv_desc vnodeop_opv_desc = {
    &vnode_operations,                // opv_desc_vector_p
    vnodeop_entries                   // opv_desc_ops
};

// vnodeop_opv_desc_list is an array of vnodeopv_desc that allows us to register multiple vnode operations arrays at the same time.  A full-featured
// file system would use this to register different arrays for standard vnodes, device vnodes (VBLK and VCHR), and FIFO vnodes (VFIFO).  In our case, we only
// support standard vnodes, so our array only has one entry.
static struct vnodeopv_desc * vnodeop_opv_desc_list[] = {
    &vnodeop_opv_desc
};

// vfs_ops is a structure that contains pointer to all of the vfsop routines. These are routines that operate on instances of the file system (rather than on vnodes).
static struct vfsops vfs_ops = {
    lustre_vfsop_mount,                             // vfs_mount
    lustre_vfsop_start,                             // vfs_start
    lustre_vfsop_unmount,                           // vfs_unmount
    lustre_vfsop_root,                              // vfs_root
    NULL,                                           // vfs_quotactl
    lustre_vfsop_getattr,                           // vfs_getattr
    lustre_vfsop_sync,                              // vfs_sync
    NULL,                                           // vfs_vget
    NULL,                                           // vfs_fhtovp
    NULL,                                           // vfs_vptofh
    NULL,                                           // vfs_init
    NULL,                                           // vfs_sysctl
    NULL,                                           // vfs_setattr
    NULL,                                           // vfs_ioctl
    NULL,                                           // vfs_vget_snapdir
    NULL, NULL, NULL, NULL, NULL                    // vfs_reserved
};

// vfs_entry describes the overall VFS plug-in.  It's passed as a parameter to vfs_fsadd to register this file system.
struct vfs_fsentry vfs_entry = {
    &vfs_ops,                                   // vfe_vfsops
    sizeof(vnodeop_opv_desc_list) / sizeof(*vnodeop_opv_desc_list), // vfe_vopcnt
    vnodeop_opv_desc_list,                      // vfe_opvdescs
    0,                                          // vfe_fstypenum, see VFS_TBLNOTYPENUM below
    "",                                         // vfe_fsname
                                                // vfe_flags
    VFS_TBLTHREADSAFE                           // we do our own internal locking and thus don't need funnel protection
    | VFS_TBLNOMACLABEL
    | VFS_TBLFSNODELOCK                         // ditto
    | VFS_TBLNOTYPENUM                          // we don't have a pre-defined file system type (the VT_XXX constants in <sys/vnode.h>); VFS should dynamically assign us a type
    | VFS_TBL64BITREADY,                        // we are 64-bit aware; our mount, ioctl and sysctl entry points can be called by both 32-bit and 64-bit processes; we're will
                                                // use the type of process to interpret our arguments (if they're not 32/64-bit invariant)
    {NULL, NULL}                                // vfe_reserv
};

static vfstable_t vfs_table_ref = NULL;

#pragma mark - Memory and Locks

// Disposes of lustre_os_malloc_tag and lustre_lock_group.
static void lustre_terminate_memory_and_locks(void)
{
    if (lustre_lock_group != NULL) {
        lck_grp_free(lustre_lock_group);
        lustre_lock_group = NULL;
    }
    if (lustre_os_malloc_tag != NULL) {
        OSMalloc_Tagfree(lustre_os_malloc_tag);
        lustre_os_malloc_tag = NULL;
    }
}

// Initialises of lustre_os_malloc_tag and lustre_lock_group.
static kern_return_t lustre_init_memory_and_locks(void)
{
    kern_return_t   err;

    err = KERN_SUCCESS;
    lustre_os_malloc_tag = OSMalloc_Tagalloc("com.ciderapps.lustre.Filesystem", OSMT_DEFAULT);
    if (lustre_os_malloc_tag == NULL) {
        err = KERN_FAILURE;
    }
    if (err == KERN_SUCCESS) {
        lustre_lock_group = lck_grp_alloc_init("com.ciderapps.lustre.Filesystem", LCK_GRP_ATTR_NULL);
        if (lustre_lock_group == NULL) {
            err = KERN_FAILURE;
        }
    }

    // Clean up.

    if (err != KERN_SUCCESS) {
        lustre_terminate_memory_and_locks();
    }

    LUSTRE_BUG_ON((err != KERN_SUCCESS) || (lustre_os_malloc_tag == NULL) );
    LUSTRE_BUG_ON((err != KERN_SUCCESS) || (lustre_lock_group    == NULL) );

    return err;
}

#pragma mark - Extension Start/Stop

// Called by the kernel to initialise the KEXT.  The main feature of this routine is a call to vfs_fsadd to register our VFS plug-in.
kern_return_t lustre_start(kmod_info_t * ki, void * d)
{
    kern_return_t result;

    LUSTRE_BUG_ON(vfs_table_ref != NULL);           // just in case we get loaded twice (which shouldn't ever happen)

    lustre_logging_alloc();
    
    result = lustre_init_memory_and_locks();

    if (result == KERN_SUCCESS) {
        strlcpy(vfs_entry.vfe_fsname, kLustreFilesystemName, MFSNAMELEN);
        if (vfs_fsadd(&vfs_entry, &vfs_table_ref) != 0) {
            result = KERN_FAILURE;
        }
    }

    if (result != KERN_SUCCESS) {
        lustre_terminate_memory_and_locks();
    }

    return result;
}

// Called by the kernel to terminate the KEXT.  The main feature of
// this routine is a call to vfs_fsremove to deregister our VFS plug-in.
// If this fails (which it will if any of our volumes mounted), the KEXT
// can't be unloaded.
kern_return_t lustre_stop(kmod_info_t * ki, void * d)
{
    kern_return_t result;

    if (vfs_fsremove(vfs_table_ref) == 0) {
        vfs_table_ref = NULL;

        lustre_terminate_memory_and_locks();
        
        result = KERN_SUCCESS;
    } else {
        result = KERN_FAILURE;
    }

    lustre_logging_free();

    return result;
}
