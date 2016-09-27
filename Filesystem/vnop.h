//
//  vnop.h
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

#ifndef lustre_vnop_h
#define lustre_vnop_h

errno_t lustre_vnop_close(struct vnop_close_args *ap);
errno_t lustre_vnop_getattr(struct vnop_getattr_args *ap);
errno_t lustre_vnop_lookup(struct vnop_lookup_args *ap);
errno_t lustre_vnop_open(struct vnop_open_args *ap);
errno_t lustre_vnop_read_dir(struct vnop_readdir_args *ap);
errno_t lustre_vnop_reclaim(struct vnop_reclaim_args *ap);

#endif /* lustre_vnop_h */
