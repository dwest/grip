/* dialog.c
 *
 * Copyright (c) 1998-2004  Mike Oliphant <grip@nostatic.org>
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

#include <stdio.h>
#include <stdlib.h>
#include <gtk/gtk.h>
#include <gnome.h>
#include "dialog.h"

/*
 * Display a gtk message dialog with warning icon, OK button,
 * and the message passed in.
 */
void GripWarnDialog(GtkWidget *parent, gchar *message)
{
    GtkWidget *dialog = gtk_message_dialog_new(GTK_WINDOW(parent),
                                               GTK_DIALOG_DESTROY_WITH_PARENT,
                                               GTK_MESSAGE_WARNING,
                                               GTK_BUTTONS_OK,
                                               _(message));
    gtk_dialog_run(GTK_DIALOG(dialog));
    gtk_widget_destroy(dialog);
}

/*
 * Display a gtk message dialog with error icon, OK button,
 * and the message passed in.
 */
void GripErrorDialog(GtkWidget *parent, gchar *message)
{
    GtkWidget *dialog = gtk_message_dialog_new(GTK_WINDOW(parent),
                                               GTK_DIALOG_DESTROY_WITH_PARENT,
                                               GTK_MESSAGE_ERROR,
                                               GTK_BUTTONS_OK,
                                               _(message));
    gtk_dialog_run(GTK_DIALOG(dialog));
    gtk_widget_destroy(dialog);
}

/*
 * Display a gtk message dialog asking user a question.
 * User can press 'No' or 'Yes'. Return TRUE if 'Yes' is 
 * pressed, otherwise return FALSE.
 */
gboolean GripConfirmDialog(GtkWidget *parent, gchar *message)
{
    GtkWidget *dialog;
    gint response;

    dialog = gtk_message_dialog_new(GTK_WINDOW(parent),
                                    GTK_DIALOG_DESTROY_WITH_PARENT,
                                    GTK_MESSAGE_QUESTION,
                                    GTK_BUTTONS_YES_NO,
                                    _(message));
    response = gtk_dialog_run(GTK_DIALOG(dialog));
    gtk_widget_destroy(dialog);
    
    return response == GTK_RESPONSE_YES ? TRUE : FALSE;
}

void InputDialog(char *prompt,char *default_str,int len,char *doit,
		 GtkSignalFunc doitfunc,
		 char *cancel,GtkSignalFunc cancelfunc)
{
  GtkWidget *dialog;
  GtkWidget *label;
  GtkWidget *doitbutton;
  GtkWidget *cancelbutton;
  GtkWidget *entry;

  dialog=gtk_dialog_new();

  gtk_container_border_width(GTK_CONTAINER(GTK_DIALOG(dialog)->vbox),5);

  label=gtk_label_new(prompt);
  gtk_box_pack_start(GTK_BOX(GTK_DIALOG(dialog)->vbox),label,TRUE,TRUE,0);
  gtk_widget_show(label);

  entry=gtk_entry_new_with_max_length(len);
  if(default_str) gtk_entry_set_text(GTK_ENTRY(entry),default_str);
  gtk_box_pack_start(GTK_BOX(GTK_DIALOG(dialog)->vbox),entry,TRUE,TRUE,0);
  gtk_widget_show(entry);

  doitbutton=gtk_button_new_with_label(doit);
  if(doitfunc)
    gtk_signal_connect(GTK_OBJECT(doitbutton),"clicked",
		       doitfunc,(gpointer)entry);
  gtk_signal_connect_object(GTK_OBJECT(doitbutton),"clicked",
			    GTK_SIGNAL_FUNC(gtk_widget_destroy),
			    GTK_OBJECT(dialog));
  gtk_box_pack_start(GTK_BOX(GTK_DIALOG(dialog)->action_area),doitbutton,
		     TRUE,TRUE,0);
  gtk_widget_show(doitbutton);

  if(cancel) {
    cancelbutton=gtk_button_new_with_label(cancel);
    if(cancelfunc)
      gtk_signal_connect(GTK_OBJECT(cancelbutton),"clicked",
			 cancelfunc,NULL);
    gtk_signal_connect_object(GTK_OBJECT(cancelbutton),"clicked",
			      GTK_SIGNAL_FUNC(gtk_widget_destroy),
			      GTK_OBJECT(dialog));
    gtk_box_pack_start(GTK_BOX(GTK_DIALOG(dialog)->action_area),cancelbutton,
		       TRUE,TRUE,0);
    gtk_widget_show(cancelbutton);
  }

  gtk_widget_show(dialog);

  gtk_grab_add(dialog);
}

void ChangeStrVal(GtkWidget *widget,gpointer data)
{
  strcpy((char *)data,gtk_entry_get_text(GTK_ENTRY(widget)));
}

GtkWidget *MakeStrEntry(GtkWidget **entry,char *var,char *name,
			int len,gboolean editable)
{
  GtkWidget *widget;
  GtkWidget *label;
  GtkWidget *hbox;

  hbox=gtk_hbox_new(FALSE,5);

  label=gtk_label_new(name);
  gtk_label_set_justify(GTK_LABEL(label),GTK_JUSTIFY_LEFT);
  gtk_box_pack_start(GTK_BOX(hbox),label,FALSE,FALSE,0);
  gtk_widget_show(label);

  widget=gtk_entry_new_with_max_length(len);
  gtk_entry_set_editable(GTK_ENTRY(widget),editable);
  if(var) {
    gtk_entry_set_text(GTK_ENTRY(widget),var);

    gtk_signal_connect(GTK_OBJECT(widget),"changed",
		       GTK_SIGNAL_FUNC(ChangeStrVal),(gpointer)var);
  }

  gtk_box_pack_start(GTK_BOX(hbox),widget,TRUE,TRUE,0);

  gtk_entry_set_position(GTK_ENTRY(widget),0);

  gtk_widget_show(widget);

  if(entry) *entry=widget;

  return hbox;
}

void ChangeIntVal(GtkWidget *widget,gpointer data)
{
  *((int *)data)=atoi(gtk_entry_get_text(GTK_ENTRY(widget)));
}

GtkWidget *MakeNumEntry(GtkWidget **entry,int *var,char *name,int len)
{
  GtkWidget *widget;
  char buf[80];
  GtkWidget *label;
  GtkWidget *hbox;

  hbox=gtk_hbox_new(FALSE,5);

  label=gtk_label_new(name);
  gtk_box_pack_start(GTK_BOX(hbox),label,FALSE,FALSE,0);
  gtk_widget_show(label);

  widget=gtk_entry_new_with_max_length(len);
  gtk_widget_set_usize(widget,len*8+5,0);

  if(var) {
    sprintf(buf,"%d",*var);
    gtk_entry_set_text(GTK_ENTRY(widget),buf);
    gtk_signal_connect(GTK_OBJECT(widget),"changed",
		       GTK_SIGNAL_FUNC(ChangeIntVal),(gpointer)var);
  }

  gtk_box_pack_end(GTK_BOX(hbox),widget,FALSE,FALSE,0);
  gtk_widget_show(widget);

  if(entry) *entry=widget;

  return hbox;
}

void ChangeDoubleVal(GtkWidget *widget,gpointer data)
{
  *((gdouble *)data)=gtk_spin_button_get_value_as_float
    (GTK_SPIN_BUTTON(widget));
}

GtkWidget *MakeDoubleEntry(GtkWidget **entry,gdouble *var,char *name)
{
  GtkWidget *widget;
  GtkWidget *label;
  GtkWidget *hbox;
  GtkObject *adj;

  hbox=gtk_hbox_new(FALSE,5);

  label=gtk_label_new(name);
  gtk_box_pack_start(GTK_BOX(hbox),label,FALSE,FALSE,0);
  gtk_widget_show(label);

  adj=gtk_adjustment_new(0,0,1.0,0.001,0.1,0);

  widget=gtk_spin_button_new(GTK_ADJUSTMENT(adj),0.1,3);



  /***************************



  gtk_widget_set_usize(widget,
		       gdk_string_width((widget)->style->font,
					"0.000")+25,0);


  ************************/




  if(var) {
    gtk_spin_button_set_value(GTK_SPIN_BUTTON(widget),*var);
    gtk_signal_connect(GTK_OBJECT(widget),"changed",
		       GTK_SIGNAL_FUNC(ChangeDoubleVal),(gpointer)var);
  }

  gtk_box_pack_end(GTK_BOX(hbox),widget,FALSE,FALSE,0);
  gtk_widget_show(widget);

  if(entry) *entry=widget;

  return hbox;
}

void ChangeBoolVal(GtkWidget *widget,gpointer data)
{
  *((gboolean *)data)=!*((gboolean *)data);
}

GtkWidget *MakeCheckButton(GtkWidget **button,gboolean *var,char *name)
{
  GtkWidget *widget;

  widget=gtk_check_button_new_with_label(name);

  if(var) {
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(widget),
				 *var);
    gtk_signal_connect(GTK_OBJECT(widget),"clicked",
		       GTK_SIGNAL_FUNC(ChangeBoolVal),
		       (gpointer)var);
  }

  if(button) *button=widget;

  return widget;
}
