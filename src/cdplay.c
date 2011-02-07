/* cdplay.c
 *
 * Copyright (c) 1998-2001  Mike Oliphant <grip@nostatic.org>
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

#include "cdplay.h"
#include "grip.h"
#include "config.h"
#include "common.h"
#include "discdb.h"
#include "cddev.h"
#include "discedit.h"
#include "dialog.h"
#include "rip.h"
#include "grip_id3.h"

static void ShutDownCB(GtkWidget *widget,gpointer data);
static void DiscDBToggle(GtkWidget *widget,gpointer data);
static void DoLookup(void *data);
static void SetCurrentTrack(GripInfo *ginfo,int track);
static void ToggleChecked(GripGUI *uinfo,int track);
static void ClickColumn(GtkTreeViewColumn *column,gpointer data);
static gboolean TracklistButtonPressed(GtkWidget *widget,GdkEventButton *event,
                                       gpointer data);
static void SelectRow(GripInfo *ginfo,int track);
static void SelectionChanged(GtkTreeSelection *selection,gpointer data);
static void PlaylistChanged(GtkWindow *window,GtkWidget *widget,gpointer data);
static void ToggleLoop(GtkWidget *widget,gpointer data);
static void ChangePlayMode(GtkWidget *widget,gpointer data);
static void ChangeTimeMode(GtkWidget *widget,gpointer data);
static void ToggleProg(GtkWidget *widget,gpointer data);
static void ToggleControlButtons(GtkWidget *widget,GdkEventButton *event,
                                 gpointer data);
static void ToggleVol(GtkWidget *widget,gpointer data);
static void SetVolume(GtkWidget *widget,gpointer data);
static void FastFwdCB(GtkWidget *widget,gpointer data);
static void RewindCB(GtkWidget *widget,gpointer data);
static void NextDisc(GtkWidget *widget,gpointer data);
static void PlayTrack(GripInfo *ginfo,int track);
static void PrevTrack(GripInfo *ginfo);
static void InitProgram(GripInfo *ginfo);
static void ShuffleTracks(GripInfo *ginfo);
static gboolean CheckTracks(DiscInfo *disc);

static void ShutDownCB(GtkWidget *widget,gpointer data)
{
    GripInfo *ginfo;

    ginfo=(GripInfo *)data;

    GripDie(ginfo->gui_info.app,NULL);
}

static void DiscDBToggle(GtkWidget *widget,gpointer data)
{
    GripInfo *ginfo;
    GtkWidget *dialog;

    ginfo=(GripInfo *)data;
  
    if(ginfo->looking_up) {
        return;
    }
    else {
        if(ginfo->ripping_a_disc) {
            dialog = gtk_message_dialog_new(GTK_WINDOW(ginfo->gui_info.app),
                                            GTK_DIALOG_DESTROY_WITH_PARENT,
                                            GTK_MESSAGE_ERROR, 
                                            GTK_BUTTONS_OK, 
                                            _("Cannot do lookup while ripping."));
            gtk_dialog_run(GTK_DIALOG(dialog));
            gtk_widget_destroy(dialog);
            return;
        }
 
        if(ginfo->have_disc)
            LookupDisc(ginfo,TRUE);
    }
}

void LookupDisc(GripInfo *ginfo,gboolean manual)
{
    int track;
    gboolean present;
    DiscInfo *disc;
    DiscData *ddata;

    disc=&(ginfo->disc);
    ddata=&(ginfo->ddata);

    ddata->data_multi_artist=FALSE;
    ddata->data_year=0;

    present=DiscDBStatDiscData(disc);

    if(!manual&&present) {
        DiscDBReadDiscData(disc,ddata,ginfo->discdb_encoding);
        if(ginfo->ddata.data_id3genre==-1)
            ginfo->ddata.data_id3genre=DiscDB2ID3(ginfo->ddata.data_genre);

        ginfo->update_required=TRUE;
        ginfo->is_new_disc=TRUE;
    }
    else {
        if(!manual) {
            ddata->data_id=DiscDBDiscid(disc);
            ddata->data_genre=7; /* "misc" */
            strcpy(ddata->data_title,_("Unknown Disc"));
            strcpy(ddata->data_artist,"");
      
            for(track=0;track<disc->num_tracks;track++) {
                sprintf(ddata->data_track[track].track_name,_("Track %02d"),track+1);
                *(ddata->data_track[track].track_artist)='\0';
                *(ddata->data_track[track].track_extended)='\0';
                *(ddata->data_playlist)='\0';
            }

            *ddata->data_extended='\0';
      
            ginfo->update_required=TRUE;
        }

        if(!ginfo->local_mode && (manual?TRUE:ginfo->automatic_discdb)) {
            ginfo->looking_up=TRUE;
      
            pthread_create(&(ginfo->discdb_thread),NULL,(void *)&DoLookup,
                           (void *)ginfo);
            pthread_detach(ginfo->discdb_thread);
        }
    }
}

static void DoLookup(void *data)
{
    GripInfo *ginfo;
    gboolean discdb_found=FALSE;

    ginfo=(GripInfo *)data;

    if(!DiscDBLookupDisc(ginfo,&(ginfo->dbserver))) {
        if(*(ginfo->dbserver2.name)) {
            if(DiscDBLookupDisc(ginfo,&(ginfo->dbserver2))) {
                discdb_found=TRUE;
                ginfo->ask_submit=TRUE;
            }
        }
    }
    else {
        discdb_found=TRUE;
    }

    if(ginfo->ddata.data_id3genre==-1)
        ginfo->ddata.data_id3genre=DiscDB2ID3(ginfo->ddata.data_genre);

    ginfo->looking_up=FALSE;
    pthread_exit(0);
}

gboolean DiscDBLookupDisc(GripInfo *ginfo,DiscDBServer *server)
{
    DiscDBHello hello;
    DiscDBQuery query;
    DiscDBEntry entry;
    gboolean success=FALSE;
    DiscInfo *disc;
    DiscData *ddata;

    disc=&(ginfo->disc);
    ddata=&(ginfo->ddata);

    if(server->use_proxy)
        LogStatus(ginfo,_("Querying %s (through %s) for disc %02x.\n"),
                  server->name,
                  server->proxy->name,
                  DiscDBDiscid(disc));
    else
        LogStatus(ginfo,_("Querying %s for disc %02x.\n"),server->name,
                  DiscDBDiscid(disc));

    strncpy(hello.hello_program,"Grip",256);
    strncpy(hello.hello_version,VERSION,256);

    if(ginfo->db_use_freedb && !strcasecmp(ginfo->discdb_encoding,"UTF-8"))
        hello.proto_version=6;
    else
        hello.proto_version=5;
	
    if(!DiscDBDoQuery(disc,server,&hello,&query)) {
        ginfo->update_required=TRUE;
    } else {
        switch(query.query_match) {
        case MATCH_INEXACT:
        case MATCH_EXACT:
            LogStatus(ginfo,_("Match for \"%s / %s\"\nDownloading data...\n"),
                      query.query_list[0].list_artist,
                      query.query_list[0].list_artist);

            entry.entry_genre = query.query_list[0].list_genre;
            entry.entry_id = query.query_list[0].list_id;
            DiscDBRead(disc,server,&hello,&entry,ddata,ginfo->discdb_encoding);

            Debug(_("Done\n"));
            success=TRUE;
		
            if(DiscDBWriteDiscData(disc,ddata,NULL,TRUE,FALSE,"utf-8")<0)
                g_print(_("Error saving disc data\n"));

            ginfo->update_required=TRUE;
            ginfo->is_new_disc=TRUE;
            break;
        case MATCH_NOMATCH:
            LogStatus(ginfo,_("No match\n"));
            break;
        }
    }

    return success;
}

int GetLengthRipWidth(GripInfo *ginfo)
{
    GtkWidget *track_list;
    int width,tot_width=0;
    PangoLayout *layout;

    track_list=ginfo->gui_info.track_list;

    if(track_list) {
        layout=gtk_widget_create_pango_layout(GTK_WIDGET(track_list),
                                              _("Length"));

        pango_layout_get_size(layout,&width,NULL);
    
        g_object_unref(layout);
    
        tot_width+=width;
    
        layout=gtk_widget_create_pango_layout(GTK_WIDGET(track_list),
                                              _("Rip"));

        pango_layout_get_size(layout,&width,NULL);
    
        g_object_unref(layout);

        tot_width+=width;

        tot_width/=PANGO_SCALE;

        tot_width+=25;
    }

    return tot_width;
}

void ResizeTrackList(GripInfo *ginfo)
{
    GtkWidget *track_list;
    GtkTreeViewColumn *column;
    int tot_width=0;

    track_list=ginfo->gui_info.track_list;

    if(track_list) {
        tot_width=GetLengthRipWidth(ginfo);

        column=gtk_tree_view_get_column(GTK_TREE_VIEW(track_list),
                                        TRACKLIST_TRACK_COL);
        gtk_tree_view_column_set_fixed_width(column,track_list->
                                             allocation.width-tot_width);
    }
}

void MakeTrackPage(GripInfo *ginfo)
{
    GtkWidget *trackpage;
    GtkWidget *vbox;
    GripGUI *uinfo;
    GtkRequisition sizereq;
    GtkWidget *scroll;
    GtkTreeViewColumn *column;
    GtkCellRenderer *renderer;
    GtkTreeSelection *select;

    uinfo=&(ginfo->gui_info);

    trackpage=MakeNewPage(uinfo->notebook,_("Tracks"));

    vbox=gtk_vbox_new(FALSE,0);
    gtk_container_border_width(GTK_CONTAINER(vbox),3);

    uinfo->disc_name_label=gtk_label_new("");
    gtk_box_pack_start(GTK_BOX(vbox),uinfo->disc_name_label,FALSE,FALSE,0);
    gtk_widget_show(uinfo->disc_name_label);

    uinfo->disc_artist_label=gtk_label_new("");
    gtk_box_pack_start(GTK_BOX(vbox),uinfo->disc_artist_label,FALSE,FALSE,0);
    gtk_widget_show(uinfo->disc_artist_label);

    uinfo->track_list_store=gtk_list_store_new(TRACKLIST_N_COLUMNS,
                                               G_TYPE_STRING,
                                               G_TYPE_STRING,
                                               G_TYPE_BOOLEAN,
                                               G_TYPE_INT);

    uinfo->track_list=
        gtk_tree_view_new_with_model(GTK_TREE_MODEL(uinfo->track_list_store));

    renderer=gtk_cell_renderer_text_new();

    column=gtk_tree_view_column_new_with_attributes(_("Track"),renderer,
                                                    "text",TRACKLIST_TRACK_COL,
                                                    NULL);

    gtk_tree_view_column_set_sizing(column,GTK_TREE_VIEW_COLUMN_FIXED);
    gtk_tree_view_column_set_fixed_width(column,
                                         uinfo->win_width-
                                         (GetLengthRipWidth(ginfo)+15));

    gtk_tree_view_append_column(GTK_TREE_VIEW(uinfo->track_list),column);

    column=gtk_tree_view_column_new_with_attributes(_("Length"),renderer,
                                                    "text",TRACKLIST_LENGTH_COL,
                                                    NULL);

    gtk_tree_view_column_set_alignment(column,0.5);

    gtk_tree_view_append_column(GTK_TREE_VIEW(uinfo->track_list),column);


    renderer=gtk_cell_renderer_toggle_new();

    column=gtk_tree_view_column_new_with_attributes(_("Rip"),renderer,
                                                    "active",
                                                    TRACKLIST_RIP_COL,
                                                    NULL);

    gtk_tree_view_column_set_alignment(column,0.5);
    gtk_tree_view_column_set_fixed_width(column,20);
    gtk_tree_view_column_set_sizing(column,GTK_TREE_VIEW_COLUMN_FIXED);
    gtk_tree_view_column_set_max_width(column,20);
    gtk_tree_view_column_set_clickable(column,TRUE);

    g_signal_connect(G_OBJECT(column),"clicked",
                     G_CALLBACK(ClickColumn),(gpointer)ginfo);

    gtk_tree_view_append_column(GTK_TREE_VIEW(uinfo->track_list),column);

    select=gtk_tree_view_get_selection(GTK_TREE_VIEW(uinfo->track_list));

    gtk_tree_selection_set_mode(select,GTK_SELECTION_SINGLE);

    g_signal_connect(G_OBJECT(select),"changed",
                     G_CALLBACK(SelectionChanged),(gpointer)ginfo);

  
    g_signal_connect(G_OBJECT(uinfo->track_list),"button_press_event",
                     G_CALLBACK(TracklistButtonPressed),(gpointer)ginfo);



    /*  g_signal_connect(G_OBJECT(uinfo->track_list),"cursor_changed",
        G_CALLBACK(SelectRow),
        (gpointer)ginfo);

        g_signal_connect(G_OBJECT(uinfo->track_list),"unselect_row",
        G_CALLBACK(UnSelectRow),
        (gpointer)uinfo);
  
        g_signal_connect(G_OBJECT(uinfo->track_list),"button_press_event",
        G_CALLBACK(CListButtonPressed),(gpointer)uinfo);
  
        g_signal_connect(G_OBJECT(uinfo->track_list),"click_column",
        G_CALLBACK(ClickColumn),(gpointer)ginfo);*/


    scroll=gtk_scrolled_window_new(NULL,NULL);
    gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(scroll),
                                   GTK_POLICY_NEVER,GTK_POLICY_AUTOMATIC);
    gtk_container_add(GTK_CONTAINER(scroll),uinfo->track_list);
    gtk_box_pack_start(GTK_BOX(vbox),scroll,TRUE,TRUE,0);

    gtk_widget_show(scroll);

    gtk_widget_show(uinfo->track_list);

    gtk_widget_size_request(uinfo->track_list,&sizereq);
    //  gtk_widget_set_usize(trackpage,sizereq.width+30,-1);
    gtk_widget_set_usize(trackpage,500,-1);

    gtk_container_add(GTK_CONTAINER(trackpage),vbox);
    gtk_widget_show(vbox);
}

void SetCurrentTrackIndex(GripInfo *ginfo,int track)
{
    /* Looks up the track of index track in the program */
    for(ginfo->current_track_index = 0;
        (ginfo->current_track_index < MAX_TRACKS)
            && (ginfo->current_track_index < ginfo->prog_totaltracks)
            && (CURRENT_TRACK != track);
        ginfo->current_track_index++)
        continue;
}

static void SetCurrentTrack(GripInfo *ginfo,int track)
{
    char buf[256];
    int tracklen;

    GripGUI *uinfo;

    uinfo=&(ginfo->gui_info);

    if(track<0) {
        gtk_label_set(GTK_LABEL(uinfo->current_track_label),"--");
        gtk_entry_set_text(GTK_ENTRY(uinfo->start_sector_entry),"0");
        gtk_entry_set_text(GTK_ENTRY(uinfo->end_sector_entry),"0");

        g_signal_handlers_block_by_func(G_OBJECT(uinfo->track_edit_entry),
                                        TrackEditChanged,(gpointer)ginfo);
        gtk_entry_set_text(GTK_ENTRY(uinfo->track_edit_entry),"");
    
        g_signal_handlers_unblock_by_func(G_OBJECT(uinfo->track_edit_entry),
                                          TrackEditChanged,(gpointer)ginfo);
    
        g_signal_handlers_block_by_func(G_OBJECT(uinfo->
                                                 track_artist_edit_entry),
                                        TrackEditChanged,(gpointer)ginfo);
    
        gtk_entry_set_text(GTK_ENTRY(uinfo->track_artist_edit_entry),"");
    
        g_signal_handlers_unblock_by_func(G_OBJECT(uinfo->
                                                   track_artist_edit_entry),
                                          TrackEditChanged,(gpointer)ginfo);
    }
    else {
        g_signal_handlers_block_by_func(G_OBJECT(uinfo->track_edit_entry),
                                        TrackEditChanged,(gpointer)ginfo);
        gtk_entry_set_text(GTK_ENTRY(uinfo->track_edit_entry),
                           ginfo->ddata.data_track[track].track_name);

        g_signal_handlers_unblock_by_func(G_OBJECT(uinfo->track_edit_entry),
                                          TrackEditChanged,(gpointer)ginfo);

        g_signal_handlers_block_by_func(G_OBJECT(uinfo->
                                                 track_artist_edit_entry),
                                        TrackEditChanged,(gpointer)ginfo);

        gtk_entry_set_text(GTK_ENTRY(uinfo->track_artist_edit_entry),
                           ginfo->ddata.data_track[track].track_artist);

        g_signal_handlers_unblock_by_func(G_OBJECT(uinfo->
                                                   track_artist_edit_entry),
                                          TrackEditChanged,(gpointer)ginfo);
        g_snprintf(buf,80,"%02d",track+1);
        gtk_label_set(GTK_LABEL(uinfo->current_track_label),buf);
	
        gtk_entry_set_text(GTK_ENTRY(uinfo->start_sector_entry),"0");
	
        tracklen=(ginfo->disc.track[track+1].start_frame-1)-
            ginfo->disc.track[track].start_frame;
        g_snprintf(buf,80,"%d",tracklen);
        gtk_entry_set_text(GTK_ENTRY(uinfo->end_sector_entry),buf);

        SetCurrentTrackIndex(ginfo,track);
    }
}

gboolean TrackIsChecked(GripGUI *uinfo,int track)
{
    GtkTreePath *path;
    GtkTreeIter iter;
    gboolean checked;

    path=gtk_tree_path_new_from_indices(track,-1);

    gtk_tree_model_get_iter(GTK_TREE_MODEL(uinfo->track_list_store),&iter,path);

    gtk_tree_model_get(GTK_TREE_MODEL(uinfo->track_list_store),
                       &iter,TRACKLIST_RIP_COL,&checked,-1);

    return checked;
}

static void ToggleChecked(GripGUI *uinfo,int track)
{
    SetChecked(uinfo,track,!TrackIsChecked(uinfo,track));
}

void SetChecked(GripGUI *uinfo,int track,gboolean checked)
{
    GtkTreePath *path;
    GtkTreeIter iter;

    path=gtk_tree_path_new_from_indices(track,-1);

    gtk_tree_model_get_iter(GTK_TREE_MODEL(uinfo->track_list_store),&iter,path);

    gtk_list_store_set(uinfo->track_list_store,&iter,
                       TRACKLIST_RIP_COL,checked,-1);

    gtk_tree_path_free(path);
}

static void ClickColumn(GtkTreeViewColumn *column,gpointer data)
{
    int track;
    int numsel=0;
    gboolean check;
    GripInfo *ginfo;

    ginfo=(GripInfo *)data;

    if(ginfo->have_disc) {
        for(track=0;track<ginfo->disc.num_tracks;track++)
            if(TrackIsChecked(&(ginfo->gui_info),track)) numsel++;
    
        if(ginfo->disc.num_tracks>1) {
            check=(numsel<ginfo->disc.num_tracks/2);
        }
        else {
            check=(numsel==0);
        }
    
        for(track=0;track<ginfo->disc.num_tracks;track++)
            SetChecked(&(ginfo->gui_info),track,check);
    }
}

static gboolean TracklistButtonPressed(GtkWidget *widget,GdkEventButton *event,
                                       gpointer data)
{
    GripInfo *ginfo;
    GripGUI *uinfo;
    GtkTreeViewColumn *column;
    GtkTreePath *path;
    int *indices;
    GList *cols;
    int row,col;

    ginfo=(GripInfo *)data;
    uinfo=&(ginfo->gui_info);

    if(gtk_tree_view_get_path_at_pos(GTK_TREE_VIEW(uinfo->track_list),
                                     event->x,event->y,
                                     &path,&column,NULL,NULL)) {
        indices=gtk_tree_path_get_indices(path);

        row=indices[0];

        cols=gtk_tree_view_get_columns(GTK_TREE_VIEW(column->tree_view));

        col=g_list_index(cols,(gpointer)column);

        g_list_free(cols);

        if(event->type==GDK_BUTTON_PRESS) {
            if((event->button>1) || (col==2)) {
                ToggleChecked(uinfo,row);
            }
        }
    }

    return FALSE;
}

static void SelectRow(GripInfo *ginfo,int track)
{
    GtkTreePath *path;
    GtkTreeSelection *select;
  
    path=gtk_tree_path_new_from_indices(track,-1);
  
    select=
        gtk_tree_view_get_selection(GTK_TREE_VIEW(ginfo->gui_info.track_list));
  
    gtk_tree_selection_select_path(select,path);
  
    gtk_tree_path_free(path);
}

static void SelectionChanged(GtkTreeSelection *selection,gpointer data)
{
    GtkTreeIter iter;
    GtkTreeModel *model;
    int row=-1;
    GripInfo *ginfo;
    GripGUI *uinfo;
 
    ginfo=(GripInfo *)data;
    uinfo=&(ginfo->gui_info);
  
    if(gtk_tree_selection_get_selected(selection,&model,&iter)) {
        gtk_tree_model_get(model,&iter,TRACKLIST_NUM_COL,&row,-1);
    }

    if(row!=-1)
        SetCurrentTrack(ginfo,row);
  
    if((ginfo->disc.disc_mode==CDAUDIO_PLAYING)&&
       (ginfo->disc.curr_track!=(row+1)))
        PlayTrack(ginfo,row);
}

static void PlaylistChanged(GtkWindow *window,GtkWidget *widget,gpointer data)
{
    GripInfo *ginfo;
    GtkWidget *dialog;

    ginfo=(GripInfo *)data;

    strcpy(ginfo->ddata.data_playlist,
           gtk_entry_get_text(GTK_ENTRY(ginfo->gui_info.playlist_entry)));

    InitProgram(ginfo);

    if(DiscDBWriteDiscData(&(ginfo->disc),&(ginfo->ddata),NULL,TRUE,FALSE,
                           "utf-8")<0){
        dialog = gtk_message_dialog_new(GTK_WINDOW(ginfo->gui_info.app),
                                        GTK_DIALOG_DESTROY_WITH_PARENT,
                                        GTK_MESSAGE_ERROR,
                                        GTK_BUTTONS_OK, 
                                        _("Error saving disc data."));
        gtk_dialog_run(GTK_DIALOG(dialog));
        gtk_widget_destroy(dialog);
    }
}

static void ToggleLoop(GtkWidget *widget,gpointer data)
{
    GripInfo *ginfo;

    ginfo=(GripInfo *)data;

    ginfo->playloop=!ginfo->playloop;

    if(ginfo->playloop) 
        CopyPixmap(GTK_PIXMAP(ginfo->gui_info.loop_image),\
                   GTK_PIXMAP(ginfo->gui_info.loop_indicator));
    else
        CopyPixmap(GTK_PIXMAP(ginfo->gui_info.noloop_image),
                   GTK_PIXMAP(ginfo->gui_info.loop_indicator));

}

static void ChangePlayMode(GtkWidget *widget,gpointer data)
{
    GripInfo *ginfo;

    ginfo=(GripInfo *)data;

    ginfo->play_mode=(ginfo->play_mode+1)%PM_LASTMODE;

    CopyPixmap(GTK_PIXMAP(ginfo->gui_info.play_pix[ginfo->play_mode]),
               GTK_PIXMAP(ginfo->gui_info.play_indicator));

    gtk_widget_set_sensitive(GTK_WIDGET(ginfo->gui_info.playlist_entry),
                             ginfo->play_mode==PM_PLAYLIST);

    InitProgram(ginfo);
}

GtkWidget *MakePlayOpts(GripInfo *ginfo)
{
    GripGUI *uinfo;
    GtkWidget *ebox;
    GtkWidget *hbox;
    GtkWidget *button;

    uinfo=&(ginfo->gui_info);

    ebox=gtk_event_box_new();
    gtk_widget_set_style(ebox,uinfo->style_wb);

    hbox=gtk_hbox_new(FALSE,2);

    uinfo->playlist_entry=gtk_entry_new_with_max_length(256);
    g_signal_connect(G_OBJECT(uinfo->playlist_entry),"focus_out_event",
                     G_CALLBACK(PlaylistChanged),(gpointer)ginfo);
    gtk_widget_set_sensitive(GTK_WIDGET(uinfo->playlist_entry),
                             ginfo->play_mode==PM_PLAYLIST);
    gtk_box_pack_start(GTK_BOX(hbox),uinfo->playlist_entry,TRUE,TRUE,0);
    gtk_widget_show(uinfo->playlist_entry);

    uinfo->play_indicator=NewBlankPixmap(uinfo->app);
    CopyPixmap(GTK_PIXMAP(uinfo->play_pix[ginfo->play_mode]),
               GTK_PIXMAP(uinfo->play_indicator));

    button=gtk_button_new();
    gtk_container_add(GTK_CONTAINER(button),uinfo->play_indicator);
    gtk_widget_show(uinfo->play_indicator);
    gtk_widget_set_style(button,uinfo->style_dark_grey);
    gtk_box_pack_start(GTK_BOX(hbox),button,TRUE,TRUE,0);
    g_signal_connect(G_OBJECT(button),"clicked",
                     G_CALLBACK(ChangePlayMode),(gpointer)ginfo);
    gtk_tooltips_set_tip(MakeToolTip(),button,
                         _("Rotate play mode"),NULL);
    gtk_widget_show(button);

    uinfo->loop_indicator=NewBlankPixmap(uinfo->app);

    if(ginfo->playloop)
        CopyPixmap(GTK_PIXMAP(uinfo->loop_image),
                   GTK_PIXMAP(uinfo->loop_indicator));
    else
        CopyPixmap(GTK_PIXMAP(uinfo->noloop_image),
                   GTK_PIXMAP(uinfo->loop_indicator));

    button=gtk_button_new();
    gtk_container_add(GTK_CONTAINER(button),uinfo->loop_indicator);
    gtk_widget_show(uinfo->loop_indicator);
    gtk_widget_set_style(button,uinfo->style_dark_grey);
    gtk_box_pack_start(GTK_BOX(hbox),button,TRUE,TRUE,0);
    g_signal_connect(G_OBJECT(button),"clicked",
                     G_CALLBACK(ToggleLoop),(gpointer)ginfo);
    gtk_tooltips_set_tip(MakeToolTip(),button,
                         _("Toggle loop play"),NULL);
    gtk_widget_show(button);

    gtk_container_add(GTK_CONTAINER(ebox),hbox);
    gtk_widget_show(hbox);

    return ebox;
}

GtkWidget *MakeControls(GripInfo *ginfo)
{
    GripGUI *uinfo;
    GtkWidget *vbox,*vbox3,*hbox,*imagebox,*hbox2;
    GtkWidget *indicator_box;
    GtkWidget *button;
    GtkWidget *ebox,*lcdbox;
    GtkObject *adj;
    int mycpu;

    uinfo=&(ginfo->gui_info);

    ebox=gtk_event_box_new();
    gtk_widget_set_style(ebox,uinfo->style_wb);

    vbox=gtk_vbox_new(FALSE,0);
    gtk_container_border_width(GTK_CONTAINER(vbox),0);

    vbox3=gtk_vbox_new(FALSE,2);
    gtk_container_border_width(GTK_CONTAINER(vbox3),2);

    lcdbox=gtk_event_box_new();
    g_signal_connect(G_OBJECT(lcdbox),"button_press_event",
                     G_CALLBACK(ToggleControlButtons),(gpointer)ginfo);
    gtk_widget_set_style(lcdbox,uinfo->style_LCD);

    hbox2=gtk_hbox_new(FALSE,0);

    imagebox=gtk_vbox_new(FALSE,0);

    gtk_box_pack_start(GTK_BOX(imagebox),uinfo->upleft_image,FALSE,FALSE,0);
    gtk_widget_show(uinfo->upleft_image);

    gtk_box_pack_end(GTK_BOX(imagebox),uinfo->lowleft_image,FALSE,FALSE,0);
    gtk_widget_show(uinfo->lowleft_image);

    gtk_box_pack_start(GTK_BOX(hbox2),imagebox,FALSE,FALSE,0);
    gtk_widget_show(imagebox);
  
    hbox=gtk_hbox_new(TRUE,0);
    gtk_container_border_width(GTK_CONTAINER(hbox),0);

    uinfo->current_track_label=gtk_label_new("--");
    gtk_box_pack_start(GTK_BOX(hbox),uinfo->current_track_label,FALSE,FALSE,0);
    gtk_widget_show(uinfo->current_track_label);

    button=gtk_button_new();
    gtk_widget_set_style(button,uinfo->style_LCD);

    gtk_button_set_relief(GTK_BUTTON(button),GTK_RELIEF_NONE);

    g_signal_connect(G_OBJECT(button),"clicked",
                     G_CALLBACK(ChangeTimeMode),(gpointer)ginfo);

    uinfo->play_time_label=gtk_label_new("--:--");
    gtk_container_add(GTK_CONTAINER(button),uinfo->play_time_label);
    gtk_widget_show(uinfo->play_time_label);

    gtk_box_pack_start(GTK_BOX(hbox),button,TRUE,TRUE,0);
    gtk_widget_show(button);

    indicator_box=gtk_hbox_new(TRUE,0);

    uinfo->rip_indicator=NewBlankPixmap(GTK_WIDGET(uinfo->app));
    gtk_box_pack_start(GTK_BOX(indicator_box),uinfo->rip_indicator,TRUE,TRUE,0);
    gtk_widget_show(uinfo->rip_indicator);

    uinfo->lcd_smile_indicator=NewBlankPixmap(GTK_WIDGET(uinfo->app));
    gtk_tooltips_set_tip(MakeToolTip(),uinfo->lcd_smile_indicator,
                         _("Rip status"),NULL);
    gtk_box_pack_start(GTK_BOX(indicator_box),uinfo->lcd_smile_indicator,
                       TRUE,TRUE,0);
    gtk_widget_show(uinfo->lcd_smile_indicator);

    for(mycpu=0;mycpu<ginfo->num_cpu;mycpu++){
        uinfo->mp3_indicator[mycpu]=NewBlankPixmap(GTK_WIDGET(uinfo->app));
        gtk_box_pack_start(GTK_BOX(indicator_box),
                           uinfo->mp3_indicator[mycpu],TRUE,TRUE,0);
        gtk_widget_show(uinfo->mp3_indicator[mycpu]);
    }
  
    uinfo->discdb_indicator=NewBlankPixmap(GTK_WIDGET(uinfo->app));
    gtk_box_pack_start(GTK_BOX(indicator_box),uinfo->discdb_indicator,
                       TRUE,TRUE,0);
    gtk_widget_show(uinfo->discdb_indicator);

    gtk_box_pack_start(GTK_BOX(hbox),indicator_box,TRUE,TRUE,0);
    gtk_widget_show(indicator_box);

    gtk_container_add(GTK_CONTAINER(hbox2),hbox);
    gtk_widget_show(hbox);

    imagebox=gtk_vbox_new(FALSE,0);

    gtk_box_pack_start(GTK_BOX(imagebox),uinfo->upright_image,FALSE,FALSE,0);
    gtk_widget_show(uinfo->upright_image);

    gtk_box_pack_end(GTK_BOX(imagebox),uinfo->lowright_image,FALSE,FALSE,0);
    gtk_widget_show(uinfo->lowright_image);

    gtk_box_pack_start(GTK_BOX(hbox2),imagebox,FALSE,FALSE,0);
    gtk_widget_show(imagebox);
  
    gtk_container_add(GTK_CONTAINER(lcdbox),hbox2);
    gtk_widget_show(hbox2);

    gtk_box_pack_start(GTK_BOX(vbox3),lcdbox,FALSE,FALSE,0);
    gtk_widget_show(lcdbox);

    gtk_box_pack_start(GTK_BOX(vbox),vbox3,FALSE,FALSE,0);
    gtk_widget_show(vbox3);

    adj=gtk_adjustment_new((gfloat)ginfo->volume,0.0,255.0,1.0,1.0,0.0);
    g_signal_connect(adj,"value_changed",
                     G_CALLBACK(SetVolume),(gpointer)ginfo);
    uinfo->volume_control=gtk_hscale_new(GTK_ADJUSTMENT(adj));

    gtk_scale_set_draw_value(GTK_SCALE(uinfo->volume_control),FALSE);
    gtk_widget_set_name(uinfo->volume_control,"darkgrey");
    gtk_box_pack_start(GTK_BOX(vbox),uinfo->volume_control,FALSE,FALSE,0);

    /*  CDGetVolume(cd_desc,&vol);
        gtk_adjustment_set_value(GTK_ADJUSTMENT(adj),(vol.vol_front.left+
        vol.vol_front.right)/2);*/

    if(uinfo->volvis) gtk_widget_show(uinfo->volume_control);

    uinfo->control_button_box=gtk_vbox_new(TRUE,0);

    hbox=gtk_hbox_new(TRUE,0);

    button=ImageButton(GTK_WIDGET(uinfo->app),uinfo->playpaus_image);
    gtk_widget_set_style(button,uinfo->style_dark_grey);
    g_signal_connect(G_OBJECT(button),"clicked",
                     G_CALLBACK(PlayTrackCB),(gpointer)ginfo);
    gtk_tooltips_set_tip(MakeToolTip(),button,
                         _("Play track / Pause play"),NULL);
    gtk_box_pack_start(GTK_BOX(hbox),button,TRUE,TRUE,0);
    gtk_widget_show(button);

    button=ImageButton(GTK_WIDGET(uinfo->app),uinfo->rew_image);
    gtk_widget_set_style(button,uinfo->style_dark_grey);
    g_signal_connect(G_OBJECT(button),"pressed",
                     G_CALLBACK(RewindCB),(gpointer)ginfo);
    g_signal_connect(G_OBJECT(button),"released",
                     G_CALLBACK(RewindCB),(gpointer)ginfo);
    gtk_tooltips_set_tip(MakeToolTip(),button,
                         _("Rewind"),NULL);
    gtk_box_pack_start(GTK_BOX(hbox),button,TRUE,TRUE,0);
    gtk_widget_show(button);

    button=ImageButton(GTK_WIDGET(uinfo->app),uinfo->ff_image);
    gtk_widget_set_style(button,uinfo->style_dark_grey);
    g_signal_connect(G_OBJECT(button),"pressed",
                     G_CALLBACK(FastFwdCB),(gpointer)ginfo);
    g_signal_connect(G_OBJECT(button),"released",
                     G_CALLBACK(FastFwdCB),(gpointer)ginfo);
    gtk_tooltips_set_tip(MakeToolTip(),button,
                         _("FastForward"),NULL);
    gtk_box_pack_start(GTK_BOX(hbox),button,TRUE,TRUE,0);
    gtk_widget_show(button);

    button=ImageButton(GTK_WIDGET(uinfo->app),uinfo->prevtrk_image);
    gtk_widget_set_style(button,uinfo->style_dark_grey);
    gtk_box_pack_start(GTK_BOX(hbox),button,TRUE,TRUE,0);
    g_signal_connect(G_OBJECT(button),"clicked",
                     G_CALLBACK(PrevTrackCB),(gpointer)ginfo);
    gtk_tooltips_set_tip(MakeToolTip(),button,
                         _("Go to previous track"),NULL);
    gtk_widget_show(button);

    button=ImageButton(GTK_WIDGET(uinfo->app),uinfo->nexttrk_image);
    gtk_widget_set_style(button,uinfo->style_dark_grey);
    gtk_box_pack_start(GTK_BOX(hbox),button,TRUE,TRUE,0);
    g_signal_connect(G_OBJECT(button),"clicked",
                     G_CALLBACK(NextTrackCB),(gpointer)ginfo);
    gtk_tooltips_set_tip(MakeToolTip(),button,
                         _("Go to next track"),NULL);
    gtk_widget_show(button);

    button=ImageButton(GTK_WIDGET(uinfo->app),uinfo->progtrack_image);
    gtk_widget_set_style(button,uinfo->style_dark_grey);
    gtk_box_pack_start(GTK_BOX(hbox),button,TRUE,TRUE,0);
    g_signal_connect(G_OBJECT(button),"clicked",
                     G_CALLBACK(ToggleProg),(gpointer)ginfo);
    gtk_tooltips_set_tip(MakeToolTip(),button,
                         _("Toggle play mode options"),NULL);
    gtk_widget_show(button);

    if(ginfo->changer_slots>1) {
        button=ImageButton(GTK_WIDGET(uinfo->app),uinfo->rotate_image);
        gtk_widget_set_style(button,uinfo->style_dark_grey);
        gtk_box_pack_start(GTK_BOX(hbox),button,TRUE,TRUE,0);
        g_signal_connect(G_OBJECT(button),"clicked",
                         G_CALLBACK(NextDisc),(gpointer)ginfo);
        gtk_tooltips_set_tip(MakeToolTip(),button,
                             _("Next Disc"),NULL);
        gtk_widget_show(button);
    }

    gtk_box_pack_start(GTK_BOX(uinfo->control_button_box),hbox,TRUE,TRUE,0);
    gtk_widget_show(hbox);

    hbox=gtk_hbox_new(TRUE,0);

    button=ImageButton(GTK_WIDGET(uinfo->app),uinfo->stop_image);
    gtk_widget_set_style(button,uinfo->style_dark_grey);
    g_signal_connect(G_OBJECT(button),"clicked",
                     G_CALLBACK(StopPlayCB),(gpointer)ginfo);
    gtk_tooltips_set_tip(MakeToolTip(),button,
                         _("Stop play"),NULL);
    gtk_box_pack_start(GTK_BOX(hbox),button,TRUE,TRUE,0);
    gtk_widget_show(button);

    button=ImageButton(GTK_WIDGET(uinfo->app),uinfo->eject_image);
    gtk_widget_set_style(button,uinfo->style_dark_grey);
    gtk_box_pack_start(GTK_BOX(hbox),button,TRUE,TRUE,0);
    g_signal_connect(G_OBJECT(button),"clicked",
                     G_CALLBACK(EjectDisc),(gpointer)ginfo);
    gtk_tooltips_set_tip(MakeToolTip(),button,
                         _("Eject disc"),NULL);
    gtk_widget_show(button);

    button=ImageButton(GTK_WIDGET(uinfo->app),uinfo->cdscan_image);
    gtk_widget_set_style(button,uinfo->style_dark_grey);
    gtk_box_pack_start(GTK_BOX(hbox),button,TRUE,TRUE,0);
    g_signal_connect(G_OBJECT(button),"clicked",
                     G_CALLBACK(ScanDisc),(gpointer)ginfo);
    gtk_tooltips_set_tip(MakeToolTip(),button,
                         _("Scan Disc Contents"),NULL);
    gtk_widget_show(button);

    button=ImageButton(GTK_WIDGET(uinfo->app),uinfo->vol_image);
    gtk_widget_set_style(button,uinfo->style_dark_grey);
    g_signal_connect(G_OBJECT(button),"clicked",
                     G_CALLBACK(ToggleVol),(gpointer)ginfo);
    gtk_tooltips_set_tip(MakeToolTip(),button,
                         _("Toggle Volume Control"),NULL);
    gtk_box_pack_start(GTK_BOX(hbox),button,TRUE,TRUE,0);
    gtk_widget_show(button);

    button=ImageButton(GTK_WIDGET(uinfo->app),uinfo->edit_image);
    gtk_widget_set_style(button,uinfo->style_dark_grey);
    g_signal_connect(G_OBJECT(button),"clicked",
                     G_CALLBACK(ToggleTrackEdit),(gpointer)ginfo);
    gtk_tooltips_set_tip(MakeToolTip(),button,
                         _("Toggle disc editor"),NULL);
    gtk_box_pack_start(GTK_BOX(hbox),button,TRUE,TRUE,0);
    gtk_widget_show(button);

    if(!ginfo->local_mode) {
        button=ImageButton(GTK_WIDGET(uinfo->app),uinfo->discdbwht_image);
        gtk_widget_set_style(button,uinfo->style_dark_grey);
        g_signal_connect(G_OBJECT(button),"clicked",
                         G_CALLBACK(DiscDBToggle),(gpointer)ginfo);
        gtk_tooltips_set_tip(MakeToolTip(),button,
                             _("Initiate/abort DiscDB lookup"),NULL);
        gtk_box_pack_start(GTK_BOX(hbox),button,TRUE,TRUE,0);
        gtk_widget_show(button);
    }

    button=ImageButton(GTK_WIDGET(uinfo->app),uinfo->minmax_image);
    gtk_widget_set_style(button,uinfo->style_dark_grey);
    g_signal_connect(G_OBJECT(button),"clicked",
                     G_CALLBACK(MinMax),(gpointer)ginfo);
    gtk_tooltips_set_tip(MakeToolTip(),button,
                         _("Toggle track display"),NULL);
    gtk_box_pack_start(GTK_BOX(hbox),button,TRUE,TRUE,0);
    gtk_widget_show(button);

    button=ImageButton(GTK_WIDGET(uinfo->app),uinfo->quit_image);
    gtk_widget_set_style(button,uinfo->style_dark_grey);
    gtk_tooltips_set_tip(MakeToolTip(),button,
                         _("Exit Grip"),NULL);

    gtk_box_pack_start(GTK_BOX(hbox),button,TRUE,TRUE,0);
    g_signal_connect(G_OBJECT(button),"clicked",
                     G_CALLBACK(ShutDownCB),(gpointer)ginfo);
    gtk_widget_show(button);
  
    gtk_box_pack_start(GTK_BOX(uinfo->control_button_box),hbox,TRUE,TRUE,0);
    gtk_widget_show(hbox);

    gtk_box_pack_start(GTK_BOX(vbox),uinfo->control_button_box,TRUE,TRUE,0);
    gtk_widget_show(uinfo->control_button_box);


    gtk_container_add(GTK_CONTAINER(ebox),vbox);
    gtk_widget_show(vbox);

    return ebox;
}

static void ChangeTimeMode(GtkWidget *widget,gpointer data)
{
    GripInfo *ginfo;

    ginfo=(GripInfo *)data;

    ginfo->gui_info.time_display_mode=(ginfo->gui_info.time_display_mode+1)%4;
    UpdateDisplay(ginfo);
}

void MinMax(GtkWidget *widget,gpointer data)
{
    GripInfo *ginfo;
    GripGUI *uinfo;

    ginfo=(GripInfo *)data;
    uinfo=&(ginfo->gui_info);

    if(uinfo->minimized) {
        gtk_container_border_width(GTK_CONTAINER(uinfo->winbox),3);
        gtk_box_set_child_packing(GTK_BOX(uinfo->winbox),
                                  uinfo->controls,FALSE,FALSE,0,GTK_PACK_START);
        gtk_widget_show(uinfo->notebook);

        CopyPixmap(GTK_PIXMAP(uinfo->lcd_smile_indicator),
                   GTK_PIXMAP(uinfo->smile_indicator));
        CopyPixmap(GTK_PIXMAP(uinfo->empty_image),
                   GTK_PIXMAP(uinfo->lcd_smile_indicator));

        gtk_widget_set_size_request(GTK_WIDGET(uinfo->app),
                                    WINWIDTH,WINHEIGHT);

        gtk_window_resize(GTK_WINDOW(uinfo->app),
                          uinfo->win_width,
                          uinfo->win_height);
    }
    else {
        gtk_container_border_width(GTK_CONTAINER(uinfo->winbox),0);
        gtk_box_set_child_packing(GTK_BOX(uinfo->winbox),uinfo->controls,
                                  TRUE,TRUE,0,GTK_PACK_START);

        gtk_widget_hide(uinfo->notebook);

        CopyPixmap(GTK_PIXMAP(uinfo->smile_indicator),
                   GTK_PIXMAP(uinfo->lcd_smile_indicator));

        if(uinfo->track_edit_visible) ToggleTrackEdit(NULL,(gpointer)ginfo);
        if(uinfo->volvis) ToggleVol(NULL,(gpointer)ginfo);
        if(uinfo->track_prog_visible) ToggleProg(NULL,(gpointer)ginfo);

        gtk_widget_set_size_request(GTK_WIDGET(uinfo->app),
                                    MIN_WINWIDTH,MIN_WINHEIGHT);

        gtk_window_resize(GTK_WINDOW(uinfo->app),
                          uinfo->win_width_min,
                          uinfo->win_height_min);

        UpdateGTK();
    }

    uinfo->minimized=!uinfo->minimized;
}

static void ToggleProg(GtkWidget *widget,gpointer data)
{
    GripInfo *ginfo;
    GripGUI *uinfo;

    ginfo=(GripInfo *)data;
    uinfo=&(ginfo->gui_info);

    if(uinfo->track_prog_visible) {
        gtk_widget_hide(uinfo->playopts);
    }
    else {
        gtk_widget_show(uinfo->playopts);
    }

    uinfo->track_prog_visible=!uinfo->track_prog_visible;

    if(uinfo->minimized) {
        MinMax(NULL,ginfo);
    }
}

static void ToggleControlButtons(GtkWidget *widget,GdkEventButton *event,
                                 gpointer data)
{
    GripGUI *uinfo;

    uinfo=&((GripInfo *)data)->gui_info;

    if(uinfo->control_buttons_visible) {
        gtk_widget_hide(uinfo->control_button_box);

        UpdateGTK();
    }
    else {
        gtk_widget_show(uinfo->control_button_box);
    }

    uinfo->control_buttons_visible=!uinfo->control_buttons_visible;
}

static void ToggleVol(GtkWidget *widget,gpointer data)
{
    GripInfo *ginfo;
    GripGUI *uinfo;

    ginfo=(GripInfo *)data;
    uinfo=&(ginfo->gui_info);

    if(uinfo->volvis) {
        gtk_widget_hide(uinfo->volume_control);
    }
    else {
        gtk_widget_show(uinfo->volume_control);
    }

    uinfo->volvis=!uinfo->volvis;

    if(uinfo->minimized) {
        MinMax(NULL,ginfo);
    }
}

static void SetVolume(GtkWidget *widget,gpointer data)
{
    GripInfo *ginfo;
    DiscVolume vol;

    ginfo=(GripInfo *)data;

    ginfo->volume=vol.vol_front.left=vol.vol_front.right=
        vol.vol_back.left=vol.vol_back.right=GTK_ADJUSTMENT(widget)->value;

    CDSetVolume(&(ginfo->disc),&vol);
}

static void FastFwdCB(GtkWidget *widget,gpointer data)
{
    GripInfo *ginfo;
    GtkWidget *dialog;

    ginfo=(GripInfo *)data;

    if(ginfo->ripping_a_disc) {
        dialog = gtk_message_dialog_new(GTK_WINDOW(ginfo->gui_info.app),
                                        GTK_DIALOG_DESTROY_WITH_PARENT,
                                        GTK_MESSAGE_ERROR,
                                        GTK_BUTTONS_OK,
                                        _("Cannot fast forward while ripping."));
        gtk_dialog_run(GTK_DIALOG(dialog));
        gtk_widget_destroy(dialog);
        return;
    }

    ginfo->ffwding=!ginfo->ffwding;

    if(ginfo->ffwding) FastFwd(ginfo);
}

void FastFwd(GripInfo *ginfo)
{
    DiscTime tv;

    tv.mins=0;
    tv.secs=5;

    if((ginfo->disc.disc_mode==CDAUDIO_PLAYING)||
       (ginfo->disc.disc_mode==CDAUDIO_PAUSED)) {
        CDAdvance(&(ginfo->disc),&tv);
    }
}

static void RewindCB(GtkWidget *widget,gpointer data)
{
    GripInfo *ginfo;
    GtkWidget *dialog;

    ginfo=(GripInfo *)data;

    if(ginfo->ripping_a_disc) {
        dialog = gtk_message_dialog_new(GTK_WINDOW(ginfo->gui_info.app),
                                        GTK_DIALOG_DESTROY_WITH_PARENT,
                                        GTK_MESSAGE_ERROR,
                                        GTK_BUTTONS_OK,
                                        _("Cannot rewind while ripping."));
        gtk_dialog_run(GTK_DIALOG(dialog));
        gtk_widget_destroy(dialog);
        return;
    }

    ginfo->rewinding=!ginfo->rewinding;

    if(ginfo->rewinding) Rewind(ginfo);
}

void Rewind(GripInfo *ginfo)
{
    DiscTime tv;

    tv.mins=0;
    tv.secs=-5;

    if((ginfo->disc.disc_mode==CDAUDIO_PLAYING)||
       (ginfo->disc.disc_mode==CDAUDIO_PAUSED)) {
        CDAdvance(&(ginfo->disc),&tv);
    }
}

static void NextDisc(GtkWidget *widget,gpointer data)
{
    GripInfo *ginfo;
    GtkWidget *dialog;

    ginfo=(GripInfo *)data;

    if(ginfo->ripping_a_disc) {
        dialog = gtk_message_dialog_new(GTK_WINDOW(ginfo->gui_info.app),
                                        GTK_DIALOG_DESTROY_WITH_PARENT,
                                        GTK_MESSAGE_ERROR,
                                        GTK_BUTTONS_OK,
                                        _("Cannot switch discs while ripping."));
        gtk_dialog_run(GTK_DIALOG(dialog));
        gtk_widget_destroy(dialog);
        return;
    }

    if(ginfo->changer_slots>1) {
        ginfo->current_disc=(ginfo->current_disc+1)%ginfo->changer_slots;
        CDChangerSelectDisc(&(ginfo->disc),ginfo->current_disc);
        ginfo->have_disc=FALSE;
    }
}

void EjectDisc(GtkWidget *widget,gpointer data)
{
    GripInfo *ginfo;
    GtkWidget *dialog;

    ginfo=(GripInfo *)data;

    LogStatus(ginfo,_("Eject disc\n"));

    if(ginfo->ripping_a_disc) {
        dialog = gtk_message_dialog_new(GTK_WINDOW(ginfo->gui_info.app),
                                        GTK_DIALOG_DESTROY_WITH_PARENT,
                                        GTK_MESSAGE_ERROR,
                                        GTK_BUTTONS_OK,
                                        _("Cannot eject while ripping."));
        gtk_dialog_run(GTK_DIALOG(dialog));
        gtk_widget_destroy(dialog);
        return;
    }

    if(ginfo->auto_eject_countdown) return;

    Busy(&(ginfo->gui_info));

    if(ginfo->have_disc) {
        Debug(_("Have disc -- ejecting\n"));

        CDStop(&(ginfo->disc));
        CDEject(&(ginfo->disc));
        ginfo->playing=FALSE;
        ginfo->have_disc=FALSE;
        ginfo->update_required=TRUE;
        ginfo->current_discid=0;
        ginfo->tray_open=TRUE;
    }
    else {
        if(ginfo->faulty_eject) {
            if(ginfo->tray_open) CDClose(&(ginfo->disc));
            else CDEject(&(ginfo->disc));
        }
        else {
            if(TrayOpen(&(ginfo->disc))!=0) CDClose(&(ginfo->disc));
            else CDEject(&(ginfo->disc));
        }

        ginfo->tray_open=!ginfo->tray_open;

        if(!ginfo->tray_open)
            CheckNewDisc(ginfo,FALSE);
    }

    UnBusy(&(ginfo->gui_info));
}

void StopPlayCB(GtkWidget *widget,gpointer data)
{
    GripInfo *ginfo;

    ginfo=(GripInfo *)data;

    if(ginfo->ripping_a_disc) return;

    CDStop(&(ginfo->disc));
    CDStat(&(ginfo->disc),FALSE);
    ginfo->stopped=TRUE;

    if(ginfo->stop_first)
        SelectRow(ginfo,0);
    
    TrayMenuShowPlay(ginfo);
}

void PlaySegment(GripInfo *ginfo,int track)
{
    CDPlayFrames(&(ginfo->disc),
                 ginfo->disc.track[track].start_frame+ginfo->start_sector,
                 ginfo->disc.track[track].start_frame+ginfo->end_sector);
}



void PlayTrackCB(GtkWidget *widget,gpointer data)
{
    int track;
    GripInfo *ginfo;
    DiscInfo *disc;
    GtkWidget *dialog;

    ginfo=(GripInfo *)data;
    disc=&(ginfo->disc);

    if(ginfo->ripping_a_disc) {
        dialog = gtk_message_dialog_new(GTK_WINDOW(ginfo->gui_info.app),
                                        GTK_DIALOG_DESTROY_WITH_PARENT,
                                        GTK_MESSAGE_ERROR,
                                        GTK_BUTTONS_OK,
                                        _("Cannot play while ripping."));
        gtk_dialog_run(GTK_DIALOG(dialog));
        gtk_widget_destroy(dialog);
        return;
    }

    CDStat(disc,FALSE);

    if(ginfo->play_mode!=PM_NORMAL&&!((disc->disc_mode==CDAUDIO_PLAYING)||
                                      disc->disc_mode==CDAUDIO_PAUSED)) {
        if(ginfo->play_mode==PM_SHUFFLE && ginfo->automatic_reshuffle)
            ShuffleTracks(ginfo);
        ginfo->current_track_index=0;

        SelectRow(ginfo,CURRENT_TRACK);
    }

    track=CURRENT_TRACK;

    if(track==(disc->curr_track-1)) {
        switch(disc->disc_mode) {
        case CDAUDIO_PLAYING:
            CDPause(disc);
            TrayMenuShowPlay(ginfo);
            return;
            break;
        case CDAUDIO_PAUSED:
            CDResume(disc);
            TrayMenuShowPause(ginfo);
            return;
            break;
        default:
            PlayTrack(ginfo,track);
            TrayMenuShowPause(ginfo);
            break;
        }
    }
    else {
        PlayTrack(ginfo,track);
        TrayMenuShowPause(ginfo);
    }
}

static void PlayTrack(GripInfo *ginfo,int track)
{
    Busy(&(ginfo->gui_info));
  
    if(ginfo->play_mode==PM_NORMAL)
        CDPlayTrack(&(ginfo->disc),track+1,ginfo->disc.num_tracks);
    else CDPlayTrack(&(ginfo->disc),track+1,track+1);

    UnBusy(&(ginfo->gui_info));

    ginfo->playing=TRUE;
}

void NextTrackCB(GtkWidget *widget,gpointer data)
{
    GripInfo *ginfo;

    ginfo=(GripInfo *)data;

    NextTrack(ginfo);
}

void NextTrack(GripInfo *ginfo)
{
    GtkWidget *dialog;

    if(ginfo->ripping_a_disc) {
        dialog = gtk_message_dialog_new(GTK_WINDOW(ginfo->gui_info.app),
                                        GTK_DIALOG_DESTROY_WITH_PARENT,
                                        GTK_MESSAGE_ERROR,
                                        GTK_BUTTONS_OK,
                                        _("Cannot switch tracks while ripping."));
        gtk_dialog_run(GTK_DIALOG(dialog));
        gtk_widget_destroy(dialog);
        return;
    }
  
    CDStat(&(ginfo->disc),FALSE);

    if(ginfo->current_track_index<(ginfo->prog_totaltracks-1)) {
        SelectRow(ginfo,NEXT_TRACK);
    }
    else {
        if(!ginfo->playloop) {
            ginfo->stopped=TRUE;
        }

        SelectRow(ginfo,ginfo->tracks_prog[0]);
    }
}

void PrevTrackCB(GtkWidget *widget,gpointer data)
{
    GripInfo *ginfo;

    ginfo=(GripInfo *)data;

    PrevTrack(ginfo);
}

static void PrevTrack(GripInfo *ginfo)
{
    GtkWidget *dialog;

    if(ginfo->ripping_a_disc) {
        dialog = gtk_message_dialog_new(GTK_WINDOW(ginfo->gui_info.app),
                                        GTK_DIALOG_DESTROY_WITH_PARENT,
                                        GTK_MESSAGE_ERROR,
                                        GTK_BUTTONS_OK,
                                        _("Cannot switch tracks while ripping."));
        gtk_dialog_run(GTK_DIALOG(dialog));
        gtk_widget_destroy(dialog);
        return;
    }

    CDStat(&(ginfo->disc),FALSE);

    if((ginfo->disc.disc_mode==CDAUDIO_PLAYING) &&
       ((ginfo->disc.curr_frame-
         ginfo->disc.track[ginfo->disc.curr_track-1].start_frame) > 100))
        PlayTrack(ginfo,CURRENT_TRACK);
    else {
        if(ginfo->current_track_index) {
            SelectRow(ginfo,PREV_TRACK);
        }
        else {
            if(ginfo->playloop) {
                SelectRow(ginfo,ginfo->tracks_prog[ginfo->prog_totaltracks-1]);
            }
        }
    }
}

static void InitProgram(GripInfo *ginfo)
{
    int track;
    char *tok;
    int mode;
    char *plist;
    const char *tmp;

    mode=ginfo->play_mode;

    if((mode==PM_PLAYLIST)) {
        tmp=gtk_entry_get_text(GTK_ENTRY(ginfo->gui_info.playlist_entry));

        if(!tmp || !*tmp) {
            mode=PM_NORMAL;
        }
    }

    if(mode==PM_PLAYLIST) {
        plist=
            strdup(gtk_entry_get_text(GTK_ENTRY(ginfo->gui_info.playlist_entry)));

        ginfo->prog_totaltracks=0;

        tok=strtok(plist,",");

        while(tok) {
            ginfo->tracks_prog[ginfo->prog_totaltracks++]=atoi(tok)-1;

            tok=strtok(NULL,",");
        }

        free(plist);
    }
    else {
        ginfo->prog_totaltracks=ginfo->disc.num_tracks;
    
        for(track=0;track<ginfo->prog_totaltracks;track++) {
            ginfo->tracks_prog[track]=track;
        }
    
        if(mode==PM_SHUFFLE)
            ShuffleTracks(ginfo);
    }
}

/* Shuffle the tracks around a bit */
static void ShuffleTracks(GripInfo *ginfo)
{
    int t1,t2,tmp,shuffle;

    for(shuffle=0;shuffle<(ginfo->prog_totaltracks*10);shuffle++) {
        t1=RRand(ginfo->prog_totaltracks);
        t2=RRand(ginfo->prog_totaltracks);
    
        tmp=ginfo->tracks_prog[t1];
        ginfo->tracks_prog[t1]=ginfo->tracks_prog[t2];
        ginfo->tracks_prog[t2]=tmp;
    }
}

void CheckNewDisc(GripInfo *ginfo,gboolean force)
{
    int new_id;
    DiscInfo *disc;

    disc=&(ginfo->disc);

    if(!ginfo->looking_up) {
        Debug(_("Checking for a new disc\n"));

        if(CDStat(disc,FALSE)
           && disc->disc_present
           && CDStat(disc,TRUE)) {
            Debug(_("CDStat found a disc, checking tracks\n"));
      
            if(CheckTracks(disc)) {
                Debug(_("We have a valid disc!\n"));
	
                new_id=DiscDBDiscid(disc);

                InitProgram(ginfo);

                if(ginfo->play_first)
                    if(disc->disc_mode == CDAUDIO_COMPLETED ||
                       disc->disc_mode == CDAUDIO_NOSTATUS) {
                        SelectRow(ginfo,0);

                        disc->curr_track = 1;
                    }
	
                if(new_id || force) {
                    ginfo->have_disc=TRUE;

                    if(ginfo->play_on_insert) PlayTrackCB(NULL,(gpointer)ginfo);

                    LookupDisc(ginfo,FALSE);
                }
            }
            else {
                if(ginfo->have_disc)
                    ginfo->update_required=TRUE;
	
                ginfo->have_disc=FALSE;
                Debug(_("No non-zero length tracks\n"));
            }
        }
        else {
            if(ginfo->have_disc) {
                ginfo->update_required=TRUE;
            }

            ginfo->have_disc=FALSE;
            Debug(_("CDStat said no disc\n"));
        }
    }
}

/* Check to make sure we didn't get a false alarm from the cdrom device */

static gboolean CheckTracks(DiscInfo *disc)
{
    int track;
    gboolean have_track=FALSE;

    for(track=0;track<disc->num_tracks;track++)
        if(disc->track[track].length.mins||
           disc->track[track].length.secs) have_track=TRUE;

    return have_track;
}

/* Scan the disc */
void ScanDisc(GtkWidget *widget,gpointer data)
{
    GripInfo *ginfo;

    ginfo=(GripInfo *)data;

    ginfo->update_required=TRUE;

    CheckNewDisc(ginfo,TRUE);
}

void UpdateDisplay(GripInfo *ginfo)
{
    /* Note: need another solution other than statics if we ever want to be
       reentrant */
    static int play_counter=0;
    static int discdb_counter=0;
    char buf[80]="";
    char icon_buf[80];
    static int frames;
    static int secs;
    static int mins;
    static int old_width=0;
    int totsecs;
    GripGUI *uinfo;
    DiscInfo *disc;

    uinfo=&(ginfo->gui_info);
    disc=&(ginfo->disc);
  
    if(!uinfo->minimized) {
        if(uinfo->track_edit_visible) {
            gtk_window_get_size(GTK_WINDOW(uinfo->app),&uinfo->win_width,
                                &uinfo->win_height_edit);
        }
        else
            gtk_window_get_size(GTK_WINDOW(uinfo->app),&uinfo->win_width,
                                &uinfo->win_height);
    
        if(old_width &&
           (old_width != uinfo->track_list->allocation.width)) {
            ResizeTrackList(ginfo);
        }
    
        old_width=uinfo->track_list->allocation.width;
    }
    else {
        gtk_window_get_size(GTK_WINDOW(uinfo->app),&uinfo->win_width_min,
                            &uinfo->win_height_min);
    }

    if(!ginfo->looking_up) {
        if(discdb_counter%2)
            discdb_counter++;
    }
    else
        CopyPixmap(GTK_PIXMAP(uinfo->discdb_pix[discdb_counter++%2]),
                   GTK_PIXMAP(uinfo->discdb_indicator));

    if(!ginfo->update_required) {
        if(ginfo->have_disc) {
            /* Allow disc time to spin down after ripping before checking for a new
               disc. Some drives report no disc when spinning down. */
            if(ginfo->rip_finished) {
                if((time(NULL)-ginfo->rip_finished)>5) {
                    ginfo->rip_finished=0;
                }
            }

            if(!ginfo->rip_finished) {
                CDStat(disc,FALSE);
	
                if(!disc->disc_present) {
                    ginfo->have_disc=FALSE;
                    ginfo->update_required=TRUE;
                }
            }
        }
    }

    if(!ginfo->update_required) {
        if(ginfo->have_disc) {
            if((disc->disc_mode==CDAUDIO_PLAYING)||
               (disc->disc_mode==CDAUDIO_PAUSED)) {
                if(disc->disc_mode==CDAUDIO_PAUSED) {
                    if((play_counter++%2)==0) {
                        strcpy(buf,"");
                    }
                    else {
                        g_snprintf(buf,80,"%02d:%02d",mins,secs);
                    }
                }
                else {
                    if((disc->curr_track-1)!=CURRENT_TRACK) {
                        SelectRow(ginfo,disc->curr_track-1);
                    }

                    frames=disc->curr_frame-disc->track[disc->curr_track-1].start_frame;

                    switch(uinfo->time_display_mode) {
                    case TIME_MODE_TRACK:
                        mins=disc->track_time.mins;
                        secs=disc->track_time.secs;
                        break;
                    case TIME_MODE_DISC:
                        mins=disc->disc_time.mins;
                        secs=disc->disc_time.secs;
                        break;
                    case TIME_MODE_LEFT_TRACK:
                        secs=(disc->track_time.mins*60)+disc->track_time.secs;
                        totsecs=(disc->track[CURRENT_TRACK].length.mins*60)+
                            disc->track[CURRENT_TRACK].length.secs;
	    
                        totsecs-=secs;
	    
                        mins=totsecs/60;
                        secs=totsecs%60;
                        break;
                    case TIME_MODE_LEFT_DISC:
                        secs=(disc->disc_time.mins*60)+disc->disc_time.secs;
                        totsecs=(disc->length.mins*60)+disc->length.secs;
	    
                        totsecs-=secs;
	    
                        mins=totsecs/60;
                        secs=totsecs%60;
                        break;
                    }
	  
                    g_snprintf(buf,80,_("Current sector: %6d"),frames);
                    gtk_label_set(GTK_LABEL(uinfo->play_sector_label),buf);

                    if(uinfo->time_display_mode == TIME_MODE_LEFT_TRACK ||
                       uinfo->time_display_mode == TIME_MODE_LEFT_DISC)
                        g_snprintf(buf,80,"-%02d:%02d",mins,secs);
                    else
                        g_snprintf(buf,80,"%02d:%02d",mins,secs);
                }
            }
            else {
                if(ginfo->playing&&((disc->disc_mode==CDAUDIO_COMPLETED)||
                                    ((disc->disc_mode==CDAUDIO_NOSTATUS)&&
                                     !ginfo->stopped))) {
                    NextTrack(ginfo);
                    strcpy(buf,"00:00");
                    if(!ginfo->stopped) PlayTrack(ginfo,CURRENT_TRACK);
                }
                else if(ginfo->stopped) {
                    CDStop(disc);

                    frames=secs=mins=0;
                    g_snprintf(buf,80,_("Current sector: %6d"),frames);
                    gtk_label_set(GTK_LABEL(uinfo->play_sector_label),buf);
	  
                    strcpy(buf,"00:00");
	  
                    ginfo->stopped=FALSE;
                    ginfo->playing=FALSE;
                }
                else return;
            }
      
            gtk_label_set(GTK_LABEL(uinfo->play_time_label),buf);
            g_snprintf(icon_buf,sizeof(icon_buf),"%02d %s %s",
                       disc->curr_track,buf,PACKAGE);
            gdk_window_set_icon_name(uinfo->app->window,icon_buf);
        }
    }

    if(ginfo->update_required) {
        UpdateTracks(ginfo);

        ginfo->update_required=FALSE;

        if(ginfo->have_disc) {
            g_snprintf(buf,80,"%02d:%02d",disc->length.mins,
                       disc->length.secs);
            g_snprintf(icon_buf, sizeof(icon_buf),"%02d %s %s",
                       disc->curr_track,buf,PACKAGE);
	       
            gtk_label_set(GTK_LABEL(uinfo->play_time_label),buf);
      
            if(!ginfo->looking_up) {
                CopyPixmap(GTK_PIXMAP(uinfo->empty_image),
                           GTK_PIXMAP(uinfo->discdb_indicator));

                if(ginfo->auto_rip&&ginfo->is_new_disc) {
                    ClickColumn(NULL,ginfo);
                    DoRip(NULL,ginfo);
                }

                ginfo->is_new_disc=FALSE;
            }
      
            if(!ginfo->no_interrupt)
                SelectRow(ginfo,0);
            else
                SelectRow(ginfo,disc->curr_track-1);
        }
        else {
            gtk_label_set(GTK_LABEL(uinfo->play_time_label),"--:--");
            strncpy(icon_buf,PACKAGE,sizeof(icon_buf));
      
            SetCurrentTrack(ginfo,-1);
        }

        gdk_window_set_icon_name(uinfo->app->window,icon_buf);
    }
}

void UpdateTracks(GripInfo *ginfo)
{
    int track;
    char *col_strings[3];
    gboolean multi_artist_backup;
    GripGUI *uinfo;
    DiscInfo *disc;
    DiscData *ddata;
    EncodeTrack enc_track;
    GtkTreeIter iter;
    GtkWidget *dialog;
    gint response;

    uinfo=&(ginfo->gui_info);
    disc=&(ginfo->disc);
    ddata=&(ginfo->ddata);

    if(ginfo->have_disc) {
        /* Reset to make sure we don't eject twice */
        ginfo->auto_eject_countdown=0;

        ginfo->current_discid=DiscDBDiscid(disc);

        SetTitle(ginfo,ddata->data_title);
        SetArtist(ginfo,ddata->data_artist);
        SetYear(ginfo,ddata->data_year);
        SetID3Genre(ginfo,ddata->data_id3genre);

        multi_artist_backup=ddata->data_multi_artist;

        gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(uinfo->multi_artist_button),
                                     ginfo->ddata.data_multi_artist);

        ddata->data_multi_artist=multi_artist_backup;
        UpdateMultiArtist(NULL,(gpointer)ginfo);

        if(*(ginfo->cdupdate)) {
            FillInTrackInfo(ginfo,0,&enc_track);

            TranslateAndLaunch(ginfo->cdupdate,TranslateSwitch,&enc_track,
                               FALSE,&(ginfo->sprefs),CloseStuff,(void *)ginfo);
        }
    }
    else {
        SetTitle(ginfo,_("No Disc"));
        SetArtist(ginfo,"");
        SetYear(ginfo,0);
        SetID3Genre(ginfo,17);
    }

    gtk_entry_set_text(GTK_ENTRY(uinfo->playlist_entry),
                       ddata->data_playlist);

    if(!ginfo->first_time)
        gtk_list_store_clear(uinfo->track_list_store);
    SetCurrentTrackIndex(ginfo,disc->curr_track - 1);

    if(ginfo->have_disc) {

        col_strings[0]=(char *)malloc(260);
        col_strings[1]=(char *)malloc(6);
        col_strings[2]=NULL;

        for(track=0;track<disc->num_tracks;track++) {
            if(*ddata->data_track[track].track_artist) {
                g_snprintf(col_strings[0],260,"%02d  %s (%s)",track+1,
                           ddata->data_track[track].track_name,
                           ddata->data_track[track].track_artist);
            }
            else
                g_snprintf(col_strings[0],260,"%02d  %s",track+1,
                           ddata->data_track[track].track_name);
      
            g_snprintf(col_strings[1],6,"%2d:%02d",
                       disc->track[track].length.mins,
                       disc->track[track].length.secs);

            gtk_list_store_append(uinfo->track_list_store,&iter);

            gtk_list_store_set(uinfo->track_list_store,&iter,
                               TRACKLIST_TRACK_COL,col_strings[0],
                               TRACKLIST_LENGTH_COL,col_strings[1],
                               TRACKLIST_RIP_COL,FALSE,
                               TRACKLIST_NUM_COL,track,-1);

        }

        free(col_strings[0]);
        free(col_strings[1]);

        SelectRow(ginfo,CURRENT_TRACK);
    }

    if(ginfo->ask_submit) {
        dialog = gtk_message_dialog_new(GTK_WINDOW(uinfo->app),
                                        GTK_DIALOG_DESTROY_WITH_PARENT,
                                        GTK_MESSAGE_QUESTION,
                                        GTK_BUTTONS_YES_NO,
                                        _("This disc has been found on your secondary server,\n"
                                          "but not on your primary server.\n\n"
                                          "Do you wish to submit this disc information?"));
        response = gtk_dialog_run(GTK_DIALOG(dialog));
        gtk_widget_destroy(dialog);
        if(response == GTK_RESPONSE_YES){
            SubmitEntry(ginfo);
        }
        ginfo->ask_submit=FALSE;
    }

    ginfo->first_time=0;
}

void SubmitEntry(gpointer data)
{
    GripInfo *ginfo;
    int fd;
    FILE *efp;
    char mailcmd[256];
    char filename[256];
    GtkWidget *dialog;

    ginfo=(GripInfo *)data;

    sprintf(filename,"/tmp/grip.XXXXXX");
    fd = mkstemp(filename);

    if(fd == -1) {
        dialog = gtk_message_dialog_new(GTK_WINDOW(ginfo->gui_info.app),
                                        GTK_DIALOG_DESTROY_WITH_PARENT,
                                        GTK_MESSAGE_ERROR,
                                        GTK_BUTTONS_OK,
                                        _("Unable to create temporary file."));
        gtk_dialog_run(GTK_DIALOG(dialog));
        gtk_widget_destroy(dialog);
        return;
    }

    efp=fdopen(fd,"w");

    if(!efp) {
        close(fd);
        dialog = gtk_message_dialog_new(GTK_WINDOW(ginfo->gui_info.app),
                                        GTK_DIALOG_DESTROY_WITH_PARENT,
                                        GTK_MESSAGE_ERROR,
                                        GTK_BUTTONS_OK,
                                        _("Unable to create temporary file."));
        gtk_dialog_run(GTK_DIALOG(dialog));
        gtk_widget_destroy(dialog);
    }
    else {
        fprintf(efp,"To: %s\nFrom: %s\nSubject: cddb %s %02x\n",
                ginfo->discdb_submit_email,
                ginfo->user_email,
                DiscDBGenre(ginfo->ddata.data_genre),
                ginfo->ddata.data_id);

        if(ginfo->db_use_freedb) {
            fprintf(efp,
                    "MIME-Version: 1.0\nContent-type: text/plain; charset=UTF-8\n\n");
        }

        if(DiscDBWriteDiscData(&(ginfo->disc),&(ginfo->ddata),efp,FALSE,
                               ginfo->db_use_freedb,ginfo->db_use_freedb?
                               "UTF-8":ginfo->discdb_encoding)<0) {

            dialog = gtk_message_dialog_new(GTK_WINDOW(ginfo->gui_info.app),
                                            GTK_DIALOG_DESTROY_WITH_PARENT,
                                            GTK_MESSAGE_ERROR,
                                            GTK_BUTTONS_OK,
                                            _("Unable to write disc data."));
            gtk_dialog_run(GTK_DIALOG(dialog));
            gtk_widget_destroy(dialog);
            fclose(efp);
        }
        else {
            fclose(efp);
            close(fd);

            g_snprintf(mailcmd,256,"%s < %s",MAILER,filename);

            Debug(_("Mailing entry to %s\n"),ginfo->discdb_submit_email);

            system(mailcmd);

            remove(filename);
        }
    }
}
