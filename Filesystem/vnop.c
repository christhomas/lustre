//
//  vnop.c
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

#include "lustre.h"
#include "mount.h"
#include "volume.h"
#include "assert.h"
#include "extensions.h"
#include "logging.h"

extern int (**vnode_operations)(void *);

// Called by higher-level code within our VFS plug-in to reclaim a vnode, that is, for us to 'forget' about it.  We only 'know' about one vnode,
// the root vnode, so this code is much easier than it would be in a real file system.
static void lustre_mount_detach_root_vnode(struct lustre_volume * volume, vnode_t vn)
{
    LUSTRE_BUG_ON(!volume);
    LUSTRE_BUG_ON(!vn);
    
    lck_mtx_lock(volume->root_lock);
    
    // We can ignore mtmp->fRootAttaching here because, if it's set, mtmp->fRootVNode
    // will be NULL.  And, if that's the case, we just do nothing and return.  That's
    // exactly the correct behaviour if the system tries to reclaim the vnode while
    // some other thread is in the process of attaching it.
    //
    // The following assert checks the assumption that makes this all work.
    
    LUSTRE_BUG_ON(volume->root_attaching);
    LUSTRE_BUG_ON(!volume->root_vnode);
    
    if (volume->root_vnode == NULL) {
        // Do nothing; someone beat us to the reclaim; nothing to do.
    } else {
        // The vnode we're reclaiming should be the root vnode.  If it isn't,
        // I want to know about it.
        
        LUSTRE_BUG_ON(volume->root_vnode != vn);
        
        // Tell VFS that we're removing our soft reference to the vnode.
        
        vnode_removefsref(volume->root_vnode);
        
        volume->root_vnode = NULL;
    }
    
    lck_mtx_unlock(volume->root_lock);
}

errno_t lustre_vnop_lookup(struct vnop_lookup_args * ap)
{
    errno_t                 error;
    vnode_t                 dvp;
    vnode_t *               vpp;
    struct componentname *  cnp;
    vfs_context_t           context;
    vnode_t                 vn;
    
    // Unpack arguments
    
    dvp     = ap->a_dvp;
    vpp     = ap->a_vpp;
    cnp     = ap->a_cnp;
    context = ap->a_context;
    
    // Pre-conditions
    
    LUSTRE_BUG_ON(!dvp);
    LUSTRE_BUG_ON(!vnode_isdir(dvp));
    LUSTRE_BUG_ON(!vpp);
    LUSTRE_BUG_ON(!cnp);
    LUSTRE_BUG_ON(!context);
    
    // Prepare for failure.
    
    vn = NULL;
    
    // Trivial implementation
    
    if (cnp->cn_flags & ISDOTDOT) {
        // Implement lookup for ".." (that is, the parent directory).  As we currently
        // only support one directory (the root directory) and the parent of the root
        // is always the root, this is trivial (and, incidentally, exactly the same
        // as the code for ".", but that wouldn't be true in a more general VFS plug-in).
        // We just get an I/O reference on dvp and return that.
        
        error = vnode_get(dvp);
        if (error == 0) {
            vn = dvp;
        }
    } else if ( (cnp->cn_namelen == 1) && (cnp->cn_nameptr[0] == '.') ) {
        // Implement lookup for "." (that is, this directory).  Just get an I/O reference
        // to dvp and return that.
        
        error = vnode_get(dvp);
        if (error == 0) {
            vn = dvp;
        }
    } else {
        error = ENOENT;
    }
    
    // Under all circumstances we set *vpp to vn.  That way, we satisfy the
    // post-condition, regardless of what VFS uses as the initial value for
    // *vpp.
    
    *vpp = vn;
    
    // Post-conditions
    
    LUSTRE_BUG_ON((error == 0) && (*vpp == NULL));
    
    return error;
}

// Called by VFS to open a vnode for access.
//
// vp is the vnode that's being opened.
//
// mode contains the flags passed to open (things like FREAD).
//
// context identifies the calling process.
//
// This entry is rarely useful because VFS can read a file vnode without ever opening it, thus any work that you'd usually do here you have to do lazily in your read/write entry points.
//
// Regardless, in our implementation we have nothing to do.
errno_t lustre_vnop_open(struct vnop_open_args * ap)
{
    vnode_t         vp;
    int             mode;
    vfs_context_t   context;
    
    // Unpack arguments
    
    vp      = ap->a_vp;
    mode    = ap->a_mode;
    context = ap->a_context;
    
    // Pre-conditions
    
    LUSTRE_BUG_ON(!context);
    
    // Empty implementation
    
    LUSTRE_BUG_ON(!vnode_isdir(vp));
    
    return 0;
}

// Called by VFS to close a vnode for access.
//
// vp is the vnode that's being closed.
//
// fflags contains the flags associated with the close (things like FREAD).
//
// context identifies the calling process.
//
// This entry is not as useful as you might think because a vnode can be accessed after the last close (if, for example, if has been memory mapped).  In most cases
// the work that you might think to do here, you end up doing in lustre_vnop_inactive.
//
// Regardless, in our implementation we have nothing to do.
errno_t lustre_vnop_close(struct vnop_close_args * ap)
{
    vnode_t         vp;
    int             fflag;
    vfs_context_t   context;
    
    // Unpack arguments
    
    vp      = ap->a_vp;
    fflag   = ap->a_fflag;
    context = ap->a_context;
    
    // Pre-conditions
    
    LUSTRE_BUG_ON(!context);
    
    // Empty implementation
    
    LUSTRE_BUG_ON(!vnode_isdir(vp));
    
    return 0;
}

// Called by VFS to get information about a vnode (this is called by the VFS implementation of <x-man-page://2/stat> and <x-man-page://2/getattrlist>).
//
// vp is the vnode whose information is requested.
//
// vap describes the attributes requested and the place to store the results.
//
// context identifies the calling process.
//
// You have two options for doing this:
//
// o For attributes whose values you have readily available, use the VATTR_RETURN
//   macro to unilaterally return the value.
//
// o For attributes whose values are hard to calculate, use VATTR_IS_ACTIVE to see
//   if the caller requested the attribute and, if so, copy the value into the
//   appropriate field.
//
// Our implementation is trivial; we just return statically configured values.
errno_t lustre_vnop_getattr(struct vnop_getattr_args * ap)
{
    vnode_t                 vp;
    struct vnode_attr *     vap;
    vfs_context_t           context;
    struct lustre_volume *  volume;
    
    // Unpack arguments
    
    vp      = ap->a_vp;
    vap     = ap->a_vap;
    context = ap->a_context;
    
    // Pre-conditions
    
    LUSTRE_BUG_ON(!vap);
    LUSTRE_BUG_ON(!context);
    
    // Trivial implementation
    
    LUSTRE_BUG_ON(!vnode_isdir(vp));
    
    volume = vfs_fsprivate(vnode_mount(vp));

    VATTR_RETURN(vap, va_rdev,          0);
    VATTR_RETURN(vap, va_nlink,         2);           // traditional for directories
    VATTR_RETURN(vap, va_data_size,     2 * sizeof(struct dirent));
    VATTR_RETURN(vap, va_uid,           kLustreFilesystemNobodyOwnerId);
    VATTR_RETURN(vap, va_gid,           kLustreFilesystemNobodyGroupId);
    VATTR_RETURN(vap, va_mode,          S_IFDIR | S_IRUSR | S_IXUSR | S_IRGRP | S_IXGRP | S_IROTH | S_IXOTH);
    VATTR_RETURN(vap, va_create_time,   volume->create_time);
    VATTR_RETURN(vap, va_access_time,   volume->access_time);
    VATTR_RETURN(vap, va_modify_time,   volume->modify_time);
    VATTR_RETURN(vap, va_backup_time,   volume->backup_time);
    VATTR_RETURN(vap, va_fileid,        2);
    VATTR_RETURN(vap, va_fsid,          lustre_volume_fsid(volume).val[0]);
    
    return 0;
}

errno_t lustre_vnop_read_dir(struct vnop_readdir_args * ap)
{
    errno_t         error;
    vnode_t         vp;
    struct uio *    uio;
    int             flags;
    int *           eofflagPtr;
    int             eofflag;
    int *           numdirentPtr;
    int             numdirent;
    vfs_context_t   context;
    
    // Unpack arguments
    
    vp           = ap->a_vp;
    uio          = ap->a_uio;
    flags        = ap->a_flags;
    eofflagPtr   = ap->a_eofflag;
    numdirentPtr = ap->a_numdirent;
    context      = ap->a_context;
    
    // Pre-conditions
    
    LUSTRE_BUG_ON(!uio);
    LUSTRE_BUG_ON(!context);
    
    // An easy, but non-trivial, implementation
    
    LUSTRE_BUG_ON(!vnode_isdir(vp));
    
    eofflag = FALSE;
    numdirent = 0;
    
    if ((flags & VNODE_READDIR_EXTENDED) || (flags & VNODE_READDIR_REQSEEKOFF)) {
        // We only need to support these flags if we want to support being exported by NFS.
        error = EINVAL;
    } else {
        struct dirent   thisItem;
        off_t           index;
        
        error = 0;
        
        // Set up thisItem.
        
        thisItem.d_fileno = 2;
        thisItem.d_reclen = sizeof(thisItem);
        thisItem.d_type = DT_DIR;
        strncpy(thisItem.d_name, ".", 1);
        thisItem.d_namlen = strlen(".");
        
        // We set uio_offset to the directory item index * 7 to:
        //
        // o Illustrate the points about uio_offset usage in the comment above.
        //
        // o Allow us to check that we're getting valid input.
        //
        // However, be aware of the comments above about not trusting uio_offset;
        // the client can set it to an arbitrary value using lseek.
        
        LUSTRE_BUG_ON((uio_offset(uio) % 7) != 0);
        
        index = uio_offset(uio) / 7;
        
        // If we're being asked for the first directory entry...
        
        if (index == 0) {
            error = lustre_extension_uio_move(&thisItem, sizeof(thisItem), uio);
            if (error == 0) {
                numdirent += 1;
                index += 1;
            }
        }
        
        // If we're being asked for the second directory entry...
        
        if ((error == 0) && (index == 1)) {
            strncpy(thisItem.d_name, "..", 2);
            thisItem.d_namlen = strlen("..");
            error = lustre_extension_uio_move(&thisItem, sizeof(thisItem), uio);
            if (error == 0) {
                numdirent += 1;
                index += 1;
            }
        }
        
        // If we failed because there wasn't enough space in the user's buffer,
        // just swallow the error.  This will result getdirentries returning
        // less than the buffer size (possibly even zero), and the caller is
        // expected to cope with that.
        
        if (error == ENOBUFS) {
            error = 0;
        }
        
        // Update uio_offset.
        
        uio_setoffset(uio, index * 7);
        
        // Determine if we're at the end of the directory.
        
        eofflag = (index > 1);
    }
    
    // Copy out any information that's requested by the caller.
    
    if (eofflagPtr != NULL) {
        *eofflagPtr = eofflag;
    }
    if (numdirentPtr != NULL) {
        *numdirentPtr = numdirent;
    }
    
    return error;
}

errno_t lustre_vnop_reclaim(struct vnop_reclaim_args * ap)
{
    vnode_t                 vnode;
    vfs_context_t       	context;
    struct lustre_volume *  volume;
    
    vnode       = ap->a_vp;
    context     = ap->a_context;
    
    LUSTRE_BUG_ON(!vnode);
    LUSTRE_BUG_ON(!context);
    
    volume  = vfs_fsprivate(vnode_mount(vnode));
    
    lustre_mount_detach_root_vnode(volume, vnode);
    
    return 0;
}
