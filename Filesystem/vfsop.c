//
//  vfsop.c
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

#include <libkern/libkern.h>
#include <libkern/OSMalloc.h>
#include <libkern/locks.h>
#include <sys/mount.h>
#include <mach/mach_types.h>
#include <sys/errno.h>
#include <sys/mount.h>
#include <sys/vnode.h>
#include <sys/vnode_if.h>
#include <sys/kernel_types.h>
#include <sys/stat.h>
#include <sys/dirent.h>
#include <sys/proc.h>
#include <sys/fcntl.h>
#include <sys/sysctl.h>

#include "vfsop.h"
#include "lustre.h"
#include "mount.h"
#include "logging.h"
#include "volume.h"
#include "assert.h"

#pragma mark - Core Data Structures

extern errno_t (**vnode_operations)(void *);

extern struct vnodeopv_entry_desc   lustre_vnodeop_entries[];
extern struct vnodeopv_desc         lustre_vnodeop_opv_desc;

#pragma mark - Helpers

// Returns the root vnode for the volume, creating it if necessary.  The resulting vnode has a I/O reference count, which the caller is responsible for releasing (using vnode_put) or passing along to its caller.
static errno_t lustre_vfsop_get_root_vnode_creating_if_necessary(struct lustre_volume * volume, vnode_t * vnode)
{
    errno_t     error;
    vnode_t     result_vn;
    uint32_t    vid;
    
    // Pre-conditions
    
    LUSTRE_BUG_ON(!volume);
    LUSTRE_BUG_ON(!vnode);
    LUSTRE_BUG_ON(*vnode);
    
    // resultVN holds vnode we're going to return in *vnPtr.  If this ever goes non-NULL,
    // we're done.
    
    result_vn = NULL;
    
    // First lock the revelant fields of the mount point.
    
    lck_mtx_lock(volume->root_lock);
    
    do {
        LUSTRE_BUG_ON(result_vn);       // no point looping if we already have a result
        
#if MACH_ASSERT
        lck_mtx_assert(volume->root_lock, LCK_MTX_ASSERT_OWNED);
#endif
        
        if (volume->root_attaching) {
            // If someone else is already trying to create the root vnode, wait for
            // them to get done.  Note that msleep will unlock and relock volume->root_lock,
            // so once it returns we have to loop and start again from scratch.
            
            volume->root_waiting = TRUE;
            
            (void) msleep(&volume->root_vnode, volume->root_lock, PINOD, __FUNCTION__, NULL);
            
            error = EAGAIN;
        } else if (volume->root_vnode == NULL) {
            vnode_t                 new_vn;
            struct vnode_fsparam    params;
            
            // There is no root vnode, so create it.  While we're creating it, we
            // drop our lock (to avoid the possibility of deadlock), so we set
            // fRootAttaching to stall anyone else entering the code (and eliminate
            // the possibility of two people trying to create the same vnode).
            
            volume->root_attaching = TRUE;
            
            lck_mtx_unlock(volume->root_lock);
            
            new_vn = NULL;
            
            params.vnfs_mp         = volume->mount_point;
            params.vnfs_vtype      = VDIR;
            params.vnfs_str        = NULL;
            params.vnfs_dvp        = NULL;
            params.vnfs_fsnode     = NULL;
            params.vnfs_vops       = vnode_operations;
            params.vnfs_markroot   = TRUE;
            params.vnfs_marksystem = FALSE;
            params.vnfs_rdev       = 0;                                 // we don't currently support VBLK or VCHR
            params.vnfs_filesize   = 0;                                 // not relevant for a directory
            params.vnfs_cnp        = NULL;
            params.vnfs_flags      = VNFS_NOCACHE | VNFS_CANTCACHE;     // do no vnode name caching
            
            error = vnode_create(VNCREATE_FLAVOR, sizeof(params), &params, &new_vn);
            
            LUSTRE_BUG_ON(error != 0);
            LUSTRE_BUG_ON(!new_vn);
            
            lck_mtx_lock(volume->root_lock);
            
            if (error == 0) {
                // If we successfully create the vnode, it's time to install it as
                // the root.  No one else should have been able to get here, so
                // mtmp->fRootVNode should still be NULL.  If it's not, that's bad.
                
                LUSTRE_BUG_ON(volume->root_vnode);
                volume->root_vnode = new_vn;
                
                // Also let the VFS layer know that we have a soft reference to
                // the vnode.
                
                vnode_addfsref(new_vn);
                
                // If anyone got hung up on mtmp->fRootAttaching, unblock them.
                
                LUSTRE_BUG_ON(!volume->root_attaching);
                volume->root_attaching = FALSE;
                if (volume->root_waiting) {
                    wakeup(&volume->root_vnode);
                    volume->root_waiting = FALSE;
                }
                
                // Set up the function result.  Note that vnode_create creates the
                // vnode with an I/O reference count, so we can just return it
                // directly.
                
                result_vn = volume->root_vnode;
            }
        } else {
            vnode_t candidate_vn;
            
            // We already have a root vnode.  Drop our lock (again, to avoid deadlocks)
            // and get a reference on it, using the vnode ID (vid) to confirm that it's
            // still valid.  If that works, we're all set.  Otherwise, let's just start
            // again from scratch.
            
            candidate_vn = volume->root_vnode;
            
            vid = vnode_vid(candidate_vn);
            
            lck_mtx_unlock(volume->root_lock);
            
            error = vnode_getwithvid(candidate_vn, vid);
            
            if (error == 0) {
                // All ok.   vnode_getwithvid has taken an I/O reference count on the
                // vnode, so we can just return it to the caller.  This reference
                // prevents the vnode from being reclaimed in the interim.
                
                result_vn = candidate_vn;
            } else {
                // vnode_getwithvid failed.  This is most likely because the vnode
                // has been reclaimed between dropping the lock and calling vnode_getwithvid.
                // That's cool.  We just loop again, and this time we'll get the updated
                // results (hopefully).
                
                error = EAGAIN;
            }
            
            // We need to reacquire the lock because that's the loop invariant.
            // Strictly speaking we don't need to do this in the 'success' case,
            // but it makes the code simpler (and I don't care about the trivial
            // performance cost in this sample).
            
            lck_mtx_lock(volume->root_lock);
        }
        
        // resultVN should only be set if everything is OK.
        
        LUSTRE_BUG_ON(error != 0 && result_vn);
    } while (error == EAGAIN);
    
    lck_mtx_unlock(volume->root_lock);
    
    if (error == 0) {
        *vnode = result_vn;
    }
    
    return error;
}

#pragma mark - vfsops

// Called by VFS to mount an instance of our file system.
//
// mp is a reference to the kernel structure tracking this instance of the file system.
//
// devvp is either:
//   o an open vnode for the block device on which we're mounted, or
//   o NULL
// depending on the VFS_TBLLOCALVOL flag in the vfe_flags field of the vfs_fsentry that we registered.  In the former case, the first field of our file system specific
// mount arguments must be a pointer to a C string holding the UTF-8 path to the block device node.
//
// data is a pointer to our file system specific mount arguments in the address space of the current process (the one that called mount).  This is a parameter
// block passed to us by our mount tool telling us what to mount and how.  Because VFS_TBLLOCALVOL is set, the first field of this structure must be pointer to the
// path of the block device node; the kernel interprets this parameter, opening up the node for us.
//
// IMPORTANT:
// If VFS_TBLLOCALVOL is set, the first field of the file system specific mount parameters is interpreted by the kernel AND THE KERNEL INCREMENTS data TO POINT
// TO THE FIELD AFTER THE PATH.  We handle this by defining our mount parameter structure (struct lustre_volumeArgs) in two ways: for user space code, the first field
// (fDevNodePath) is a poiner to the block device node path; for kernel code, we omit this field.
//
// IMPORTANT:
// If your file system claims to be 64-bit ready (VFS_TBL64BITREADY is set), you must be prepared to handle mount requests from both 32- and 64-bit processes.  Thus,
// your file system specific mount parameters must be either 32/64-bit invariant (as is the case for this example), or you must intepret them differently depending
// on the type of process you're being called by (see proc_is64bit from <sys/proc.h>).
//
// context identifies the calling process.
errno_t lustre_vfsop_mount(mount_t mp, vnode_t devvp, user_addr_t data, vfs_context_t context)
{
    int                     error;
    struct lustre_volume *  volume;
    
    // Pre-conditions
    
    LUSTRE_BUG_ON(!mp);
    LUSTRE_BUG_ON(!data);
    LUSTRE_BUG_ON(!context);
    
    volume  = NULL;
    error     = 0;
    
    // This fs does not support updating a volume's state (for example,
    // upgrading it from read-only to read/write).
    
    if (vfs_isupdate(mp)) {
        error = ENOTSUP;
    }
    
    if (error == 0) {
        volume = lustre_volume_alloc();
        if (volume == NULL) {
            error = ENOMEM;
        } else {
            volume->mount_point = mp;
            vfs_setfsprivate(mp, volume);
        }
    }
    
    if (error == 0) {
        error = lustre_mount_setup(volume, data, context);
    }
    
    // Don't think you need to call vnode_setmountedon because the system does it for you.
    
    if (error == 0) {
        // Let the VFS layer know about the specifics of this volume.
        lustre_mount_setup_vfs(volume);
        
        if (volume->mount_args.force_failure) {
            
            // By setting the above to true, you can force a mount failure, which llows you to test the unmount path.
            os_log_info(lustre_logger_vfs, "mount succeeded, force failure");
            error = ENOTSUP;
        } else {
            os_log_info(lustre_logger_vfs, "Lustre: mount succeeded");
        }
    } else {
        os_log_error(lustre_logger_vfs, "Lustre: mount failed with error %d", error);
    }
    
    // If we return an error, our unmount vfsop is never called.  Thus, we have to clean up ourselves.
    
    if (error != 0) {
        lustre_vfsop_unmount(mp, MNT_FORCE, context);
    }
    
    return error;
}

// Called by VFS to confirm the mount.
//
// mp is a reference to the kernel structure tracking this instance of the file system.
//
// flags is reserved.
//
// context identifies the calling process.
//
// This entry point isn't particularly useful; to avoid concurrency problems you should do all of your initialisation before returning from lustre_vfsop_mount.
//
// Moreover, it's not necessary to implement this because the kernel glue (VFS_START) a ignores NULL entry and returns ENOTSUP, and the caller ignores that error.
//
// Still, I implement it just in case.
errno_t lustre_vfsop_start(mount_t mp, int flags, vfs_context_t context)
{
    struct lustre_volume * volume;
    
    // Pre-conditions
    LUSTRE_BUG_ON(!mp);
    LUSTRE_BUG_ON(!context);
    
    volume = lustre_volume_peek(mp);
    
    return 0;
}

// Called by VFS to unmount a volume.  Also called by our lustre_vfsop_mount code to clean up if something goes wrong.
//
// mp is a reference to the kernel structure tracking this instance of the file system.
//
// mntflags is a set of flags; currently only MNT_FORCE is defined.
//
// context identifies the calling process.
errno_t lustre_vfsop_unmount(mount_t mp, int mntflags, vfs_context_t context)
{
    errno_t                 error;
    boolean_t               forced_unmount;
    struct lustre_volume *  volume;
    int                     flush_flags;
    
    LUSTRE_BUG_ON(!mp);
    LUSTRE_BUG_ON(!context);
    
    volume  = NULL;
    
    // Implementation
    
    forced_unmount = (mntflags & MNT_FORCE) != 0;
    if (forced_unmount) {
        flush_flags = FORCECLOSE;
    } else {
        flush_flags = 0;
    }
    
    // Prior to calling us, VFS has flushed all regular vnodes (that is, it called
    // vflush with SKIPSWAP, SKIPSYSTEM, and SKIPROOT set).  Now we have to flush
    // all vnodes, including the root.  If flushFlags is FORCECLOSE, this is a
    // forced unmount (which will succeed even if there are files open on the volume).
    // In this case, if a vnode can't be flushed, vflush will disconnect it from the
    // mount.
    
    error = vflush(mp, NULL, flush_flags);
    
    // Clean up the file system specific data attached to the mount.
    
    if (error == 0) {
        
        // If lustre_vfsop_mount fails, it's possible for us to end up here without a
        // valid file system specific mount record.  We skip the clean up if
        // that happens.
        
        volume = vfs_fsprivate(mp);
        
        if (volume != NULL) {
            error = lustre_mount_teardown(volume);
            if (error != 0) {
                os_log_error(lustre_logger_vfs, "Failed to store volume core");
                return error;
            }
            
            // Prior to calling us, VFS ensures that no one is running within
            // our file system.  Thus, neither of these flags should be set.
            
            LUSTRE_BUG_ON(volume->root_attaching);
            LUSTRE_BUG_ON(volume->root_waiting);
            
            // The vflush, above, forces VFS to reclaim any vnodes on our volume.
            // Thus, fRootVNode should be NULL.
            
            LUSTRE_BUG_ON(volume->root_vnode);
            
            lustre_volume_ref_count_dec(volume);
        }
        
        os_log_info(lustre_logger_vfs, "unmount success");
    }
    
    if (error != 0) {
        os_log_error(lustre_logger_vfs, "unmount failed with error %d\n", error);
    }
    
    return error;
}

// Called by VFS to get the root vnode of this instance of the file system.
//
// mp is a reference to the kernel structure tracking this instance of the file system.
//
// vpp is a pointer to a vnode reference.  On success, we must set this to the root vnode.  We must have an I/O reference on that vnode, and it's the caller's responsibility to release it.
//
// context identifies the calling process.
//
// Our implementation is fairly simple
errno_t lustre_vfsop_root(mount_t mp, struct vnode **vpp, vfs_context_t context)
{
    errno_t                 error;
    vnode_t                 vn;
    struct lustre_volume *  volume;
    
    // Pre-conditions
    
    LUSTRE_BUG_ON(!mp);
    LUSTRE_BUG_ON(!vpp);
    LUSTRE_BUG_ON(!context);
    
    // Trivial implementation
    
    volume  = lustre_volume_peek(mp);
    vn      = NULL;
    error   = lustre_vfsop_get_root_vnode_creating_if_necessary(volume, &vn);
    
    // Under all circumstances we set *vpp to vn.  That way, we satisfy the
    // post-condition, regardless of what VFS uses as the initial value for
    // *vpp.
    
    if (error == 0) {
        *vpp = vn;
    }
    
    LUSTRE_BUG_ON((error == 0) && (*vpp == NULL));

    return error;
}

// Called by VFS to get information about this instance of the file system.
//
// mp is a reference to the kernel structure tracking this instance of the file system.
//
// vap describes the attributes requested and the place to store the results.
//
// context identifies the calling process.
//
// Like lustre_vnop_getattr, you have two macros that let you a) return values easily (VFSATTR_RETURN), and b) see if you need to return a value (VFSATTR_IS_ACTIVE).
//
// Our implementation is trivial because we pre-calculated all of the file system attributes in a convenient form.
errno_t lustre_vfsop_getattr(mount_t mp, struct vfs_attr * attr, vfs_context_t context)
{
    struct lustre_volume *  mount;
    
    // Pre-conditions
    
    LUSTRE_BUG_ON(!mp);
    LUSTRE_BUG_ON(!attr);
    LUSTRE_BUG_ON(!context);
    
    mount = lustre_volume_peek(mp);
    
    lustre_mount_volume_get_attr(mount, attr);
    
    return 0;
}

// After receiving this calldown, a filesystem will be hooked into the mount list and should expect calls down from the VFS layer.
//
// Marks a mount as ready to be used.
//
// mp is a reference to the kernel structure tracking this instance of the file system.
//
// flags are ununsed
//
// context identifies the calling process.
//
errno_t lustre_vfsop_sync(struct mount *mp, int flags, vfs_context_t context)
{
    return 0;
}
