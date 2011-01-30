/* parsecfg.h
 *
 * Copyright (c) 1998-2002  Mike Oliphant <oliphant@gtk.org>
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

#ifndef GRIP_PARSECFG_H
#define GRIP_PARSECFG_H

/* Config entry types */

typedef enum
{
  CFG_ENTRY_STRING,
  CFG_ENTRY_BOOL,
  CFG_ENTRY_INT,
  CFG_ENTRY_LAST
} CFGEntryType;

typedef struct _cfg_entry
{
  char name[80];
  CFGEntryType type;
  int length;
  void *destvar;
} CFGEntry;


int LoadConfig(char *filename,char *name,int ver,int reqver,
	       CFGEntry *cfg);
gboolean SaveConfig(char *filename,char *name,int ver,CFGEntry *cfg);

#endif /* GRIP_PARSECFG_H */
