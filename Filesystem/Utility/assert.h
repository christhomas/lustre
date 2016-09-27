//
//  assert.h
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

#ifndef lustre_assert_h
#define lustre_assert_h

#include <kern/debug.h>

#define LUSTRE_STATIC_BUG_CONCAT_(a, b) a##b
#define LUSTRE_STATIC_BUG_CONCAT(a, b) LUSTRE_STATIC_BUG_CONCAT_(a, b)
#define LUSTRE_STATIC_BUG_ON(e) enum { LUSTRE_STATIC_BUG_CONCAT(LUSTRE_STATIC_BUG_CONCAT(LUSTRE_STATIC_BUG_CONCAT(assert_line_, __COUNTER__), _), __LINE__) = 1/(!(e)) }

static inline __attribute__((noreturn)) void lustre_bug_fn(const char * file, unsigned long line)
{
    panic("Lustre BUG, from: %s:%ld", file, line);
    
    while (1) {};
}

#define LUSTRE_BUG()            do { lustre_bug_fn(__FILE__, __LINE__); } while(0)
#define LUSTRE_BUG_ON(_cond)    do { if(_cond) LUSTRE_BUG(); } while(0)

#endif /* lustre_assert_h */
