/* cdpar.h
 *
 * Copyright 1999-2004 by Mike Oliphant (grip@nostatic.org)
 *
 *   http://www.nostatic.org/grip
 *
 * This program is free software; you can redistribute it and/or 
 * modify it under the terms of the GNU General Public License as 
 * published by the Free Software Foundation; either version 2 of the
 * License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307
 * USA
 */

#define size16 short
#define size32 int

#ifdef HAVE_CDDA_INTERFACE_H
#include <cdda_interface.h>
#include <cdda_paranoia.h>
#else
#include <cdda/cdda_interface.h>
#include <cdda/cdda_paranoia.h>
#endif
