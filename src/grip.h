/* grip.h
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
 *
 */

#ifndef GRIP_H
#define GRIP_H

#include "config.h"
#include <gnome.h>
#include "cddev.h"
#include "discdb.h"
#include "pthread.h"
#include "launch.h"
#include "status_window.h"
#include "eggtrayicon.h"

#if defined(HAVE_CDDA_INTERFACE_H) || defined(HAVE_CDDA_CDDA_INTERFACE_H)
#define CDPAR
#endif

#define WINWIDTH 400
#define WINHEIGHT 450

#define WINHEIGHTEDIT 600

#define MIN_WINWIDTH 200
#define MIN_WINHEIGHT 80

#define MAX_NUM_CPU 16

#define RRand(range) (random()%(range))

#if defined(__linux__) || defined(__FreeBSD__) || defined(__NetBSD__) || defined(__osf__)  /* __osf__ ?? */

#define MAILER "/usr/sbin/sendmail -i -t"

#elif defined(__sparc__)

#define MAILER "/usr/lib/sendmail -i -t"

#endif

typedef struct _grip_gui {
  GtkWidget *app;
  GtkWidget *winbox;
  GtkWidget *notebook;
  gboolean minimized;
  int win_width;
  int win_height;
  int win_height_edit;
  int win_width_min;
  int win_height_min;
  int x;
  int y;
  GtkStyle *style_wb;
  GtkStyle *style_LCD;
  GtkStyle *style_dark_grey;

  GtkWidget *disc_name_label;
  GtkWidget *disc_artist_label;
  GtkListStore *track_list_store;
  GtkWidget *track_list;

  GtkWidget *current_track_label;
  int time_display_mode;
  GtkWidget *play_time_label;
  GtkWidget *rip_indicator;
  GtkWidget *lcd_smile_indicator;
  GtkWidget *mp3_indicator[MAX_NUM_CPU];
  GtkWidget *discdb_indicator;
  GtkWidget *control_button_box;
  GtkWidget *controls;
  gboolean control_buttons_visible;
  GdkCursor *wait_cursor;

  gboolean track_edit_visible;
  GtkWidget *track_edit_box;
  GtkWidget *artist_edit_entry;
  GtkWidget *title_edit_entry;
  GtkWidget *id3_genre_combo;
  GList *id3_genre_item_list;
  GtkWidget *year_spin_button;
  GtkWidget *track_edit_entry;
  GtkWidget *multi_artist_box;
  GtkWidget *track_artist_edit_entry;
  GtkWidget *split_chars_entry;
  GtkWidget *multi_artist_button;
  GtkWidget *playopts;
  GtkWidget *playlist_entry;
  GtkWidget *play_indicator;
  GtkWidget *loop_indicator;
  gboolean track_prog_visible;

  GtkWidget *volume_control;
  gboolean volvis;

  StatusWindow *status_window;
  StatusWindow *rip_status_window;
  StatusWindow *encode_status_window;

  GtkWidget *play_sector_label;

  GtkWidget *partial_rip_box;
  GtkWidget *rip_prog_label;
  GtkWidget *ripprogbar;
  GtkWidget *smile_indicator;
  GtkWidget *mp3_prog_label[MAX_NUM_CPU];
  GtkWidget *mp3progbar[MAX_NUM_CPU];

  GtkWidget *start_sector_entry;
  GtkWidget *end_sector_entry;

  GtkWidget *rip_exe_box;
  GtkWidget *rip_builtin_box;
  GtkWidget *ripexename_entry;
  GtkWidget *ripcmdline_entry;
  GtkWidget *mp3exename_entry;
  GtkWidget *mp3cmdline_entry;
  GtkWidget *mp3extension_entry;

  /* Overall prgress */
  GtkWidget *all_label;
  GtkWidget *all_rip_label;
  GtkWidget *all_enc_label;
  GtkWidget *all_ripprogbar;
  GtkWidget *all_encprogbar;

  /* Images */
  GtkWidget *check_image;
  GtkWidget *eject_image;
  GtkWidget *cdscan_image;
  GtkWidget *ff_image;
  GtkWidget *lowleft_image;
  GtkWidget *lowright_image;
  GtkWidget *minmax_image;
  GtkWidget *nexttrk_image;
  GtkWidget *playpaus_image;
  GtkWidget *prevtrk_image;
  GtkWidget *loop_image;
  GtkWidget *noloop_image;
  GtkWidget *random_image;
  GtkWidget *playlist_image;
  GtkWidget *playnorm_image;
  GtkWidget *quit_image;
  GtkWidget *rew_image;
  GtkWidget *stop_image;
  GtkWidget *upleft_image;
  GtkWidget *upright_image;
  GtkWidget *vol_image;
  GtkWidget *discdbwht_image;
  GtkWidget *rotate_image;
  GtkWidget *edit_image;
  GtkWidget *progtrack_image;
  GtkWidget *mail_image;
  GtkWidget *save_image;
  GtkWidget *empty_image;

  GtkWidget *discdb_pix[2];
  GtkWidget *rip_pix[4];
  GtkWidget *mp3_pix[4];
  GtkWidget *smile_pix[8];

  GtkWidget *play_pix[3];
  
  /* notification area widgets */
  EggTrayIcon *tray_icon;
  GtkTooltips *tray_tips;
  GtkWidget *tray_ebox;
  GtkWidget *tray_menu;
  GtkWidget *tray_menu_play;
  GtkWidget *tray_menu_pause;
} GripGUI;

struct _encode_track;

typedef struct _grip_info {
  char version[256];
  DiscInfo disc;
  DiscData ddata;
  gboolean use_proxy;
  gboolean use_proxy_env;
  ProxyServer proxy_server;
  DiscDBServer dbserver;
  DiscDBServer dbserver2;
  char config_filename[256];
  char cd_device[256];
  char force_scsi[256];
  char discdb_submit_email[256];
  char discdb_encoding[16];
  char id3_encoding[16];
  char id3v2_encoding[16];
  gboolean db_use_freedb;
  char user_email[256];
  gboolean local_mode;
  gboolean update_required;
  gboolean have_disc;
  gboolean tray_open;
  gboolean faulty_eject;
  gboolean looking_up;
  gboolean ask_submit;
  gboolean is_new_disc;
  gboolean first_time;
  gboolean play_first;
  gboolean play_on_insert;
  gboolean stop_first;
  gboolean no_interrupt;
  gboolean automatic_discdb;
  gboolean poll_drive;
  int poll_interval;
  int auto_eject_countdown;
  int current_discid;
  pthread_t discdb_thread;
  int volume;

  int current_disc;
  int changer_slots;

  gboolean playing;
  gboolean stopped;
  gboolean ffwding;
  gboolean rewinding;
  int play_mode;
  gboolean playloop;
  int current_track_index;
  int tracks_prog[MAX_TRACKS];
  int prog_totaltracks;
  gboolean automatic_reshuffle;

  char title_split_chars[6];

  GripGUI gui_info;

  int curr_pipe_fd;

  int num_cpu;
  gboolean ripping;
  gboolean encoding;
  gboolean stop_rip;
  gboolean stop_encode;
  gboolean ripping_a_disc;
  time_t rip_finished;
  int rippid;
  int num_wavs;
  int rip_track;
  time_t rip_started;
  int ripsize;
  char ripfile[PATH_MAX];
  int start_sector;
  int end_sector;
  gboolean doencode;
  int mp3pid[MAX_NUM_CPU];
  char mp3file[MAX_NUM_CPU][PATH_MAX];
  int mp3size[MAX_NUM_CPU];
  int mp3_started[MAX_NUM_CPU];
  int mp3_enc_track[MAX_NUM_CPU];
  char rip_delete_file[MAX_NUM_CPU][PATH_MAX];
  double track_gain_adjustment;
  double disc_gain_adjustment;
  struct _encode_track *encoded_track[MAX_NUM_CPU];
  GList *encode_list;
  GList *pending_list;
  gboolean delayed_encoding;
  int selected_ripper;
  gboolean using_builtin_cdp;
  gboolean in_rip_thread;
  gboolean do_redirect;
#ifdef CDPAR
  pthread_t cdp_thread;
  gboolean stop_thread_rip_now;
  gboolean disable_paranoia;
  gboolean disable_extra_paranoia;
  gboolean disable_scratch_detect;
  gboolean disable_scratch_repair;
  gboolean calc_gain;
  int rip_smile_level;
  gfloat rip_percent_done;
#endif
  char ripexename[256];
  char ripfileformat[256];
  char ripcmdline[256];
  int ripnice;
  int max_wavs;
  gboolean auto_rip;
  gboolean beep_after_rip;
  gboolean eject_after_rip;
  gboolean rip_partial;
  int eject_delay;
  gboolean delay_before_rip;
  gboolean stop_between_tracks;
  char wav_filter_cmd[256];
  char disc_filter_cmd[256];
  int selected_encoder;
  char mp3cmdline[256];
  char mp3extension[10];
  char mp3fileformat[256];
  char mp3exename[256];
  gboolean delete_wavs;
  gboolean add_to_db;
  gboolean add_m3u;
  gboolean rel_m3u;
  char m3ufileformat[256];
  int kbits_per_sec;
  int edit_num_cpu;
  int mp3nice;
  char mp3_filter_cmd[256];
  gboolean doid3;
  gboolean doid3v2;
  gboolean tag_mp3_only;
  char id3_comment[30];
  char cdupdate[256];
  StrTransPrefs sprefs;
  gboolean keep_min_size;
  
  /* some vars for use in TrayIconUpdate */
  gfloat rip_percent;
  gfloat enc_percent;
  
  gfloat rip_tot_percent;
  gfloat enc_tot_percent;

  gboolean app_visible;
  
  gboolean show_tray_icon;
  gboolean tray_icon_made;
  gboolean tray_menu_sensitive;

  /* these are for calculating ripping progress */
  size_t all_ripsize;
  size_t all_ripdone;
  size_t all_riplast;
  size_t all_encsize;
  size_t all_encdone;
  size_t all_enclast[MAX_NUM_CPU];
} GripInfo;

GtkWidget *GripNew(const gchar* geometry,char *device,char *scsi_device,
		   char *config_filename, gboolean force_small,
		   gboolean local_mode,gboolean no_redirect);
void GripDie(GtkWidget *widget,gpointer data);
void GripUpdate(GtkWidget *app);
GtkWidget *MakeNewPage(GtkWidget *notebook,char *name);
void LogStatus(GripInfo *ginfo,char *fmt,...);
void Busy(GripGUI *uinfo);
void UnBusy(GripGUI *uinfo);
void CloseStuff(void *user_data);

#endif /* ifndef GRIP_H */

