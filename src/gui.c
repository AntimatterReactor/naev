/*
 * See Licensing and Copyright notice in naev.h
 */

/**
 * @file gui.c
 *
 * @brief Contains the GUI stuff for the player.
 */


#include "gui.h"

#include "naev.h"

#include <stdlib.h>
#include <string.h>

#include "player.h"
#include "nxml.h"
#include "pilot.h"
#include "log.h"
#include "opengl.h"
#include "font.h"
#include "ndata.h"
#include "space.h"
#include "rng.h"
#include "land.h"
#include "sound.h"
#include "economy.h"
#include "pause.h"
#include "menu.h"
#include "toolkit.h"
#include "dialogue.h"
#include "mission.h"
#include "nlua_misn.h"
#include "ntime.h"
#include "hook.h"
#include "map.h"
#include "nfile.h"
#include "spfx.h"
#include "unidiff.h"
#include "comm.h"
#include "intro.h"
#include "perlin.h"
#include "ai.h"
#include "music.h"
#include "nmath.h"
#include "gui_osd.h"


#define XML_GUI_ID   "GUIs" /**< XML section identifier for GUI document. */
#define XML_GUI_TAG  "gui" /**<  XML Section identifier for GUI tags. */

#define GUI_DATA     "dat/gui.xml" /**< Global GUI configuration file. */
#define GUI_GFX      "gfx/gui/" /**< Location of the GUI graphics. */

#define TARGET_WIDTH 128 /**< Width of target graphics. */
#define TARGET_HEIGHT 96 /**< Height of target graphics. */

#define INTERFERENCE_LAYERS      16 /**< Number of interference layers. */
#define INTERFERENCE_CHANGE_DT   0.1 /**< Speed to change at. */

#define RADAR_BLINK_PILOT        1. /**< Blink rate of the pilot target on radar. */
#define RADAR_BLINK_PLANET       1. /**< Blink rate of the planet target on radar. */


/* for interference. */
static int interference_layer = 0; /**< Layer of the current interference. */
double interference_alpha     = 0.; /**< Alpha of the current interference layer. */
static double interference_t  = 0.; /**< Interference timer to control transitions. */

/* some blinking stuff. */
static double blink_pilot     = 0.; /**< Timer on target blinking on radar. */
static double blink_planet    = 0.; /**< Timer on planet blinking on radar. */

/* for VBO. */
static gl_vbo *gui_vbo = NULL; /**< GUI VBO. */
static GLsizei gui_vboColourOffset = 0; /**< Offset of colour pixels. */

/*
 * pilot stuff for GUI
 */
extern Pilot** pilot_stack;
extern int pilot_nstack;

/*
 * map stuff for autonav
 */
extern int map_npath;


/**
 * @struct Radar
 *
 * @brief Represents the player's radar.
 */
typedef struct Radar_ {
   double x; /**< X position. */
   double y; /**< Y Position */
   double w; /**< Width. */
   double h; /**< Height. */
   RadarShape shape; /**< Shape */
   double res; /**< Resolution */
   glTexture *interference[INTERFERENCE_LAYERS]; /**< Interference texture. */
} Radar;
/* radar resolutions */
#define RADAR_RES_MAX      100. /**< Maximum radar resolution. */
#define RADAR_RES_MIN      10. /**< Minimum radar resolution. */
#define RADAR_RES_INTERVAL 10. /**< Steps used to increase/decrease resolution. */
#define RADAR_RES_DEFAULT  40. /**< Default resolution. */

/**
 * @struct Rect
 *
 * @brief Represents a rectangle.
 */
typedef struct Rect_ {
   double x; /**< X position. */
   double y; /**< Y position. */
   double w; /**< Width. */
   double h; /**< Height. */
} Rect;


typedef struct HealthBar_ {
   Rect rect; /**< Position and dimensions. */
   glColour col; /**< Colour. */
   glTexture *gfx; /**< Graphic to use (if NULL uses a bar). */
   /* Used to do fill aproximation. */
   double area; /**< Total area of the graphic. */
   double slope; /**< Slope of the aproximation. */
   double offset; /**< Offset of the aproximation. */
} HealthBar;

/**
 * @struct GUI
 *
 * @brief Represents the ingame player graphical user interface.
 */
typedef struct GUI_ {
   /* graphics */
   glTexture *gfx_frame; /**< Frame of the GUI. */
   glTexture *gfx_targetPilot; /**< Graphics used to target pilot. */
   glTexture *gfx_targetPlanet; /**< Graphics used to target planets. */

   /*
    * Rects.
    */
   /* Radar. */
   Radar radar; /**< The radar. */
   /* Navigation. */
   Rect nav; /**< Navigation computer. */
   /* Health. */
   HealthBar shield; /**< Shield bar. */
   HealthBar armour; /**< Armour bar. */
   HealthBar energy; /**< Energy bar. */
   HealthBar fuel; /**< Fuel bar. */
   /* Weapon. */
   Rect weapon; /**< Weapon targetting system. */
   /* Targetting. */
   Rect target_health; /**< Target health. */
   Rect target_name; /**< Name of the target. */
   Rect target_faction; /**< Faction of the target. */
   /* Misc. */
   Rect misc; /**< Misc stuff: credits, cargo... */
   /* Messages. */
   Rect mesg; /**< Where messages go. */

   /* positions */
   Vector2d frame; /**< Global frame position. */
   Vector2d target; /**< Global target position. */

} GUI;
static GUI gui = { .gfx_frame = NULL,
   .gfx_targetPilot = NULL,
   .gfx_targetPlanet = NULL }; /**< Ze GUI. */
/* needed to render properly */
static double gui_xoff = 0.; /**< X Offset that GUI introduces. */
static double gui_yoff = 0.; /**< Y offset that GUI introduces. */

/* messages */
#define MESG_SIZE_MAX   120 /**< Maxmimu message length. */
double mesg_timeout = 5.; /**< How long it takes for a message to timeout. */
int mesg_max = 5; /**< Maximum messages onscreen */
/**
 * @struct Mesg
 * 
 * @brief On screen player message.
 */
typedef struct Mesg_ {
   char str[MESG_SIZE_MAX]; /**< The message. */
   double t; /**< Timer related to message. */
} Mesg;
static Mesg* mesg_stack = NULL; /**< Stack of mesages, will be of mesg_max size. */


/* 
 * prototypes
 */
/* 
 * external
 */
extern void weapon_minimap( const double res, const double w, const double h,
      const RadarShape shape, double alpha ); /**< from weapon.c */
/* 
 * internal
 */
/* gui */
static void gui_createInterference (void);
static void rect_parseParam( const xmlNodePtr parent,
      char *name, double *param );
static void rect_parse( const xmlNodePtr parent,
      double *x, double *y, double *w, double *h );
static int gui_parseBar( xmlNodePtr parent, HealthBar *bar, const glColour *col );
static int gui_parse( const xmlNodePtr parent, const char *name );
static void gui_cleanupBar( HealthBar *bar );
/* Render GUI. */
static void gui_renderTarget( double dt );
static void gui_renderBorder( double dt );
static void gui_renderRadar( double dt );
static void gui_renderMessages( double dt );
static glColour *gui_getPlanetColour( int i );
static void gui_renderPlanet( int i );
static glColour* gui_getPilotColour( const Pilot* p );
static void gui_renderPilot( const Pilot* p );
static void gui_renderHealth( const HealthBar *bar, const double w );
static void gui_renderInterference( double dt );



/**
 * Sets the GUI to defaults.
 */
void gui_setDefaults (void)
{
   gui.radar.res = RADAR_RES_DEFAULT;
   memset( mesg_stack, 0, mesg_max * sizeof(Mesg));
}


/**
 * @brief Adds a mesg to the queue to be displayed on screen.
 *
 *    @param str Message to add.
 */
void player_messageRaw ( const char *str )
{
   int i;

   /* copy old messages back */
   for (i=1; i<mesg_max; i++) {
      if (mesg_stack[mesg_max-i-1].str[0] != '\0') {
         strcpy(mesg_stack[mesg_max-i].str, mesg_stack[mesg_max-i-1].str);
         mesg_stack[mesg_max-i].t = mesg_stack[mesg_max-i-1].t;
      }
   }

   /* add the new one */
   strncpy( mesg_stack[0].str, str, MESG_SIZE_MAX );

   mesg_stack[0].t = mesg_timeout;
}

/**
 * @brief Adds a mesg to the queue to be displayed on screen.
 *
 *    @param fmt String with formatting like printf.
 */
void player_message ( const char *fmt, ... )
{
   va_list ap;
   int i;

   if (fmt == NULL) return; /* message not valid */

   /* copy old messages back */
   for (i=1; i<mesg_max; i++) {
      if (mesg_stack[mesg_max-i-1].str[0] != '\0') {
         strcpy(mesg_stack[mesg_max-i].str, mesg_stack[mesg_max-i-1].str);
         mesg_stack[mesg_max-i].t = mesg_stack[mesg_max-i-1].t;
      }
   }

   /* add the new one */
   va_start(ap, fmt);
   vsnprintf( mesg_stack[0].str, MESG_SIZE_MAX, fmt, ap );
   va_end(ap);

   mesg_stack[0].t = mesg_timeout;
}


/**
 * @brief Renders the background GUI stuff, namely planet target gfx
 *
 *    @param dt Current delta tick.
 */
void gui_renderBG( double dt )
{
   (void) dt;
   double x,y;
   glColour *c;
   Planet* planet;

   /* no need to draw if pilot is dead */
   if (player_isFlag(PLAYER_DESTROYED) || player_isFlag(PLAYER_CREATING) ||
      ((player != NULL) && pilot_isFlag(player,PILOT_DEAD)))
      return;

   /* Make sure target exists. */
   if (planet_target < 0)
      return;

   /* Make sure targets are still in range. */
   if (!pilot_inRangePlanet( player, planet_target )) {
      planet_target = -1;
      return;
   }

   /* Draw planet target graphics. */
   planet = cur_system->planets[planet_target];

   c = faction_getColour(planet->faction);

   x = planet->pos.x - planet->gfx_space->sw/2.;
   y = planet->pos.y + planet->gfx_space->sh/2.;
   gl_blitSprite( gui.gfx_targetPlanet, x, y, 0, 0, c ); /* top left */

   x += planet->gfx_space->sw;
   gl_blitSprite( gui.gfx_targetPlanet, x, y, 1, 0, c ); /* top right */

   y -= planet->gfx_space->sh;
   gl_blitSprite( gui.gfx_targetPlanet, x, y, 1, 1, c ); /* bottom right */

   x -= planet->gfx_space->sw;
   gl_blitSprite( gui.gfx_targetPlanet, x, y, 0, 1, c ); /* bottom left */
}


/**
 * @brief Renders the players pilot target.
 *
 *    @double dt Current delta tick.
 */
static void gui_renderTarget( double dt )
{
   (void) dt;
   Pilot *p;
   glColour *c;
   double x, y;

   /* Player is most likely dead. */
   if (gui.gfx_targetPilot == NULL)
      return;

   /* Get the target. */
   if (player->target != PLAYER_ID)
      p = pilot_get(player->target);
   else p = NULL;

   /* Make sure pilot exists and is still alive. */
   if ((p==NULL) || pilot_isFlag(p,PILOT_DEAD)) {
      player->target = PLAYER_ID;
      return;
   }

   /* Make sure target is still in range. */
   if (!pilot_inRangePilot( player, p )) {
      player->target = PLAYER_ID;
      return;
   }

   /* Draw the pilot target. */
   if (pilot_isDisabled(p)) 
      c = &cInert;
   else if (pilot_isFlag(p,PILOT_BRIBED))
      c = &cNeutral;
   else if (pilot_isHostile(p))
      c = &cHostile;
   else if (pilot_isFriendly(p))
      c = &cFriend;
   else
      c = faction_getColour(p->faction);

   x = p->solid->pos.x - p->ship->gfx_space->sw * PILOT_SIZE_APROX/2.;
   y = p->solid->pos.y + p->ship->gfx_space->sh * PILOT_SIZE_APROX/2.;
   gl_blitSprite( gui.gfx_targetPilot, x, y, 0, 0, c ); /* top left */

   x += p->ship->gfx_space->sw * PILOT_SIZE_APROX;
   gl_blitSprite( gui.gfx_targetPilot, x, y, 1, 0, c ); /* top right */

   y -= p->ship->gfx_space->sh * PILOT_SIZE_APROX;
   gl_blitSprite( gui.gfx_targetPilot, x, y, 1, 1, c ); /* bottom right */

   x -= p->ship->gfx_space->sw * PILOT_SIZE_APROX;
   gl_blitSprite( gui.gfx_targetPilot, x, y, 0, 1, c ); /* bottom left */
}


/**
 * @brief Renders the ships/planets in the border.
 *
 *    @param dt Current delta tick.
 */
static void gui_renderBorder( double dt )
{
   (void) dt;
   double z;
   int i, j;
   Pilot *plt;
   Planet *pnt;
   glTexture *tex;
   Vector2d *pos;
   int hw, hh;
   int cw, ch;
   double rx,ry, crx,cry;
   double cx,cy;
   glColour *col;
   double a, int_a;
   GLfloat vertex[5*2], colours[5*4];

   /* Get zoom. */
   gl_cameraZoomGet( &z );

   /* Get player position. */
   pos   = &player->solid->pos;
   hw    = SCREEN_W/2;  
   hh    = SCREEN_H/2;

   /* Interference. */
   int_a = 1. - interference_alpha;

   /* Draw planets. */
   for (i=0; i<cur_system->nplanets; i++) {
      pnt = cur_system->planets[i];
      tex = pnt->gfx_space;

      /* See if in sensor range. */
      if (!pilot_inRangePlanet(player, i))
         continue;

      /* Get relative positions. */
      rx = (pnt->pos.x - player->solid->pos.x) * z;
      ry = (pnt->pos.y - player->solid->pos.y) * z;

      /* Correct for offset. */
      crx = rx - gui_xoff;
      cry = ry - gui_yoff;

      /* Compare dimensions. */
      cw = hw + tex->sw/2;
      ch = hh + tex->sh/2;

      /* Check if out of range. */
      if ((ABS(crx) > cw) || (ABS(cry) > ch)) {
         /* Get angle. */
         a = atan2( ry, rx );
         if (a < 0.)
            a += 2.*M_PI;

         /* Handle by quadrant. */
         if ((a > M_PI/4.) && (a < M_PI*3./4.)) {
            cx = cos(a) * (hw-7.) * M_SQRT2;
            cy = hh-7.;
         }
         else if ((a > M_PI*3./4.) && (a < M_PI*5./4.)) {
            cx = -hw+7.;
            cy = sin(a) * (hh-7.) * M_SQRT2;
         }
         else if ((a > M_PI*5./4.) && (a < M_PI*7./4.)) {
            cx = cos(a) * (hw-7.) * M_SQRT2;
            cy = -hh+7.;
         }
         else {
            cx = hw-7.;
            cy = sin(a) * (hh-7.) * M_SQRT2;
         }


         /* Set up colours. */
         col = gui_getPlanetColour(i);
         for (j=0; j<5; j++) {
            colours[4*j + 0] = col->r;
            colours[4*j + 1] = col->g;
            colours[4*j + 2] = col->b;
            colours[4*j + 3] = int_a;
         }
         gl_vboSubData( gui_vbo, gui_vboColourOffset,
               sizeof(GLfloat) * 5*4, colours );
         /* Set up vertex. */
         vertex[0] = cx-5.;
         vertex[1] = cy-5;
         vertex[2] = cx-5.;
         vertex[3] = cy+5;
         vertex[4] = cx+5.;
         vertex[5] = cy+5;
         vertex[6] = cx+5.;
         vertex[7] = cy-5;
         vertex[8] = cx-5.;
         vertex[9] = cy-5;
         gl_vboSubData( gui_vbo, 0, sizeof(GLfloat) * 5*2, vertex );
         /* Draw tho VBO. */
         gl_vboActivateOffset( gui_vbo, GL_VERTEX_ARRAY, 0, 2, GL_FLOAT, 0 );
         gl_vboActivateOffset( gui_vbo, GL_COLOR_ARRAY,
               gui_vboColourOffset, 4, GL_FLOAT, 0 );
         glDrawArrays( GL_LINE_STRIP, 0, 5 );
      }
   }

   /* Draw pilots. */
   for (i=1; i<pilot_nstack; i++) { /* skip the player */
      plt = pilot_stack[i];
      tex = plt->ship->gfx_space;

      /* See if in sensor range. */
      if (!pilot_inRangePilot(player, plt))
         continue;

      /* Get relative positions. */
      rx = (plt->solid->pos.x - player->solid->pos.x)*z;
      ry = (plt->solid->pos.y - player->solid->pos.y)*z;

      /* Correct for offset. */
      rx -= gui_xoff;
      ry -= gui_yoff;

      /* Compare dimensions. */
      cw = hw + tex->sw/2;
      ch = hh + tex->sh/2;

      /* Check if out of range. */
      if ((ABS(rx) > cw) || (ABS(ry) > ch)) {
         /* Get angle. */
         a = atan2( ry, rx );
         if (a < 0.)
            a += 2.*M_PI;

         /* Handle by quadrant. */
         if ((a > M_PI/4.) && (a < M_PI*3./4.)) {
            cx = cos(a) * (hw-7.) * M_SQRT2;
            cy = hh-7.;
         }
         else if ((a > M_PI*3./4.) && (a < M_PI*5./4.)) {
            cx = -hw+7.;
            cy = sin(a) * (hh-7.) * M_SQRT2;
         }
         else if ((a > M_PI*5./4.) && (a < M_PI*7./4.)) {
            cx = cos(a) * (hw-7.) * M_SQRT2;
            cy = -hh+7.;
         }
         else {
            cx = hw-7.;
            cy = sin(a) * (hh-7.) * M_SQRT2;
         }

         /* Set up colours. */
         col = gui_getPilotColour(plt);
         for (j=0; j<4; j++) {
            colours[4*j + 0] = col->r;
            colours[4*j + 1] = col->g;
            colours[4*j + 2] = col->b;
            colours[4*j + 3] = int_a;
         }
         gl_vboSubData( gui_vbo, gui_vboColourOffset,
               sizeof(GLfloat) * 4*4, colours );
         /* Set up vertex. */
         vertex[0] = cx-5.;
         vertex[1] = cy-5;
         vertex[2] = cx+5.;
         vertex[3] = cy+5;
         vertex[4] = cx+5.;
         vertex[5] = cy-5;
         vertex[6] = cx-5.;
         vertex[7] = cy+5;
         gl_vboSubData( gui_vbo, 0, sizeof(GLfloat) * 4*2, vertex );
         /* Draw tho VBO. */
         gl_vboActivateOffset( gui_vbo, GL_VERTEX_ARRAY, 0, 2, GL_FLOAT, 0 );
         gl_vboActivateOffset( gui_vbo, GL_COLOR_ARRAY,
               gui_vboColourOffset, 4, GL_FLOAT, 0 );
         glDrawArrays( GL_LINES, 0, 4 );
      }
   }

   /* Deactivate the VBO. */
   gl_vboDeactivate();
}


static int can_jump = 0; /**< Stores whether or not the player is able to jump. */
/**
 * @brief Renders the player's GUI.
 *
 *    @param dt Current delta tick.
 */
void gui_render( double dt )
{
   int i, j;
   double x;
   char str[10];
   Pilot* p;
   glColour* c, col;
   glFont* f;
   StarSystem *sys;
   int q;

   /* If player is dead just render the cinematic mode. */
   if (player_isFlag(PLAYER_DESTROYED) || player_isFlag(PLAYER_CREATING) ||
        ((player != NULL) && pilot_isFlag(player,PILOT_DEAD))) {

      spfx_cinematic();
      return;
   }

   /* Make sure player is valid. */
   if (player==NULL)
      return;

   /*
    * Countdown timers.
    */
   blink_pilot -= dt;
   blink_planet -= dt;

   /* Render the border ships and targets. */
   gui_renderBorder(dt);
   gui_renderTarget(dt);

   /* Lockon warning */
   if (player->lockons > 0)
      gl_printMid( NULL, SCREEN_W - gui_xoff, 0., SCREEN_H-gl_defFont.h-25.,
            &cRed, "LOCK-ON DETECTED");

   /* Volatile environment. */
   if (cur_system->nebu_volatility > 0.)
      gl_printMid( NULL, SCREEN_W - gui_xoff, 0., SCREEN_H-gl_defFont.h*2.-35.,
            &cRed, "VOLATILE ENVIRONMENT DETECTED");

   /*
    *    G U I
    */
   /*
    * frame
    */
   gl_blitStatic( gui.gfx_frame, gui.frame.x, gui.frame.y, NULL );

   /* Radar. */
   gui_renderRadar(dt);


   /*
    * NAV 
    */
   if (planet_target >= 0) { /* planet landing target */
      gl_printMid( NULL, (int)gui.nav.w,
            gui.nav.x, gui.nav.y - 5,
            &cConsole, "Land" );

      gl_printMid( &gl_smallFont, (int)gui.nav.w,
            gui.nav.x, gui.nav.y - 10 - gl_smallFont.h,
            NULL, "%s", cur_system->planets[planet_target]->name );
   }
   else if (hyperspace_target >= 0) { /* hyperspace target */

      sys = system_getIndex( cur_system->jumps[hyperspace_target] );

      /* Determine if we have to play the "enter hyperspace range" sound. */
      i = space_canHyperspace(player);
      if ((i != 0) && (i != can_jump))
         if (!pilot_isFlag(player,PILOT_HYPERSPACE))
            player_playSound(snd_jump, 1);
      can_jump = i;

      /* Determine the colour of the NAV text. */
      if (can_jump || pilot_isFlag(player,PILOT_HYPERSPACE) ||
             pilot_isFlag(player,PILOT_HYP_PREP) ||
             pilot_isFlag(player,PILOT_HYP_BEGIN))
         c = &cConsole;
      else c = NULL;
      gl_printMid( NULL, (int)gui.nav.w,
            gui.nav.x, gui.nav.y - 5,
            c, "Hyperspace" );

      gl_printMid( &gl_smallFont, (int)gui.nav.w,
            gui.nav.x, gui.nav.y - 10 - gl_smallFont.h,
            NULL, "%d - %s", pilot_getJumps(player),
            (sys_isKnown(sys)) ? sys->name : "Unknown" );
   }
   else { /* no NAV target */
      gl_printMid( NULL, (int)gui.nav.w,
            gui.nav.x, gui.nav.y - 5,
            &cConsole, "Navigation" );

      gl_printMid( &gl_smallFont, (int)gui.nav.w,
            gui.nav.x, gui.nav.y - 10 - gl_smallFont.h,
            &cGrey, "Off" );
   }


   /*
    * health
    */
   gui_renderHealth( &gui.shield, player->shield / player->shield_max );
   gui_renderHealth( &gui.armour, player->armour / player->armour_max );
   gui_renderHealth( &gui.energy, player->energy / player->energy_max );
   gui_renderHealth( &gui.fuel, player->fuel / player->fuel_max );


   /* 
    * weapon
    */ 
   if ((player->secondary==NULL) || (player->secondary->outfit == NULL)) {
      gl_printMid( NULL, (int)gui.weapon.w,
            gui.weapon.x, gui.weapon.y - 5,
            &cConsole, "Secondary" ); 

      gl_printMid( &gl_smallFont, (int)gui.weapon.w,
            gui.weapon.x, gui.weapon.y - 10 - gl_defFont.h,
            &cGrey, "None"); 
   }  
   else {
      f = &gl_defFont;

      /* check to see if weapon is ready */
      if (player->secondary->timer > 0.)
         c = &cGrey;
      else
         c = &cConsole;

      /* Launcher. */
      if ((outfit_isLauncher(player->secondary->outfit) ||
               outfit_isFighterBay(player->secondary->outfit)) &&
            (player->secondary->u.ammo.outfit != NULL)) {

         /* Get quantity. */
         q = 0;
         for (i=0; i<player->outfit_nhigh; i++) {
            if (player->outfit_high[i].outfit != player->secondary->outfit)
               continue;
            
            if (player->outfit_high[i].u.ammo.outfit == player->secondary->u.ammo.outfit)
               q += player->outfit_high[i].u.ammo.quantity;
         }

         /* Weapon name. */
         gl_printMidRaw( f, (int)gui.weapon.w,
               gui.weapon.x, gui.weapon.y - 5,
               c, player->secondary->u.ammo.outfit->name );

         /* Print ammo left underneath. */
         gl_printMid( &gl_smallFont, (int)gui.weapon.w,
               gui.weapon.x, gui.weapon.y - 10 - gl_defFont.h,
               NULL, "%d", q );
      }
      /* Other. */
      else { /* just print the item name */
         /* Mark as out of ammo. */
         if (outfit_isLauncher(player->secondary->outfit) ||
                  outfit_isFighterBay(player->secondary->outfit))
            c = &cGrey;

         /* Render normally. */
         i = gl_printWidthRaw( f, player->secondary->outfit->name);
         if (i > (int)gui.weapon.w) /* font is too big */
            f = &gl_smallFont;
         gl_printMidRaw( f, (int)gui.weapon.w,
               gui.weapon.x, gui.weapon.y - (gui.weapon.h - f->h)/2.,
               c, player->secondary->outfit->name );
      }
   } 


   /*
    * target
    */
   if (player->target != PLAYER_ID) {
      p = pilot_get(player->target);

      /* blit the pilot target */
      gl_blitStatic( p->ship->gfx_target, gui.target.x, gui.target.y, NULL );
      /* blit the pilot space image */
      /*x = gui.target.x + (TARGET_WIDTH - p->ship->gfx_space->sw)/2.;
      y = gui.target.y + (TARGET_HEIGHT - p->ship->gfx_space->sh)/2.;
      gl_blitStaticSprite( p->ship->gfx_space,
            x, y, p->tsx, p->tsy, NULL );*/


      /* target name */
      i = gl_printWidth( NULL, p->name );
      gl_printMaxRaw( (i>gui.target_name.w) ? &gl_smallFont : NULL,
            gui.target_name.w,
            gui.target_name.x,
            gui.target_name.y,
            NULL, p->name );
      gl_printMaxRaw( &gl_smallFont, gui.target_faction.w,
            gui.target_faction.x,
            gui.target_faction.y,
            NULL, faction_name(p->faction) );

      /* target status */
      if (pilot_isDisabled(p)) /* pilot is disabled */
         gl_printMaxRaw( &gl_smallFont, gui.target_health.w,
               gui.target_health.x,
               gui.target_health.y,
               NULL, "Disabled" );

      else if (p->shield > p->shield_max * 5./100.) /* > 5% on shields */
         gl_printMax( &gl_smallFont, gui.target_health.w,
            gui.target_health.x,
               gui.target_health.y, NULL,
               "%s: %.0f%%", "Shield", p->shield/p->shield_max*100. );

      else /* on armour */
         gl_printMax( &gl_smallFont, gui.target_health.w,
               gui.target_health.x,
               gui.target_health.y, NULL, 
               "%s: %.0f%%", "Armour", p->armour/p->armour_max*100. );
   }
   else { /* no target */
      gl_printMidRaw( NULL, SHIP_TARGET_W,
            gui.target.x, gui.target.y  + (SHIP_TARGET_H - gl_defFont.h)/2.,
            &cGrey, "No Target" );
   }


   /*
    * misc
    */
   /* monies */
   j = gui.misc.y - 8 - gl_smallFont.h;
   gl_print( &gl_smallFont,
         gui.misc.x + 8, j,
         &cConsole, "Creds:" );
   credits2str( str, player->credits, 2 );
   i = gl_printWidth( &gl_smallFont, str );
   gl_print( &gl_smallFont,
         gui.misc.x + gui.misc.w - 8 - i, j,
         NULL, str );
   /* cargo and friends */
   if (player->ncommodities > 0) {
      j -= gl_smallFont.h + 5;
      gl_print( &gl_smallFont,
            gui.misc.x + 8, j,
            &cConsole, "Cargo:" );
      for (i=0; i < MIN(player->ncommodities,3); i++) { 
         j -= gl_smallFont.h + 3;
         if (player->commodities[i].quantity > 0.) /* quantity is over */
            gl_printMax( &gl_smallFont, gui.misc.w - 15,
                  gui.misc.x + 13, j,
                  NULL, "%d %s%s", player->commodities[i].quantity,
                  player->commodities[i].commodity->name,
                  (player->commodities[i].id) ? "*" : "" );
         else /* basically for weightless mission stuff */ 
            gl_printMax( &gl_smallFont, gui.misc.w - 15,
                  gui.misc.x + 13, j,
                  NULL, "%s%s",  player->commodities[i].commodity->name,
                  (player->commodities[i].id) ? "*" : "" );

      }
   }

   j -= gl_smallFont.h + 5;
   gl_print( &gl_smallFont,
         gui.misc.x + 8, j,
         &cConsole, "Free:" );
   i = gl_printWidth( &gl_smallFont, "%d", pilot_cargoFree(player) );
   gl_print( &gl_smallFont,
         gui.misc.x + gui.misc.w - 8 - i, j,
         NULL, "%d", pilot_cargoFree(player) );


   /* Messages. */
   gui_renderMessages(dt);


   /* OSD. */
   osd_render();

   /*
    * hyperspace
    */
   if (pilot_isFlag(player, PILOT_HYPERSPACE) &&
         (player->ptimer < HYPERSPACE_FADEOUT)) {
      if (i < j) {
         x = (HYPERSPACE_FADEOUT-player->ptimer) / HYPERSPACE_FADEOUT;
         col.r = 1.;
         col.g = 1.;
         col.b = 1.;
         col.a = x;
         gl_renderRect( -SCREEN_W/2., -SCREEN_H/2., SCREEN_W, SCREEN_H, &col );
      }
   }
}


/**
 * @brief Renders the GUI radar.
 *
 *    @param dt Current deltatick.
 */
static void gui_renderRadar( double dt )
{
   int i, j;
   GLfloat vertex[2*4], colours[4*4];

   gl_matrixMode( GL_PROJECTION );
   gl_matrixPush();
   if (gui.radar.shape==RADAR_RECT)
      gl_matrixTranslate( gui.radar.x - SCREEN_W/2. + gui.radar.w/2.,
            gui.radar.y - SCREEN_H/2. - gui.radar.h/2.);
   else if (gui.radar.shape==RADAR_CIRCLE)
      gl_matrixTranslate( gui.radar.x - SCREEN_W/2.,
            gui.radar.y - SCREEN_H/2.);

   /*
    * planets
    */
   for (i=0; i<cur_system->nplanets; i++)
      if (i != planet_target)
         gui_renderPlanet( i );
   if (planet_target > -1)
      gui_renderPlanet( planet_target );

   /*
    * weapons
    */
   weapon_minimap(gui.radar.res, gui.radar.w, gui.radar.h,
         gui.radar.shape, 1.-interference_alpha);


   /* render the pilot_nstack */
   j = 0;
   for (i=1; i<pilot_nstack; i++) { /* skip the player */
      if (pilot_stack[i]->id == player->target)
         j = i;
      else
         gui_renderPilot(pilot_stack[i]);
   }
   /* render the targetted pilot */
   if (j!=0)
      gui_renderPilot(pilot_stack[j]);

   /* Intereference. */
   gui_renderInterference(dt);

   /* the + sign in the middle of the radar representing the player */
   for (i=0; i<4; i++) {
      colours[4*i + 0] = cRadar_player.r;
      colours[4*i + 1] = cRadar_player.g;
      colours[4*i + 2] = cRadar_player.b;
      colours[4*i + 3] = cRadar_player.a;
   }
   gl_vboSubData( gui_vbo, gui_vboColourOffset,
         sizeof(GLfloat) * 4*4, colours );
   /* Set up vertex. */
   vertex[0] = 0.;
   vertex[1] = -3.;
   vertex[2] = 0.;
   vertex[3] = +3.;
   vertex[4] = -3.;
   vertex[5] = 0.;
   vertex[6] = +3.;
   vertex[7] = 0.;
   gl_vboSubData( gui_vbo, 0, sizeof(GLfloat) * 4*2, vertex );
   /* Draw tho VBO. */
   gl_vboActivateOffset( gui_vbo, GL_VERTEX_ARRAY, 0, 2, GL_FLOAT, 0 );
   gl_vboActivateOffset( gui_vbo, GL_COLOR_ARRAY,
         gui_vboColourOffset, 4, GL_FLOAT, 0 );
   glDrawArrays( GL_LINES, 0, 4 );
   gl_vboDeactivate();

   gl_matrixPop();
}


/**
 * @brief Clears the GUI messages.
 */
void gui_clearMessages (void)
{
   int i;
   for (i=0; i<mesg_max; i++) {
      mesg_stack[i].t = -1.;
   }
}


/**
 * @brief Renders the player's messages on screen.
 *
 *    @param dt Current delta tick.
 */
static void gui_renderMessages( double dt )
{
   double x, y;
   glColour c;
   int i;

   x = gui.mesg.x;
   y = gui.mesg.y + (double)(gl_defFont.h*mesg_max)*1.2;
   c.r = c.g = c.b = 1.;

   for (i=mesg_max-1; i>=0; i--) {
      y -= (double)gl_defFont.h*1.2;

      /* Only handle non-NULL messages. */
      if (mesg_stack[i].str[0] != '\0') {

         /* Decrement timer. */
         mesg_stack[i].t -= dt;

         /* Set to NULL if timer is up. */
         if (mesg_stack[i].t < 0.)
            mesg_stack[i].str[0] = '\0';

         /* Draw with variable alpha. */
         else {
            if (mesg_stack[i].t - mesg_timeout/2 < 0.)
               c.a = mesg_stack[i].t / (mesg_timeout/2.);
            else
               c.a = 1.;
            gl_print( NULL, x, y, &c, "%s", mesg_stack[i].str );
         }
      }
   }


}


/**
 * @brief Renders interference if needed.
 *
 *    @param dt Current deltatick.
 */
static void gui_renderInterference( double dt )
{
   glColour c;
   glTexture *tex;
   int t;

   /* Must be displaying interference. */
   if (interference_alpha <= 0.)
      return;

   /* Calculate frame to draw. */
   interference_t += dt;
   if (interference_t > INTERFERENCE_CHANGE_DT) { /* Time to change */
      t = RNG(0, INTERFERENCE_LAYERS-1);
      if (t != interference_layer)
         interference_layer = t;
      else
         interference_layer = (interference_layer == INTERFERENCE_LAYERS-1) ?
               0 : interference_layer+1;
      interference_t -= INTERFERENCE_CHANGE_DT;
   }

   /* Render the interference. */
   c.r = c.g = c.b = 1.;
   c.a = interference_alpha;
   tex = gui.radar.interference[interference_layer];
   if (gui.radar.shape == RADAR_CIRCLE)
   gl_blitStatic( tex,
      SCREEN_W/2. - gui.radar.w,
      SCREEN_H/2. - gui.radar.w,
      &c );
   else if (gui.radar.shape == RADAR_RECT)
      gl_blitStatic( tex,
            SCREEN_W/2. - gui.radar.w/2,
            SCREEN_H/2. - gui.radar.h/2, &c );
}


/**
 * @brief Gets the pilot colour.
 *
 *    @param p Pilot to get colour of.
 *    @return The colour of the pilot.
 */
static glColour* gui_getPilotColour( const Pilot* p )
{
   glColour *col;

   if (p->id == player->target) col = &cRadar_tPilot;
   else if (pilot_isDisabled(p)) col = &cInert;
   else if (pilot_isFlag(p,PILOT_BRIBED)) col = &cNeutral;
   else if (pilot_isHostile(p)) col = &cHostile;
   else if (pilot_isFriendly(p)) col = &cFriend;
   else col = faction_getColour(p->faction);

   return col;
}


/**
 * @brief Renders a pilot in the GUI radar.
 *
 *    @param p Pilot to render.
 */
#define CHECK_PIXEL(x,y)   \
(gui.radar.shape==RADAR_RECT && ABS(x)<w/2. && ABS(y)<h/2.) || \
   (gui.radar.shape==RADAR_CIRCLE && (((x)*(x)+(y)*(y)) <= rc))
static void gui_renderPilot( const Pilot* p )
{
   int i, curs;
   int x, y, sx, sy;
   double w, h;
   double px, py;
   glColour *col, ccol;
   double a;
   GLfloat vertex[2*8], colours[4*8];
   GLfloat cx, cy;
   int rc;

   /* Make sure is in range. */
   if (!pilot_inRangePilot( player, p ))
      return;

   /* Get position. */
   x = (p->solid->pos.x - player->solid->pos.x) / gui.radar.res;
   y = (p->solid->pos.y - player->solid->pos.y) / gui.radar.res;
   /* Get size. */
   sx = PILOT_SIZE_APROX/2. * p->ship->gfx_space->sw / gui.radar.res;
   sy = PILOT_SIZE_APROX/2. * p->ship->gfx_space->sh / gui.radar.res;
   if (sx < 1.)
      sx = 1.;
   if (sy < 1.)
      sy = 1.;

   /* Check if pilot in range. */
   if ( ((gui.radar.shape==RADAR_RECT) &&
            ((ABS(x) > gui.radar.w/2+sx) || (ABS(y) > gui.radar.h/2.+sy)) ) ||
         ((gui.radar.shape==RADAR_CIRCLE) &&
            ((x*x+y*y) > (int)(gui.radar.w*gui.radar.w))) ) {

      /* Draw little targetted symbol. */
      if (p->id == player->target) {
         /* Circle radars have it easy. */
         if (gui.radar.shape==RADAR_CIRCLE)  {
            /* We'll create a line. */
            a = ANGLE(x,y);
            x = gui.radar.w * cos(a);
            y = gui.radar.w * sin(a);
            sx = 0.85 * x;
            sy = 0.85 * y;

            /* Set up colours. */
            for (i=0; i<2; i++) {
               colours[4*i + 0] = cRadar_tPilot.r;
               colours[4*i + 1] = cRadar_tPilot.g;
               colours[4*i + 2] = cRadar_tPilot.b;
               colours[4*i + 3] = 1.-interference_alpha;
            }
            gl_vboSubData( gui_vbo, gui_vboColourOffset,
                  sizeof(GLfloat) * 2*4, colours );
            /* Set up vertex. */
            vertex[0] = x;
            vertex[1] = y;
            vertex[2] = sx;
            vertex[3] = sy;
            gl_vboSubData( gui_vbo, 0, sizeof(GLfloat) * 2*2, vertex );
            /* Draw tho VBO. */
            gl_vboActivateOffset( gui_vbo, GL_VERTEX_ARRAY, 0, 2, GL_FLOAT, 0 );
            gl_vboActivateOffset( gui_vbo, GL_COLOR_ARRAY,
                  gui_vboColourOffset, 4, GL_FLOAT, 0 );
            glDrawArrays( GL_LINES, 0, 2 );
         }
      }
      return;
   }

   if (gui.radar.shape==RADAR_RECT) {
      w = gui.radar.w/2.;
      h = gui.radar.h/2.;
   }
   else if (gui.radar.shape==RADAR_CIRCLE) {
      w = gui.radar.w;
      h = gui.radar.w;
      rc = (int)(gui.radar.w*gui.radar.w);
   }

   /* Draw selection if targetted. */
   if (p->id == player->target) {
      if (blink_pilot < RADAR_BLINK_PILOT/2.) {
         /* Set up colours. */
         for (i=0; i<8; i++) {
            colours[4*i + 0] = cRadar_tPilot.r;
            colours[4*i + 1] = cRadar_tPilot.g;
            colours[4*i + 2] = cRadar_tPilot.b;
            colours[4*i + 3] = 1.-interference_alpha;
         }
         gl_vboSubData( gui_vbo, gui_vboColourOffset,
               sizeof(GLfloat) * 8*4, colours );
         /* Set up vertex. */
         curs = 0;
         cx = x-sx;
         cy = y+sy;
         if (CHECK_PIXEL(cx-3.3,cy+3.3)) {
            vertex[curs++] = cx-1.5;
            vertex[curs++] = cy+1.5;
            vertex[curs++] = cx-3.3;
            vertex[curs++] = cy+3.3;
         }
         cx = x+sx;
         if (CHECK_PIXEL(cx+3.3,cy+3.3)) {
            vertex[curs++] = cx+1.5;
            vertex[curs++] = cy+1.5;
            vertex[curs++] = cx+3.3;
            vertex[curs++] = cy+3.3;
         }
         cy = y-sy;
         if (CHECK_PIXEL(cx+3.3,cy-3.3)) {
            vertex[curs++] = cx+1.5;
            vertex[curs++] = cy-1.5;
            vertex[curs++] = cx+3.3;
            vertex[curs++] = cy-3.3;
         }
         cx = x-sx;
         if (CHECK_PIXEL(cx-3.3,cy-3.3)) {
            vertex[curs++] = cx-1.5;
            vertex[curs++] = cy-1.5;
            vertex[curs++] = cx-3.3;
            vertex[curs++] = cy-3.3;
         }
         gl_vboSubData( gui_vbo, 0, sizeof(GLfloat) * (curs/2)*2, vertex );
         /* Draw tho VBO. */
         gl_vboActivateOffset( gui_vbo, GL_VERTEX_ARRAY, 0, 2, GL_FLOAT, 0 );
         gl_vboActivateOffset( gui_vbo, GL_COLOR_ARRAY,
               gui_vboColourOffset, 4, GL_FLOAT, 0 );
         glDrawArrays( GL_LINES, 0, curs/2 );
      }

      if (blink_pilot < 0.)
         blink_pilot += RADAR_BLINK_PILOT;
   }

   /* Deactivate VBO. */
   gl_vboDeactivate();

   /* Draw square. */
   px = MAX(x-sx,-w);
   py = MAX(y-sy, -h);
   col = gui_getPilotColour(p);
   ccol.r = col->r;
   ccol.g = col->g;
   ccol.b = col->b;
   ccol.a = 1.-interference_alpha;
   gl_renderRect( px, py, MIN( 2*sx, w-px ), MIN( 2*sy, h-py ), &ccol );
}


/**
 * @brief Gets the colour of a planet.
 *
 *    @param i Index of the planet to get colour of.
 *    @return Colour of the planet.
 */
static glColour *gui_getPlanetColour( int i )
{
   glColour *col;
   Planet *planet;

   planet = cur_system->planets[i];

   col = faction_getColour(planet->faction);
   if (i == planet_target)
      col = &cRadar_tPlanet;
   else if ((col != &cHostile) && !planet_hasService(planet,PLANET_SERVICE_BASIC))
      col = &cInert; /* Override non-hostile planets without service. */
   
   return col;
}


#define PIXEL(x,y)      \
   if (CHECK_PIXEL(x,y)) \
   glVertex2i((x),(y)) /**< Puts a pixel on radar if inbounds. */
/**
 * @brief Draws the planets in the minimap.
 *
 * Matrix mode is already displaced to center of the minimap.
 */
static void gui_renderPlanet( int ind )
{
   int i, curs;
   int cx, cy, x, y, r, rc;
   int w, h;
   double res;
   double a, tx,ty;
   GLfloat vx, vy, vr;
   glColour *col;
   Planet *planet;
   GLfloat vertex[8*2], colours[8*4];

   /* Make sure is in range. */
   if (!pilot_inRangePlanet( player, ind ))
      return;

   /* Default values. */
   res = gui.radar.res;
   planet = cur_system->planets[ind];
   w = gui.radar.w;
   h = gui.radar.h;
   r = (int)(planet->gfx_space->sw / res);
   vr = r;
   cx = (int)((planet->pos.x - player->solid->pos.x) / res);
   cy = (int)((planet->pos.y - player->solid->pos.y) / res);
   if (gui.radar.shape==RADAR_CIRCLE)
      rc = (int)(gui.radar.w*gui.radar.w);

   /* Check if in range. */
   if (gui.radar.shape == RADAR_RECT) {
      /* Out of range. */
      if ((ABS(cx) - r > w/2.) || (ABS(cy) - r  > h/2.))
         return;
   }
   else if (gui.radar.shape == RADAR_CIRCLE) {
      x = ABS(cx)-r;
      y = ABS(cy)-r;
      /* Out of range. */
      if (x*x + y*y > rc) {
         if (planet_target == ind) {
            /* Draw a line like for pilots. */
            a = ANGLE(cx,cy);
            tx = w*cos(a);
            ty = w*sin(a);

            /* Set the colour. */
            for (i=0; i<2; i++) {
               colours[4*i + 0] = cRadar_tPlanet.r;
               colours[4*i + 1] = cRadar_tPlanet.g;
               colours[4*i + 2] = cRadar_tPlanet.b;
               colours[4*i + 3] = 1.-interference_alpha;
            }
            gl_vboSubData( gui_vbo, gui_vboColourOffset,
                  sizeof(GLfloat) * 2*4, colours );
            /* Set the vertex. */
            vertex[0] = tx;
            vertex[1] = ty;
            vertex[2] = 0.85*tx;
            vertex[3] = 0.85*ty;
            gl_vboSubData( gui_vbo, 0, sizeof(GLfloat) * 2*2, vertex );
            /* Draw tho VBO. */
            gl_vboActivateOffset( gui_vbo, GL_VERTEX_ARRAY, 0, 2, GL_FLOAT, 0 );
            gl_vboActivateOffset( gui_vbo, GL_COLOR_ARRAY,
                  gui_vboColourOffset, 4, GL_FLOAT, 0 );
            glDrawArrays( GL_LINES, 0, 2 );
         }
         return;
      }
   }

   /* Do the blink. */
   if (ind == planet_target) {
      if (blink_planet < RADAR_BLINK_PLANET/2.) {
         curs = 0;
         vx = cx-vr;
         vy = cy+vr;
         if (CHECK_PIXEL(vx-3.3, vy+3.3)) {
            vertex[curs++] = vx-1.5;
            vertex[curs++] = vy+1.5;
            vertex[curs++] = vx-3.3;
            vertex[curs++] = vy+3.3;
         }
         vx = cx+vr;
         if (CHECK_PIXEL(vx+3.3, vy+3.3)) {
            vertex[curs++] = vx+1.5;
            vertex[curs++] = vy+1.5;
            vertex[curs++] = vx+3.3;
            vertex[curs++] = vy+3.3;
         }
         vy = cy-vr;
         if (CHECK_PIXEL(vx+3.3, vy-3.3)) {
            vertex[curs++] = vx+1.5;
            vertex[curs++] = vy-1.5;
            vertex[curs++] = vx+3.3;
            vertex[curs++] = vy-3.3;
         }
         vx = cx-vr;
         if (CHECK_PIXEL(vx-3.3, vy-3.3)) {
            vertex[curs++] = vx-1.5;
            vertex[curs++] = vy-1.5;
            vertex[curs++] = vx-3.3;
            vertex[curs++] = vy-3.3;
         }
         gl_vboSubData( gui_vbo, 0, sizeof(GLfloat) * (curs/2)*2, vertex );
         /* Set the colours. */
         for (i=0; i<curs/2; i++) {
            colours[4*i + 0] = cRadar_tPlanet.r;
            colours[4*i + 1] = cRadar_tPlanet.g;
            colours[4*i + 2] = cRadar_tPlanet.b;
            colours[4*i + 3] = 1.-interference_alpha;
         }
         gl_vboSubData( gui_vbo, gui_vboColourOffset,
               sizeof(GLfloat) * (curs/2)*4, colours );
         /* Draw tho VBO. */
         gl_vboActivateOffset( gui_vbo, GL_VERTEX_ARRAY, 0, 2, GL_FLOAT, 0 );
         gl_vboActivateOffset( gui_vbo, GL_COLOR_ARRAY,
               gui_vboColourOffset, 4, GL_FLOAT, 0 );
         glDrawArrays( GL_LINES, 0, curs/2 );
      }

      if (blink_planet < 0.)
         blink_planet += RADAR_BLINK_PLANET;
   }

   /* Get the colour. */
   col = gui_getPlanetColour(ind);
   for (i=0; i<5; i++) {
      colours[4*i + 0] = col->r;
      colours[4*i + 1] = col->g;
      colours[4*i + 2] = col->b;
      colours[4*i + 3] = 1.-interference_alpha;
   }
   gl_vboSubData( gui_vbo, gui_vboColourOffset,
      sizeof(GLfloat) * 5*4, colours );
   /* Now load the data. */
   vx = cx;
   vy = cy;
   vr = MAX( vr, 3. ); /* Make sure it's visible. */
   vertex[0] = vx;
   vertex[1] = vy + vr;
   vertex[2] = vx + vr;
   vertex[3] = vy;
   vertex[4] = vx;
   vertex[5] = vy - vr;
   vertex[6] = vx - vr;
   vertex[7] = vy;
   vertex[8] = vertex[0];
   vertex[9] = vertex[1];
   gl_vboSubData( gui_vbo, 0, sizeof(GLfloat) * 5*2, vertex );
   /* Draw tho VBO. */
   gl_vboActivateOffset( gui_vbo, GL_VERTEX_ARRAY, 0, 2, GL_FLOAT, 0 );
   gl_vboActivateOffset( gui_vbo, GL_COLOR_ARRAY,
         gui_vboColourOffset, 4, GL_FLOAT, 0 );
   glDrawArrays( GL_LINE_STRIP, 0, 5 );

   /* Deactivate the VBO. */
   gl_vboDeactivate();
}
#undef PIXEL
#undef CHECK_PIXEL



/**
 * @brief Renders a health bar.
 *
 *    @param bar Health bar to render.
 *    @param w Width of the health bar.
 */
static void gui_renderHealth( const HealthBar *bar, const double w )
{
   int i;
   double res[2], rw;
   double x,y, sx,sy, tx,ty;
   GLfloat vertex[4*4], colours[4*4];

   /* Check if need to draw. */
   if (w == 0.)
      return;

   /* Just create a bar. */
   if (bar->gfx == NULL) {
      /* Set the position values. */
      x = bar->rect.x - SCREEN_W/2.;
      y = bar->rect.y - SCREEN_H/2.;
      sx = w * bar->rect.w;
      sy = bar->rect.h;

      /* Render the bar. */
      gl_renderRect( x, y-sy, sx, sy, &bar->col );
   }
   /* Render the texture. */
   else {
      /* Calculate the line.
       * y = slope * x + offset
       * w = 1 / area * ( slope * x^2 / 2 + offset )
       * we need to isolate x. */
      if (nmath_solve2Eq( res, bar->slope / 2.,
            bar->offset, -bar->area * w ))
         WARN("Failed to solve equation: %f*x^2 + %f*x + %f = 0",
                bar->slope / 2., bar->offset, -bar->area * w );
      if (res[0] > 0.)
         rw = res[0] / bar->gfx->sw;
      else
         rw = res[1] / bar->gfx->sw;

      /* Set the position values. */
      x = bar->rect.x - SCREEN_W/2.;
      y = bar->rect.y - SCREEN_H/2. + bar->gfx->sh;
      sx = rw * bar->gfx->sw;
      sy = bar->gfx->sh;
      tx = bar->gfx->sw / bar->gfx->rw;
      ty = bar->gfx->sh / bar->gfx->rh;

      /* Set the colour. */
      COLOUR(bar->col); 

      /* Draw the image. */
      glEnable(GL_TEXTURE_2D);
      glBindTexture( GL_TEXTURE_2D, bar->gfx->texture);
      /* Set up colours. */
      for (i=0; i<4; i++) {
         colours[4*i + 0] = bar->col.r;
         colours[4*i + 1] = bar->col.g;
         colours[4*i + 2] = bar->col.b;
         colours[4*i + 3] = bar->col.a;
      }
      gl_vboSubData( gui_vbo, gui_vboColourOffset,
            sizeof(GLfloat) * 4*4, colours );
      /* Set up vertex. */
      /* Position. */
      vertex[0] = x;
      vertex[1] = y;
      vertex[2] = x + sx;
      vertex[3] = y;
      vertex[4] = x + sx;
      vertex[5] = y - sy;
      vertex[6] = x;
      vertex[7] = y - sy;
      /* Texture. */
      vertex[8]  = 0.;
      vertex[9]  = ty;
      vertex[10] = rw*tx;
      vertex[11] = ty;
      vertex[12] = rw*tx;
      vertex[13] = 0.;
      vertex[14] = 0.;
      vertex[15] = 0.;
      gl_vboSubData( gui_vbo, 0, sizeof(GLfloat) * 4*4, vertex );
      /* Draw tho VBO. */
      gl_vboActivateOffset( gui_vbo, GL_VERTEX_ARRAY, 0, 2, GL_FLOAT, 0 );
      gl_vboActivateOffset( gui_vbo, GL_TEXTURE_COORD_ARRAY,
            sizeof(GLfloat) * 2*4, 2, GL_FLOAT, 0 );
      gl_vboActivateOffset( gui_vbo, GL_COLOR_ARRAY,
            gui_vboColourOffset, 4, GL_FLOAT, 0 );
      glDrawArrays( GL_QUADS, 0, 4 );
      gl_vboDeactivate();
      glDisable(GL_TEXTURE_2D);
   }

}


/**
 * @brief Initializes the GUI system.
 *
 *    @return 0 on success;
 */
int gui_init (void)
{
   /*
    * set gfx to NULL
    */
   memset(&gui, 0, sizeof(GUI));

   /*
    * radar
    */
   gui.radar.res = RADAR_RES_DEFAULT;

   /*
    * messages
    */
   gui.mesg.x = 20;
   gui.mesg.y = 30;
   if (mesg_stack == NULL) {
      mesg_stack = calloc(mesg_max, sizeof(Mesg));
      if (mesg_stack == NULL) {
         ERR("Out of memory!");
         return -1;
      }
   }

   /*
    * VBO.
    */
   if (gui_vbo == NULL) {
      gui_vbo = gl_vboCreateStream( sizeof(GLfloat) * 8*(2+4), NULL );
      gui_vboColourOffset = sizeof(GLfloat) * 8*2;
   }

   /*
    * OSD
    */
   osd_setup( 30., SCREEN_H-90., 150., 300. );

   return 0;
}


/**
 * @brief Attempts to load the actual GUI.
 *
 *    @param name Name of the GUI to load.
 *    @return 0 on success.
 */
int gui_load( const char* name )
{
   uint32_t bufsize;
   char *buf = ndata_read( GUI_DATA, &bufsize );
   char *tmp;
   int found = 0;

   xmlNodePtr node;
   xmlDocPtr doc = xmlParseMemory( buf, bufsize );

   node = doc->xmlChildrenNode;
   if (!xml_isNode(node,XML_GUI_ID)) {
      ERR("Malformed '"GUI_DATA"' file: missing root element '"XML_GUI_ID"'");
      return -1;
   }

   node = node->xmlChildrenNode; /* first system node */
   if (node == NULL) {
      ERR("Malformed '"GUI_DATA"' file: does not contain elements");
      return -1;
   }                                                                                       
   do {
      if (xml_isNode(node, XML_GUI_TAG)) {

         tmp = xml_nodeProp(node,"name"); /* mallocs */

         /* is the gui we are looking for? */
         if (strcmp(tmp,name)==0) {
            found = 1;

            /* parse the xml node */
            if (gui_parse(node,name))
               WARN("Trouble loading GUI '%s'", name);
            free(tmp);
            break;
         }

         free(tmp);
      }
   } while (xml_nextNode(node));

   xmlFreeDoc(doc);
   free(buf);

   if (!found) {
      WARN("GUI '%s' not found in '"GUI_DATA"'",name);
      return -1;
   }

   return 0;
}


/**
 * @brief Parse a parameter of the rect node.
 */
static void rect_parseParam( const xmlNodePtr parent,
      char *name, double *param )
{
   char *buf;

   /* Get the attribute. */
   xmlr_attr( parent, name, buf );

   /* Wants attribute. */
   if (param != NULL) {
      if (buf == NULL)
         WARN("Node '%s' missing '%s' parameter.", parent->name, name);
      else if (buf != NULL)
         *param = atoi(buf);
   }
   /* Doesn't want it. */
   else if (buf != NULL)
      WARN("Node '%s' has superfluous '%s' parameter.", parent->name, name);

   /* Clean up. */
   if (buf != NULL)
      free(buf);
}


/**
 * @brief Used to pull out a rect from an xml node.
 */
static void rect_parse( const xmlNodePtr parent,
      double *x, double *y, double *w, double *h )
{
   rect_parseParam( parent, "w", w );
   rect_parseParam( parent, "h", h );
   rect_parseParam( parent, "x", x );
   rect_parseParam( parent, "y", y );
}


/**
 * @brief Creates teh interference map for the current gui.
 */
static void gui_createInterference (void)
{
   uint8_t raw;
   int i, j, k;
   float *map;
   uint32_t *pix;
   SDL_Surface *sur;
   int w,h, hw,hh;
   float c;
   int r;

   /* Dimension shortcuts. */
   if (gui.radar.shape == RADAR_CIRCLE) {
      w = gui.radar.w*2.;
      h = w;
   }
   else if (gui.radar.shape == RADAR_RECT) {
      w = gui.radar.w;
      h = gui.radar.h;
   }

   for (k=0; k<INTERFERENCE_LAYERS; k++) {

      /* Free the old texture. */
      if (gui.radar.interference[k] != NULL)
         gl_freeTexture(gui.radar.interference[k]);

      /* Create the temporary surface. */
      sur = SDL_CreateRGBSurface( SDL_SWSURFACE, w, h, 32, RGBAMASK );
      pix = sur->pixels;

      /* Clear pixels. */
      memset( pix, 0, sizeof(uint32_t)*w*h );

      /* Load the interference map. */
      map = noise_genRadarInt( w, h, 100. );

      /* Create the texture. */
      SDL_LockSurface( sur );
      if (gui.radar.shape == RADAR_CIRCLE) {
         r = pow2((int)gui.radar.w);
         hw = w/2;
         hh = h/2;
         for (i=0; i<h; i++) {
            for (j=0; j<w; j++) {
               /* Must be in circle. */
               if (pow2(i-hh) + pow2(j-hw) > r)
                  continue;
               c = map[i*w + j];
               raw = 0xff & (uint8_t)((float)0xff * c);
               memset( &pix[i*w + j], raw, sizeof(uint32_t) );
               pix[i*w + j] |= AMASK;
            }
         }
      }
      else if (gui.radar.shape == RADAR_RECT) {
         for (i=0; i<h*w; i++) {
            /* Process pixels. */
            c = map[i];
            raw = 0xff & (uint8_t)((float)0xff * c);
            memset( &pix[i], raw, sizeof(uint32_t) );
            pix[i] |= AMASK;
         }
      }
      SDL_UnlockSurface( sur );

      /* Set the interference. */
      gui.radar.interference[k] = gl_loadImage( sur, 0 );

      /* Clean up. */
      free(map);
   }
}


/**
 * @brief Parses a healthbar.
 *
 *    @param parent Parent node of the healthbar.
 *    @param bar Bar to load into.
 *    @param col Default colour to use.
 */
static int gui_parseBar( xmlNodePtr parent, HealthBar *bar, const glColour *col )
{
   int i, j;
   char *tmp, buf[PATH_MAX];
   double x, y, m, n;
   double sumx, sumy, sumxx, sumxy;

   /* Clear memory. */
   memset( bar, 0, sizeof(HealthBar) );

   /* Parse the rectangle. */
   rect_parse( parent, &bar->rect.x, &bar->rect.y,
         &bar->rect.w, &bar->rect.h );

   /* Load the colour. */
   memcpy( &bar->col, col, sizeof(glColour) );
   xmlr_attr( parent, "alpha", tmp );
   if (tmp != NULL) {
      bar->col.a = atof(tmp);
      free(tmp);
   }

   /* Check for graphics. */
   tmp = xml_get(parent);
   if (tmp != NULL) {
      snprintf( buf, PATH_MAX, GUI_GFX"%s.png", tmp );
      bar->gfx = gl_newImage( buf, OPENGL_TEX_MAPTRANS );

      /* We do a slope aproximation now. */
      n = 0.;
      sumx = 0.;
      sumy = 0.;
      sumxx = 0.;
      sumxy = 0.;
      for (i=0; i<bar->gfx->sw; i++) {
         x = (double)i;
         for (j=0; j<bar->gfx->sh; j++) {
            y = (double)j;
            /* Count dots. */
            if (gl_isTrans( bar->gfx, i, j)) {
               sumx += x;
               sumy += y;
               sumxx += x*x;
               sumxy += x*y;
               n++;
            }
         }
      }

      /* Now calculate slope parameters. */
      bar->slope  = (sumx*sumy - n*sumxy);
      bar->slope /= (sumx*sumx - n*sumxx);
      bar->offset = (sumy - bar->slope*sumx) / n;
      if (bar->offset < 0.) /* must be positive to ensure solution. */
         bar->offset = 0.;

      /* Calculate the area. */
      bar->area = bar->slope/2. * pow(bar->gfx->sw, 2.) + bar->offset * bar->gfx->sw;

      /* Check to see if area is the top triangle or bottom one. */
      m = sumx / n;
      if (sumy/n < bar->gfx->sh/2.) {
      /*if ((bar->slope/2. * pow(m, 2.) + bar->offset * m) < (sumy / n)) {*/
         bar->slope = -bar->slope;
         bar->offset = bar->gfx->sh - bar->offset;
         bar->area = (bar->gfx->sw * bar->gfx->sh) - bar->area;
      }
   }

   return 0;
}

#define RELATIVIZE(a)   \
{(a).x += VX(gui.frame); \
(a).y = VY(gui.frame)+gui.gfx_frame->h-(a).y;}
/**< Converts a rect to absolute coords. */
/**
 * @brief Parses a gui node.
 *
 *    @param parent node to parse from.
 *    @param name Name of the GUI to load.
 */
static int gui_parse( const xmlNodePtr parent, const char *name )
{
   xmlNodePtr cur, node;
   char *tmp, buf[PATH_MAX];

   /*
    * Clean up.
    */
   gui_cleanup();

   /*
    * gfx
    */
   /* set as a property and not a node because it must be loaded first */
   tmp = xml_nodeProp(parent,"gfx");
   if (tmp==NULL) {
      ERR("GUI '%s' has no gfx property",name);
      return -1;
   }

   /* load gfx */
   /* frame */
   snprintf( buf, PATH_MAX, GUI_GFX"%s.png", tmp );
   gui.gfx_frame = gl_newImage( buf, 0 );
   /* pilot */
   snprintf( buf, PATH_MAX, GUI_GFX"%s_pilot.png", tmp );
   gui.gfx_targetPilot = gl_newSprite( buf, 2, 2, OPENGL_TEX_MIPMAPS );
   /* planet */
   snprintf( buf, PATH_MAX, GUI_GFX"%s_planet.png", tmp );
   gui.gfx_targetPlanet = gl_newSprite( buf, 2, 2, OPENGL_TEX_MIPMAPS );
   free(tmp);

   /*
    * frame (based on gfx)
    */
   vect_csetmin( &gui.frame,
         SCREEN_W - gui.gfx_frame->w - 15,     /* x */
         SCREEN_H - gui.gfx_frame->h - 15);   /* y */

   /* now actually parse the data */
   node = parent->children;
   do { /* load all the data */

      /*
       * offset
       */
      if (xml_isNode(node,"offset"))
         rect_parse( node, &gui_xoff, &gui_yoff, NULL, NULL );

      /*
       * radar
       */
      else if (xml_isNode(node,"radar")) {

         tmp = xml_nodeProp(node,"type");

         /* make sure type is valid */
         if (strcmp(tmp,"rectangle")==0) gui.radar.shape = RADAR_RECT;
         else if (strcmp(tmp,"circle")==0) gui.radar.shape = RADAR_CIRCLE;
         else {
            WARN("Radar for GUI '%s' is missing 'type' tag or has invalid 'type' tag",name);
            gui.radar.shape = RADAR_RECT;
         }

         free(tmp);
      
         /* load the appropriate measurements */
         if (gui.radar.shape == RADAR_RECT)
            rect_parse( node, &gui.radar.x, &gui.radar.y, &gui.radar.w, &gui.radar.h );
         else if (gui.radar.shape == RADAR_CIRCLE)
            rect_parse( node, &gui.radar.x, &gui.radar.y, &gui.radar.w, NULL );
         RELATIVIZE(gui.radar);
      }

      /*
       * nav computer
       */
      else if (xml_isNode(node,"nav")) {
         rect_parse( node, &gui.nav.x, &gui.nav.y, &gui.nav.w, &gui.nav.h );
         RELATIVIZE(gui.nav);
         gui.nav.y -= gl_defFont.h;
      }

      /*
       * health bars
       */
      else if (xml_isNode(node,"health")) {
         cur = node->children;
         do {
            if (xml_isNode(cur,"shield")) {
               gui_parseBar( cur, &gui.shield, &cShield );
               RELATIVIZE(gui.shield.rect);
            }
            if (xml_isNode(cur,"armour")) {
               gui_parseBar( cur, &gui.armour, &cArmour );
               RELATIVIZE(gui.armour.rect);
            }
            if (xml_isNode(cur,"energy")) {
               gui_parseBar( cur, &gui.energy, &cEnergy );
               RELATIVIZE(gui.energy.rect);
            }
            if (xml_isNode(cur,"fuel")) {
               gui_parseBar( cur, &gui.fuel, &cFuel );
               RELATIVIZE(gui.fuel.rect);
            }
         } while (xml_nextNode(cur));
      }

      /*
       * secondary weapon
       */
      else if (xml_isNode(node,"weapon")) {
         rect_parse( node, &gui.weapon.x, &gui.weapon.y,
               &gui.weapon.w, &gui.weapon.h );
         RELATIVIZE(gui.weapon);
         gui.weapon.y -= gl_defFont.h;
      }

      /*
       * target
       */
      else if (xml_isNode(node,"target")) {
         cur = node->children;
         do {
            if (xml_isNode(cur,"gfx")) {
               rect_parse( cur, &gui.target.x, &gui.target.y, NULL, NULL );
               RELATIVIZE(gui.target);
               gui.target.y -= SHIP_TARGET_H;
            }
            else if (xml_isNode(cur,"name")) {
               rect_parse( cur, &gui.target_name.x, &gui.target_name.y,
                     &gui.target_name.w, NULL );
               RELATIVIZE(gui.target_name);
               gui.target_name.y -= gl_defFont.h;
            }
            else if (xml_isNode(cur,"faction")) {
               rect_parse( cur, &gui.target_faction.x, &gui.target_faction.y,
                     &gui.target_faction.w, NULL );
               RELATIVIZE(gui.target_faction);
               gui.target_faction.y -= gl_smallFont.h;
            }
            else if (xml_isNode(cur,"health")) {
               rect_parse( cur, &gui.target_health.x, &gui.target_health.y,
                     &gui.target_health.w, NULL );
               RELATIVIZE(gui.target_health);
               gui.target_health.y -= gl_smallFont.h;
            }
         } while (xml_nextNode(cur));
      }

      /*
       * misc
       */
      else if (xml_isNode(node,"misc")) {
         rect_parse( node, &gui.misc.x, &gui.misc.y, &gui.misc.w, &gui.misc.h );
         RELATIVIZE(gui.misc);
      }
   } while (xml_nextNode(node));

   /* Some postprocessing. */
   gui_createInterference();

   return 0;
}
#undef RELATIVIZE


/**
 * @brief Cleans up a health bar.
 */
static void gui_cleanupBar( HealthBar *bar )
{
   if (bar->gfx != NULL) {
      gl_freeTexture( bar->gfx );
      bar->gfx = NULL;
   }
}


/**
 * @brief Cleans up the GUI.
 */
void gui_cleanup (void)
{
   int i;

   /* Free textures. */
   if (gui.gfx_frame != NULL) {
      gl_freeTexture( gui.gfx_frame );
      gui.gfx_frame = NULL;
   }
   if (gui.gfx_targetPilot != NULL) {
      gl_freeTexture( gui.gfx_targetPilot );
      gui.gfx_targetPilot = NULL;
   }
   if (gui.gfx_targetPlanet != NULL) {
      gl_freeTexture( gui.gfx_targetPlanet );
      gui.gfx_targetPlanet = NULL;
   }
   /* Health textures. */
   gui_cleanupBar( &gui.shield );
   gui_cleanupBar( &gui.armour );
   gui_cleanupBar( &gui.energy );
   gui_cleanupBar( &gui.fuel );
   /* Interference. */
   for (i=0; i<INTERFERENCE_LAYERS; i++) {
      if (gui.radar.interference[i] != NULL) {
         gl_freeTexture(gui.radar.interference[i]);
         gui.radar.interference[i] = NULL;
      }
   }

   /* Clean up interference. */ 
   interference_alpha = 0.; 
   interference_layer = 0; 
   interference_t     = 0.; 

   /* Destroy offset. */
   gui_xoff = 0.;
   gui_yoff = 0.;
}


/**
 * @brief Frees the gui stuff.
 */
void gui_free (void)
{
   /* Clean up gui. */
   gui_cleanup();

   /* Free messages. */
   if (mesg_stack != NULL) {
      free(mesg_stack);
      mesg_stack = NULL;
   }

   /* Free VBO. */
   if (gui_vbo != NULL) {
      gl_vboDestroy( gui_vbo );
      gui_vbo = NULL;
   }

   /* Clean up the osd. */
   osd_exit();
}


/**
 * @brief Modifies the radar resolution.
 *
 *    @param mod Number of intervals to jump (up or down).
 */
void gui_setRadarRel( int mod )
{
   gui.radar.res += mod * RADAR_RES_INTERVAL;
   gui.radar.res = CLAMP( RADAR_RES_MIN, RADAR_RES_MAX, gui.radar.res );

   player_message( "Radar set to %dx.", (int)gui.radar.res );
}


/**
 * @brief Gets the GUI offset.
 *
 *    @param x X offset.
 *    @param y Y offset.
 */
void gui_getOffset( double *x, double *y )
{
   *x = gui_xoff;
   *y = gui_yoff;
}
