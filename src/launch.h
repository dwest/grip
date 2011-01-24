/* launch.h
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

#ifndef GRIP_LAUNCH_H
#define GRIP_LAUNCH_H

/* Options to use when munging strings */
typedef struct {
  gboolean no_underscore;
  gboolean allow_high_bits;
  gboolean escape;
  gboolean no_lower_case;
  char allow_these_chars[256];
} StrTransPrefs;

int MakeArgs(char *str,GString **args,int maxargs);
void TranslateString(char *instr,GString *outstr,
		     char *(*trans_func)(char,void *,gboolean *),
		     void *user_data,gboolean do_munge_default,
		     StrTransPrefs *prefs);
char *ReallocStrcat(char *dest, const char *src);
char *MungeString(char *str,StrTransPrefs *prefs);
int MakeTranslatedArgs(char *str,GString **args,int maxargs,
		       char *(*trans_func)(char,void *,gboolean *),
		       void *user_data,gboolean do_munge_default,
		       StrTransPrefs *prefs);
/*
void ArgsToLocale(GString **args);
*/
void TranslateAndLaunch(char *cmd,char *(*trans_func)(char,void *,gboolean *),
			void *user_data,gboolean do_munge_default,
			StrTransPrefs *prefs,void (*close_func)(void *),
			void *close_user_data);

#endif /* ifndef GRIP_LAUNCH_H */
