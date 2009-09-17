/*
 * See Licensing and Copyright notice in naev.h
 */

/**
 * @file menu.h
 *
 * @brief Handles the important game menus.
 */


#include "info.h"

#include "naev.h"

#include <string.h>

#include "menu.h"
#include "toolkit.h"
#include "dialogue.h"
#include "log.h"
#include "pilot.h"
#include "space.h"
#include "player.h"
#include "mission.h"
#include "ntime.h"
#include "map.h"
#include "land.h"
#include "equipment.h"


#define BUTTON_WIDTH    90 /**< Button width, standard across menus. */
#define BUTTON_HEIGHT   30 /**< Button height, standard across menus. */

#define menu_Open(f)    (menu_open |= (f)) /**< Marks a menu as opened. */
#define menu_Close(f)   (menu_open &= ~(f)) /**< Marks a menu as closed. */

#define INFO_WINDOWS      5 /**< Amount of windows in the tab. */

static const char *info_names[INFO_WINDOWS] = {
   "Main",
   "Ship",
   "Cargo",
   "Missions",
   "Standings"
}; /**< Name of the tab windows. */


static unsigned int info_wid = 0;
static unsigned int *info_windows = NULL;

static CstSlotWidget info_eq;
static int *info_factions;


/*
 * prototypes
 */
/* information menu */
static void info_close( unsigned int wid, char* str );
static void info_openMain( unsigned int wid );
static void info_openShip( unsigned int wid );
static void info_openCargo( unsigned int wid );
static void info_openMissions( unsigned int wid );
static void info_getDim( unsigned int wid, int *w, int *h, int *lw );
static void standings_close( unsigned int wid, char *str );
static void ship_update( unsigned int wid );
static void info_openStandings( unsigned int wid );
static void standings_update( unsigned int wid, char* str );
static void cargo_genList( unsigned int wid );
static void cargo_update( unsigned int wid, char* str );
static void cargo_jettison( unsigned int wid, char* str );
static void mission_menu_abort( unsigned int wid, char* str );
static void mission_menu_genList( unsigned int wid, int first );
static void mission_menu_update( unsigned int wid, char* str );


/**
 * @brief Opens the information menu.
 */
void menu_info (void)
{
   int w, h;

   /* Can't open menu twice. */
   if (menu_isOpen(MENU_INFO) || dialogue_isOpen())
      return;

   /* Dimensions. */
   w = 600;
   h = 500;

   /* Create the window. */
   info_wid = window_create( "Info", -1, -1, w, h );
   info_windows = window_addTabbedWindow( info_wid, -1, -1, -1, -1, "tabInfo",
         INFO_WINDOWS, info_names );

   /* Open the subwindows. */
   info_openMain( info_windows[0] );
   info_openShip( info_windows[1] );
   info_openCargo( info_windows[2] );
   info_openMissions( info_windows[3] );
   info_openStandings( info_windows[4] );

   menu_Open(MENU_INFO);
}
/**
 * @brief Closes the information menu.
 *    @param str Unused.
 */
static void info_close( unsigned int wid, char* str )
{
   (void) wid;
   if (info_wid > 0) {
      window_close( info_wid, str );
      info_wid = 0;
      menu_Close(MENU_INFO);
   }
}


/**
 * @brief Opens the main info window.
 */
static void info_openMain( unsigned int wid )
{
   char str[128], **buf, creds[16];
   char **licenses;
   int nlicenses;
   int i;
   char *nt;
   int w, h;

   /* Get the dimensions. */
   window_dimWindow( wid, &w, &h );

   /* pilot generics */
   nt = ntime_pretty( ntime_get() );
   window_addText( wid, 40, 20, 120, h-80,
         0, "txtDPilot", &gl_smallFont, &cDConsole,
         "Pilot:\n"
         "Date:\n"
         "Combat Rating:\n"
         "\n"
         "Money:\n"
         "Ship:\n"
         "Fuel:"
         );
   credits2str( creds, player->credits, 2 );
   snprintf( str, 128, 
         "%s\n"
         "%s\n"
         "%s\n"
         "\n"
         "%s Credits\n"
         "%s\n"
         "%.0f (%d Jumps)",
         player_name,
         nt,
         player_rating(),
         creds,
         player->name,
         player->fuel, pilot_getJumps(player) );
   window_addText( wid, 140, 20,
         200, h-80,
         0, "txtPilot", &gl_smallFont, &cBlack, str );
   free(nt);

   /* menu */
   window_addButton( wid, -20, 20,
         BUTTON_WIDTH, BUTTON_HEIGHT,
         "btnClose", "Close", info_close );

   /* List. */
   buf = player_getLicenses( &nlicenses );
   licenses = malloc(sizeof(char*)*nlicenses);
   for (i=0; i<nlicenses; i++)
      licenses[i] = strdup(buf[i]);
   window_addText( wid, -20, -40, w-80-200-40, 20, 1, "txtList",
         NULL, &cDConsole, "Licenses" );
   window_addList( wid, -20, -70, w-80-200-40, h-110-BUTTON_HEIGHT,
         "lstLicenses", licenses, nlicenses, 0, NULL );
}


/**
 * @brief Shows the player what outfits he has.
 *
 *    @param str Unused.
 */
static void info_openShip( unsigned int wid )
{
   int w, h;

   /* Get the dimensions. */
   window_dimWindow( wid, &w, &h );

   /* Buttons */
   window_addButton( wid, -20, 20,
         BUTTON_WIDTH, BUTTON_HEIGHT,
         "closeOutfits", "Close", info_close );

   /* Text. */
   window_addText( wid, 40, -60, 100, h-60, 0, "txtSDesc", &gl_smallFont,
         &cDConsole,
         "Name:\n"
         "Model:\n"
         "Class:\n"
         "Crew:\n"
         "\n"
         "Total CPU:\n"
         "Mass:\n"
         "Jump Time:\n"
         "Thrust:\n"
         "Speed:\n"
         "Turn:\n"
         "\n"
         "Shield:\n"
         "Armour:\n"
         "Energy:\n"
         "Cargo Space:\n"
         "Fuel:\n"
         );
   window_addText( wid, 140, -60, w-300., h-60, 0, "txtDDesc", &gl_smallFont,
         &cBlack, NULL );

   /* Custom widget. */
   equipment_slotWidget( wid, -20, -40, 180, h-60, &info_eq );
   info_eq.selected  = player;
   info_eq.canmodify = 0;

   /* Update ship. */
   ship_update( wid );
}


/**
 * @brief Updates the ship stuff.
 */
static void ship_update( unsigned int wid )
{
   char buf[1024];
   int cargo;

   cargo = pilot_cargoUsed( player ) + pilot_cargoFree( player);
   snprintf( buf, sizeof(buf),
         "%s\n"
         "%s\n"
         "%s\n"
         "%d\n"
         "\n"
         "%.0f Teraflops\n"
         "%.0f Tons\n"
         "%.1f STU Average\n"
         "%.0f KN/Ton\n"
         "%.0f M/s\n"
         "%.0f Grad/s\n"
         "\n"
         "%.0f / %.0f MJ (%.1f MW)\n" /* Shield */
         "%.0f / %.0f MJ (%.1f MW)\n" /* Armour */
         "%.0f / %.0f MJ (%.1f MW)\n" /* Energy */
         "%d / %d Tons\n"
         "%.0f / %.0f Units (%d Jumps)",
         /* Generic */
         player->name,
         player->ship->name,
         ship_class(player->ship),
         player->ship->crew,
         player->cpu_max,
         /* Movement. */
         player->solid->mass,
         pilot_hyperspaceDelay( player ),
         player->thrust / player->solid->mass,
         player->speed,
         player->turn,
         /* Health. */
         player->shield, player->shield_max, player->shield_regen,
         player->armour, player->armour_max, player->armour_regen,
         player->energy, player->energy_max, player->energy_regen,
         pilot_cargoUsed( player ), cargo,
         player->fuel, player->fuel_max, pilot_getJumps(player));
   window_modifyText( wid, "txtDDesc", buf );
}


/**
 * @brief Shows the player his cargo.
 *
 *    @param str Unused.
 */
static void info_openCargo( unsigned int wid )
{
   int w, h;

   /* Get the dimensions. */
   window_dimWindow( wid, &w, &h );

   /* Buttons */
   window_addButton( wid, -20, 20, BUTTON_WIDTH, BUTTON_HEIGHT,
         "closeCargo", "Close", info_close );
   window_addButton( wid, -40 - BUTTON_WIDTH, 20,
         BUTTON_WIDTH, BUTTON_HEIGHT, "btnJettisonCargo", "Jettison",
         cargo_jettison );
   window_disableButton( wid, "btnJettisonCargo" );

   /* Generate the list. */
   cargo_genList( wid );
}
/**
 * @brief Generates the cargo list.
 */
static void cargo_genList( unsigned int wid )
{
   char **buf;
   int nbuf;
   int i;
   int w, h;

   /* Get the dimensions. */
   window_dimWindow( wid, &w, &h );

   /* Destroy widget if needed. */
   if (widget_exists( wid, "lstCargo" ))
      window_destroyWidget( wid, "lstCargo" );

   /* List */
   if (player->ncommodities==0) {
      /* No cargo */
      buf = malloc(sizeof(char*));
      buf[0] = strdup("None");
      nbuf = 1;
   }
   else {
      /* List the player's cargo */
      buf = malloc(sizeof(char*)*player->ncommodities);
      for (i=0; i<player->ncommodities; i++) {
         buf[i] = malloc(sizeof(char)*128);
         snprintf(buf[i],128, "%s%s %d",
               player->commodities[i].commodity->name,
               (player->commodities[i].id != 0) ? "*" : "",
               player->commodities[i].quantity);
      }
      nbuf = player->ncommodities;
   }
   window_addList( wid, 20, -40,
         w - 40, h - BUTTON_HEIGHT - 80,
         "lstCargo", buf, nbuf, 0, cargo_update );

   cargo_update(wid, NULL);
}
/**
 * @brief Updates the player's cargo in the cargo menu.
 *    @param str Unused.
 */
static void cargo_update( unsigned int wid, char* str )
{
   (void)str;
   int pos;

   if (player->ncommodities==0)
      return; /* No cargo */

   pos = toolkit_getListPos( wid, "lstCargo" );

   /* Can jettison all but mission cargo when not landed*/
   if (landed)
      window_disableButton( wid, "btnJettisonCargo" );
   else
      window_enableButton( wid, "btnJettisonCargo" );
}
/**
 * @brief Makes the player jettison the currently selected cargo.
 *    @param str Unused.
 */
static void cargo_jettison( unsigned int wid, char* str )
{
   (void)str;
   int i, j, f, pos, ret;
   Mission *misn;

   if (player->ncommodities==0)
      return; /* No cargo, redundant check */

   pos = toolkit_getListPos( wid, "lstCargo" );

   /* Special case mission cargo. */
   if (player->commodities[pos].id != 0) {
      if (!dialogue_YesNo( "Abort Mission", 
               "Are you sure you want to abort this mission?" ))
         return;

      /* Get the mission. */
      f = 0;
      for (i=0; i<MISSION_MAX; i++) {
         for (j=0; j<player_missions[i].ncargo; j++) {
            if (player_missions[i].cargo[j] == player->commodities[pos].id) {
               f = 1;
               break;
            }
         }
         if (f==1)
            break;
      }
      if (!f) {
         WARN("Cargo '%d' does not belong to any active mission.",
               player->commodities[pos].id);
         return;
      }
      misn = &player_missions[i];

      /* We run the "abort" function if it's found. */
      ret = misn_tryRun( misn, "abort" );

      /* Now clean up mission. */
      if (ret != 2) {
         mission_cleanup( misn );
         memmove( misn, &player_missions[i+1], 
               sizeof(Mission) * (MISSION_MAX-i-1) );
         memset( &player_missions[MISSION_MAX-1], 0, sizeof(Mission) );
      }

      /* Reset markers. */
      mission_sysMark();

      /* Regenerate list. */
      mission_menu_genList( info_windows[3] ,0);
   }
   else {
      /* Remove the cargo */
      commodity_Jettison( player->id, player->commodities[pos].commodity,
            player->commodities[pos].quantity );
      pilot_rmCargo( player, player->commodities[pos].commodity,
            player->commodities[pos].quantity );
   }

   /* We reopen the menu to recreate the list now. */
   ship_update( info_windows[1] );
   cargo_genList( wid );
}


/**
 * @brief Gets the window standings window dimensions.
 */
static void info_getDim( unsigned int wid, int *w, int *h, int *lw )
{
   /* Get the dimensions. */
   window_dimWindow( wid, w, h );
   *lw = *w-60-BUTTON_WIDTH-120;
}


/**
 * @brief Closes the faction stuff.
 */
static void standings_close( unsigned int wid, char *str )
{
   (void) wid;
   (void) str;
   if (info_factions != NULL)
      free(info_factions);
   info_factions = NULL;
}


/**
 * @brief Displays the player's standings.
 */
static void info_openStandings( unsigned int wid )
{
   int i;
   int n, m;
   char **str;
   int w, h, lw;

   /* Get dimensions. */
   info_getDim( wid, &w, &h, &lw );

   /* On close. */
   window_onClose( wid, standings_close );

   /* Buttons */
   window_addButton( wid, -20, 20, BUTTON_WIDTH, BUTTON_HEIGHT,
         "closeMissions", "Close", info_close );

   /* Graphics. */
   window_addImage( wid, 0, 0, "imgLogo", NULL, 0 );

   /* Text. */
   window_addText( wid, lw+40, 0, (w-(lw+60)), 20, 1, "txtName",
         &gl_defFont, &cDConsole, NULL );
   window_addText( wid, lw+40, 0, (w-(lw+60)), 20, 1, "txtStanding",
         &gl_smallFont, &cBlack, NULL );

   /* Gets the faction standings. */
   info_factions  = faction_getAll( &n );
   str            = malloc( sizeof(char*) * n );

   /* Create list. */
   for (i=0; i<n; i++) {
      str[i] = malloc( 256 );
      m = round( faction_getPlayer( info_factions[i] ) );
      snprintf( str[i], 256, "%s   [ %+d%% ]",
            faction_name( info_factions[i] ), m );
   }

   /* Display list. */
   window_addList( wid, 20, -40, lw, h-60,
         "lstStandings", str, n, 0, standings_update );

   standings_update( wid , NULL );
}


/**
 * @brief Updates the standings menu.
 */
static void standings_update( unsigned int wid, char* str )
{
   (void) str;
   int p, y;
   glTexture *t;
   int w, h, lw;
   char buf[128];
   int m;

   /* Get dimensions. */
   info_getDim( wid, &w, &h, &lw );

   /* Get faction. */
   p = toolkit_getListPos( wid, "lstStandings" );
   y = 0;

   /* Render logo. */
   t = faction_logoSmall( info_factions[p] );
   if (t != NULL) {
      window_modifyImage( wid, "imgLogo", t );
      y = -40 - t->h;
      window_moveWidget( wid, "imgLogo", lw+40 + (w-(lw+60)-t->w)/2, y );
   }
   else {
      window_modifyImage( wid, "imgLogo", NULL );
      y = -20;
   }

   /* Modify text. */
   y -= 30;
   window_modifyText( wid, "txtName", faction_longname( info_factions[p] ) );
   window_moveWidget( wid, "txtName", lw+40, y );
   y -= 40;
   m = round( faction_getPlayer( info_factions[p] ) );
   snprintf( buf, sizeof(buf), "%+d%%   [ %s ]", m, faction_getStanding( m ) );
   window_modifyText( wid, "txtStanding", buf );
   window_moveWidget( wid, "txtStanding", lw+40, y );
}


/**
 * @brief Shows the player's active missions.
 *
 *    @param parent Unused.
 *    @param str Unused.
 */
static void info_openMissions( unsigned int wid )
{
   int w, h;

   /* Get the dimensions. */
   window_dimWindow( wid, &w, &h );

   /* buttons */
   window_addButton( wid, -20, 20, BUTTON_WIDTH, BUTTON_HEIGHT,
         "closeMissions", "Close", info_close );
   window_addButton( wid, -20, 40 + BUTTON_HEIGHT,
         BUTTON_WIDTH, BUTTON_HEIGHT, "btnAbortMission", "Abort",
         mission_menu_abort );
   
   /* text */
   window_addText( wid, 300+40, -60,
         200, 40, 0, "txtSReward",
         &gl_smallFont, &cDConsole, "Reward:" );
   window_addText( wid, 300+100, -60,
         140, 40, 0, "txtReward", &gl_smallFont, &cBlack, NULL );
   window_addText( wid, 300+40, -100,
         w - (300+40+40), h - BUTTON_HEIGHT - 120, 0,
         "txtDesc", &gl_smallFont, &cBlack, NULL );

   /* Put a map. */
   map_show( wid, 20, 20, 300, 260, 0.75 );

   /* list */
   mission_menu_genList(wid ,1);
}
/**
 * @brief Creates the current mission list for the mission menu.
 *    @param first 1 if it's the first time run.
 */
static void mission_menu_genList( unsigned int wid, int first )
{
   int i,j;
   char** misn_names;
   int w, h;

   if (!first)
      window_destroyWidget( wid, "lstMission" );

   /* Get the dimensions. */
   window_dimWindow( wid, &w, &h );

   /* list */
   misn_names = malloc(sizeof(char*) * MISSION_MAX);
   j = 0;
   for (i=0; i<MISSION_MAX; i++)
      if (player_missions[i].id != 0)
         misn_names[j++] = strdup(player_missions[i].title);
   if (j==0) { /* no missions */
      free(misn_names);
      misn_names = malloc(sizeof(char*));                                 
      misn_names[0] = strdup("No Missions");                              
      j = 1;
   }
   window_addList( wid, 20, -40,
         300, h-340,
         "lstMission", misn_names, j, 0, mission_menu_update );

   mission_menu_update(wid ,NULL);
}
/**
 * @brief Updates the mission menu mission information based on what's selected.
 *    @param str Unusued.
 */
static void mission_menu_update( unsigned int wid, char* str )
{
   (void)str;
   char *active_misn;
   Mission* misn;

   active_misn = toolkit_getList( wid, "lstMission" );
   if (strcmp(active_misn,"No Missions")==0) {
      window_modifyText( wid, "txtReward", "None" );
      window_modifyText( wid, "txtDesc",
            "You currently have no active missions." );
      window_disableButton( wid, "btnAbortMission" );
      return;
   }

   /* Modify the text. */
   misn = &player_missions[ toolkit_getListPos(wid, "lstMission" ) ];
   window_modifyText( wid, "txtReward", misn->reward );
   window_modifyText( wid, "txtDesc", misn->desc );
   window_enableButton( wid, "btnAbortMission" );

   /* Select the system. */
   if (misn->sys_marker != NULL)
      map_center( misn->sys_marker );
}
/**
 * @brief Aborts a mission in the mission menu.
 *    @param str Unused.
 */
static void mission_menu_abort( unsigned int wid, char* str )
{
   (void)str;
   char *selected_misn;
   int pos;
   Mission* misn;
   int ret;

   selected_misn = toolkit_getList( wid, "lstMission" );

   if (dialogue_YesNo( "Abort Mission", 
            "Are you sure you want to abort this mission?" )) {

      /* Get the mission. */
      pos = toolkit_getListPos(wid, "lstMission" );
      misn = &player_missions[pos];

      /* We run the "abort" function if it's found. */
      ret = misn_tryRun( misn, "abort" );

      /* Now clean up mission. */
      if (ret != 2) {
         mission_cleanup( misn );
         memmove( misn, &player_missions[pos+1], 
               sizeof(Mission) * (MISSION_MAX-pos-1) );
         memset( &player_missions[MISSION_MAX-1], 0, sizeof(Mission) );
      }

      /* Reset markers. */
      mission_sysMark();

      /* Regenerate list. */
      mission_menu_genList(wid ,0);
   }
}

