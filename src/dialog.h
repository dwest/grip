/* dialog.h
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

#ifndef GRIP_DIALOG_H
#define GRIP_DIALOG_H

/* Input routines */
void InputDialog(char *prompt,char *default_str,int len,char *doit,
		 GtkSignalFunc doitfunc,
		 char *cancel,GtkSignalFunc cancelfunc);
void ChangeStrVal(GtkWidget *widget,gpointer data);
GtkWidget *MakeStrEntry(GtkWidget **entry,char *var,char *name,
			int len,gboolean editable);
void ChangeIntVal(GtkWidget *widget,gpointer data);
GtkWidget *MakeNumEntry(GtkWidget **entry,int *var,char *name,int len);
void ChangeDoubleVal(GtkWidget *widget,gpointer data);
GtkWidget *MakeDoubleEntry(GtkWidget **entry,gdouble *var,char *name);
void ChangeBoolVal(GtkWidget *widget,gpointer data);
GtkWidget *MakeCheckButton(GtkWidget **button,gboolean *var,char *name);

#endif /* GRIP_DIALOG_H */
