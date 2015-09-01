/* main.c
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

#include <config.h>
#include <gnome.h>
#include <stdlib.h>

#include "grip.h"

static gint KillSession(GnomeClient* client, gpointer client_data);
static gint SaveSession(GnomeClient *client, gint phase, 
			GnomeSaveStyle save_style,
			gint is_shutdown, GnomeInteractStyle interact_style,
			gint is_fast, gpointer client_data);
static gint TimeOut(gpointer data);

gboolean do_debug=TRUE;
GtkWidget* grip_app;

/* popt table */
static char *geometry=NULL;
static char *config_filename=NULL;
static char *device=NULL;
static char *scsi_device=NULL;
static int force_small=FALSE;
static int local_mode=FALSE;
static int no_redirect=FALSE;
static int verbose=FALSE;

struct poptOption options[] = {
  { 
    "geometry",
    '\0',
    POPT_ARG_STRING,
    &geometry,
    0,
    N_("Specify the geometry of the main window"),
    N_("GEOMETRY")
  },
  {
    "config",
    '\0',
    POPT_ARG_STRING,
    &config_filename,
    0,
    N_("Specify the config file to use (in your home dir)"),
    N_("CONFIG")
  },
  { 
    "device",
    '\0',
    POPT_ARG_STRING,
    &device,
    0,
    N_("Specify the cdrom device to use"),
    N_("DEVICE")
  },
  { 
    "scsi-device",
    '\0',
    POPT_ARG_STRING,
    &scsi_device,
    0,
    N_("Specify the generic scsi device to use"),
    N_("DEVICE")
  },
  { 
    "small",
    '\0',
    POPT_ARG_NONE,
    &force_small,
    0,
    N_("Launch in \"small\" (cd-only) mode"),
    NULL
  },
  { 
    "local",
    '\0',
    POPT_ARG_NONE,
    &local_mode,
    0,
    N_("\"Local\" mode -- do not look up disc info on the net"),
    NULL
  },
  { 
    "no-redirect",
    '\0',
    POPT_ARG_NONE,
    &no_redirect,
    0,
    N_("Do not do I/O redirection"),
    NULL
  },
  { 
    "verbose",
    '\0',
    POPT_ARG_NONE,
    &verbose,
    0,
    N_("Run in verbose (debug) mode"),
    NULL
  },
  {
    NULL,
    '\0',
    0,
    NULL,
    0,
    NULL,
    NULL
  }
};

void Debug(char *fmt,...)
{
  va_list args;
  char *msg;

  if(do_debug) {
    va_start(args,fmt);

    msg=g_strdup_vprintf(fmt,args);
    if(msg) {
      g_printerr("%s", msg);
      g_free(msg);
    }
  }

  va_end(args);
}

int Cmain(int argc, char* argv[])
{
  GnomeClient *client;

  /* Unbuffer stdout */
  setvbuf(stdout, 0, _IONBF, 0);

  /* setup locale, i18n */
  gtk_set_locale();
  bindtextdomain(GETTEXT_PACKAGE,GNOMELOCALEDIR);
  textdomain(GETTEXT_PACKAGE);

  gnome_program_init(PACKAGE,VERSION,LIBGNOMEUI_MODULE,argc,argv, 
		     GNOME_PARAM_POPT_TABLE,options,
		     GNOME_PROGRAM_STANDARD_PROPERTIES,NULL);

  bind_textdomain_codeset(GETTEXT_PACKAGE, "UTF8");
  setenv("CHARSET","UTF-8",1);

  /* Session Management */
  
  client=gnome_master_client();
  gtk_signal_connect(GTK_OBJECT(client),"save_yourself",
		     GTK_SIGNAL_FUNC(SaveSession),argv[0]);
  gtk_signal_connect(GTK_OBJECT(client),"die",
		     GTK_SIGNAL_FUNC(KillSession),NULL);
  

  do_debug=verbose;

  if(scsi_device) printf("scsi=[%s]\n",scsi_device);

  /* Start a new Grip app */
  grip_app=GripNew(geometry,device,scsi_device,config_filename,
		   force_small,local_mode,
		   no_redirect);

  gtk_widget_show(grip_app);

  gtk_timeout_add(1000,TimeOut,0);

  gtk_main();

  return 0;
}

/* Save the session */
static gint SaveSession(GnomeClient *client, gint phase,
			GnomeSaveStyle save_style,
			gint is_shutdown, GnomeInteractStyle interact_style,
			gint is_fast, gpointer client_data)
{
  gchar** argv;
  guint argc;

  /* allocate 0-filled, so it will be NULL-terminated */
  argv = g_malloc0(sizeof(gchar*)*4);
  argc = 1;

  argv[0] = client_data;

  gnome_client_set_clone_command(client, argc, argv);
  gnome_client_set_restart_command(client, argc, argv);

  return TRUE;
}

/* Kill Session */
static gint KillSession(GnomeClient* client, gpointer client_data)
{
  gtk_main_quit();

  return TRUE;
}

static gint TimeOut(gpointer data)
{
  GripUpdate(grip_app);

  return TRUE;
}
