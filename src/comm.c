/*
 * See Licensing and Copyright notice in naev.h
 */


/**
 * @file comm.c
 *
 * @brief For communicating with planets/pilots.
 */


#include "comm.h"

#include "naev.h"

#include "log.h"
#include "toolkit.h"
#include "dialogue.h"
#include "pilot.h"
#include "rng.h"
#include "nlua.h"
#include "player.h"
#include "opengl.h"


#define BUTTON_WIDTH    80 /**< Button width. */
#define BUTTON_HEIGHT   30 /**< Button height. */

#define GRAPHIC_WIDTH  256 /**< Width of graphic. */
#define GRAPHIC_HEIGHT 256 /**< Height of graphic. */


static Pilot *comm_pilot       = NULL; /**< Pilot currently talking to. */
static Planet *comm_planet     = NULL; /**< Planet currently talking to. */
static glTexture *comm_graphic = NULL; /**< Pilot's graphic. */


/* We need direct pilot access. */
extern Pilot** pilot_stack;
extern int pilot_nstack;


/*
 * Prototypes.
 */
/* Static. */
static unsigned int comm_open( glTexture *gfx, int faction,
      int override, int bribed, char *name );
static void comm_close( unsigned int wid, char *unused );
static void comm_bribePilot( unsigned int wid, char *unused );
static void comm_bribePlanet( unsigned int wid, char *unused );
static void comm_requestFuel( unsigned int wid, char *unused );
static int comm_getNumber( double *val, char* str );
static const char* comm_getString( char *str );
/* Extern. */
extern void ai_refuel( Pilot* refueler, unsigned int target ); /**< ai.c */
extern void ai_setPilot( Pilot *p ); /**< from ai.c */


/**
 * @brief Checks to see if comm is open.
 *
 *    @return 1 if comm is open.
 */
int comm_isOpen (void)
{
   return window_exists( "Communication Channel" );
}


/**
 * @brief Opens the communication dialogue with a pilot.
 *
 *    @param pilot Pilot to communicate with.
 *    @return 0 on success.
 */
int comm_openPilot( unsigned int pilot )
{
   const char *msg;
   unsigned int wid;

   /* Get the pilot. */
   comm_pilot = pilot_get( pilot );
   if (comm_pilot == NULL)
      return -1;
  
   /* Must not be disabled. */
   if (pilot_isFlag(comm_pilot, PILOT_DISABLED)) {
      player_message("%s does not respond.", comm_pilot->name);
      return 0;
   }

   /* Check to see if pilot wants to communicate. */
   msg = comm_getString( "comm_no" );   
   if (msg != NULL) {
      player_message( msg );
      return 0;
   }

   /* Set up for the comm_get* functions. */
   ai_setPilot( comm_pilot );

   /* Create the generic comm window. */
   wid = comm_open( ship_loadCommGFX( comm_pilot->ship ),
         comm_pilot->faction,
         pilot_isHostile(comm_pilot) ? -1 : pilot_isFriendly(comm_pilot) ? 1 : 0,
         pilot_isFlag(comm_pilot, PILOT_BRIBED),
         comm_pilot->name );

   /* Add special buttons. */
   window_addButton( wid, -20, 20 + BUTTON_HEIGHT + 20,
         BUTTON_WIDTH, BUTTON_HEIGHT, "btnGreet", "Greet", NULL );
   if (!pilot_isFlag(comm_pilot, PILOT_BRIBED) && /* Not already bribed. */
         ((faction_getPlayer( comm_pilot->faction ) < 0) || /* Hostile. */
            pilot_isHostile(comm_pilot)))
      window_addButton( wid, -20, 20 + 2*BUTTON_HEIGHT + 40,
            BUTTON_WIDTH, BUTTON_HEIGHT, "btnBribe", "Bribe", comm_bribePilot );
   else
      window_addButton( wid, -20, 20 + 2*BUTTON_HEIGHT + 40,
            BUTTON_WIDTH, BUTTON_HEIGHT, "btnRequest",
            "Refuel", comm_requestFuel );

   /* Run hooks if needed. */
   pilot_runHook( comm_pilot, PILOT_HOOK_HAIL );

   return 0;
}


/**
 * @brief Opens a communication dialogue with a planet.
 *
 *    @param planet Planet to communicate with.
 *    @return 0 on success.
 */
int comm_openPlanet( Planet *planet )
{
   unsigned int wid;

   /* Must not be disabled. */
   if (!planet_hasService(planet, PLANET_SERVICE_BASIC)) {
      player_message("%s does not respond.", planet->name);
      return 0;
   }

   comm_planet = planet;

   /* Create the generic comm window. */
   wid = comm_open( gl_dupTexture( comm_planet->gfx_space ),
         comm_planet->faction, 0, 0, comm_planet->name );

   /* Add special buttons. */
   if (areEnemies(player->faction, planet->faction) &&
         !planet->bribed)
      window_addButton( wid, -20, 20 + BUTTON_HEIGHT + 20,
            BUTTON_WIDTH, BUTTON_HEIGHT, "btnBribe", "Bribe", comm_bribePlanet );

   return 0;
}


/**
 * @brief Sets up the comm window.
 *
 *    @param gfx Graphic to use for the comm window (is freed).
 *    @param faction Faction of what you're communicating with.
 *    @param override If positive sets to ally, if negative sets to hostile.
 *    @param name Name of object talking to.
 *    @return The comm window id.
 */
static unsigned int comm_open( glTexture *gfx, int faction,
      int override, int bribed, char *name )
{
   int x,y, w;
   glTexture *logo;
   char *stand;
   unsigned int wid;
   glColour *c;

   /* Clean up. */
   if (comm_graphic != NULL) {
      /* First clean up if needed. */
      gl_freeTexture(comm_graphic);
      comm_graphic = NULL;
   }

   /* Get faction details. */
   comm_graphic   = gfx;
   logo           = faction_logoSmall(faction);

   /* Get standing colour / text. */
   if (bribed) {
      stand = "Neutral";
      c     = &cNeutral;
   }
   else if (override < 0) {
      stand = "Hostile";
      c     = &cHostile;
   }
   else if (override > 0) {
      stand = "Friendly";
      c     = &cFriend;
   }
   else {
      stand = faction_getStandingBroad(faction_getPlayer( faction ));
      c     = faction_getColour( faction );
   }
   w = MAX(gl_printWidth( NULL, name ), gl_printWidth( NULL, stand ));
   y = gl_defFont.h*2 + 15;
   if (logo != NULL) {
      w += logo->w;
      y = MAX( y, logo->w );
   }
   x = (GRAPHIC_WIDTH - w) / 2;

   /* Create the window. */
   wid = window_create( "Communication Channel", -1, -1,
         20 + GRAPHIC_WIDTH + 20 + BUTTON_WIDTH + 20,
         30 + GRAPHIC_HEIGHT + y + 5 + 20 );

   /* Create the ship image. */
   window_addRect( wid, 20, -30, GRAPHIC_WIDTH, GRAPHIC_HEIGHT + y + 5,
         "rctGFX", &cGrey10, 1 );
   window_addImage( wid, 20 + (GRAPHIC_WIDTH-comm_graphic->w)/2,
         -30 - (GRAPHIC_HEIGHT-comm_graphic->h)/2,
         "imgGFX", comm_graphic, 0 );

   /* Faction logo. */
   if (logo != NULL) {
      window_addImage( wid, x, -30 - GRAPHIC_HEIGHT - 5,
            "imgFaction", logo, 0 );
      x += logo->w + 10;
      y -= (logo->w - (gl_defFont.h*2 + 15)) / 2;
   }
   
   /* Name. */
   window_addText( wid, x, -30 - GRAPHIC_HEIGHT - y + gl_defFont.h*2 + 10,
         GRAPHIC_WIDTH - x, 20, 0, "txtName",
         NULL, &cDConsole, name );

   /* Standing. */
   window_addText( wid, x, -30 - GRAPHIC_HEIGHT - y + gl_defFont.h + 5,
         GRAPHIC_WIDTH - x, 20, 0, "txtStanding", NULL, c, stand );

   /* Buttons. */
   window_addButton( wid, -20, 20, BUTTON_WIDTH, BUTTON_HEIGHT,
         "btnClose", "Close", comm_close );

   return wid;
}


/**
 * @brief Closes the comm window.
 *
 *    @param wid ID of window calling the function.
 *    @param unused Unused.
 */
static void comm_close( unsigned int wid, char *unused )
{
   /* Clean up a bit after ourselves. */
   if (comm_graphic != NULL) {
      gl_freeTexture(comm_graphic);
      comm_graphic = NULL;
   }
   comm_pilot  = NULL;
   comm_planet = NULL;
   /* Close the window. */
   window_close( wid, unused );
}


/**
 * @brief Tries to bribe the pilot.
 *
 *    @param wid ID of window calling the function.
 *    @param unused Unused.
 */
static void comm_bribePilot( unsigned int wid, char *unused )
{
   (void) unused;
   int answer;
   double d;
   unsigned int price;
   const char *str;
   lua_State *L;

   /* Unbribeable. */
   str = comm_getString( "bribe_no" );
   if (str != NULL) {
      dialogue_msg("Bribe Pilot", "%s", str );
      return;
   }

   /* Get amount pilot wants. */
   if (comm_getNumber( &d, "bribe" )) {
      WARN("Pilot '%s' accepts bribes but doesn't give price!", comm_pilot->name );
      d = 0.;
   }
   price = (unsigned int) d;

   /* Check to see if already bribed. */
   if (price == 0) {
      dialogue_msg("Bribe Pilot", "\"Money won't save your hide now!\"");
      return;
   }

   /* Bribe message. */
   str = comm_getString( "bribe_prompt" );
   if (str == NULL) {
      answer = dialogue_YesNo( "Bribe Pilot", "\"I'm gonna need at least %u credits to not leave you as a hunk of floating debris.\"\n\nPay %u credits?", price, price );
   }
   else
      answer = dialogue_YesNo( "Bribe Pilot", "%s\n\nPay %u credits?", str, price );

   /* Said no. */
   if (answer == 0) {
      dialogue_msg("Bribe Pilot", "You decide not to pay.");
      return;
   }

   /* Check if has the money. */
   if (player->credits < price) {
      dialogue_msg("Bribe Pilot", "You don't have enough credits for the bribery.");
      return;
   }

   player->credits -= price;
   str = comm_getString( "bribe_paid" );
   if (str == NULL)
      dialogue_msg("Bribe Pilot", "\"Pleasure to do business with you.\"");
   else
      dialogue_msg("Bribe Pilot", "%s", str);

   /* Mark as bribed and don't allow bribing again. */
   pilot_setFlag( comm_pilot, PILOT_BRIBED );
   pilot_rmHostile( comm_pilot );
   L = comm_pilot->ai->L;
   lua_getglobal(L, "mem");
   lua_pushnumber(L, 0);
   lua_setfield(L, -2, "bribe");
   lua_pop(L,1);

   /* Reopen window. */
   window_destroy( wid );
   comm_openPilot( comm_pilot->id );
}


/**
 * @brief Tries to bribe the planet
 *
 *    @param wid ID of window calling the function.
 *    @param unused Unused.
 */
static void comm_bribePlanet( unsigned int wid, char *unused )
{
   (void) unused;
   int i, j;
   int answer;
   unsigned int price;
   double d;
   double n, m;
   double o, p;
   double q, r;
   double standing;
   Fleet *f;
   
   /* Price. */
   standing = faction_getPlayer( comm_planet->faction );
   /* Get number of hostiles and mass of hostiles. */
   n = 0.;
   m = 0.;
   for (i=0; i<pilot_nstack; i++) {
      if (areAllies(comm_planet->faction, pilot_stack[i]->faction)) {
         n += 1.;
         m += pilot_stack[i]->solid->mass;
      }
   }
   /* Get now the presence factor - get mass of possible ships and mass */
   o = 0.;
   p = 0.;
   for (i=0; i<cur_system->nfleets; i++) {
      f = cur_system->fleets[i].fleet;
      if (areAllies(comm_planet->faction, f->faction)) {
         q = 0;
         r = 0;
         for (j=0; j<f->npilots; j++) {
            q += (double)f->pilots[j].chance / 100.;
            r += f->pilots[j].ship->mass;
         }
         q *= (double)cur_system->fleets[i].chance / 100.;
         o += q;
         p += r;
      }
   }
   /* Calculate the price. */
   n = MAX(0., 1.);
   o = MAX(0., 1.);
   d  = 2000.; /* Base price. */
   d *= 0.5 * (o * (sqrt( p / o ) / 9.5)) + /* Base on presence. */
        0.5 * (n * (sqrt( m / n ) / 9.5)); /* Base on current hostiles. */
   d *= 1. + (-1. * standing) / 50.; /* Modify by standing. */
   price = (unsigned int) d;

   /* Yes/No input. */
   answer = dialogue_YesNo( "Bribe Starport",
         "\"I'll let you land for the small sum of %u credits.\"\n\nPay %u credits?",
         price,  price );

   /* Said no. */
   if (answer == 0) {
      dialogue_msg("Bribe Starport", "You decide not to pay.");
      return;
   }

   /* Check if has the money. */
   if (player->credits < price) {
      dialogue_msg("Bribe Starport", "You don't have enough credits for the bribery.");
      return;
   }

   /* Pay the money. */
   player->credits -= price;
   dialogue_msg("Bribe Starport", "You have permission to dock.");

   /* Mark as bribed and don't allow bribing again. */
   comm_planet->bribed = 1;

   /* Reopen window. */
   window_destroy( wid );
   comm_openPlanet( comm_planet );
}


/**
 * @brief Tries to request help from the pilot.
 *
 *    @param wid ID of window calling the function.
 *    @param unused Unused.
 */
static void comm_requestFuel( unsigned int wid, char *unused )
{
   (void) wid;
   (void) unused;
   double val;
   const char *msg;
   int ret;
   unsigned int price;

   /* Check to see if ship has a no refuel message. */
   msg = comm_getString( "refuel_no" );
   if (msg != NULL) {
      dialogue_msg( "Request Fuel", msg );
      return;
   }

   /* Must need refueling. */
   if (player->fuel >= player->fuel_max) {
      dialogue_msg( "Request Fuel", "Your fuel deposits are already full." );
      return;
   }

   /* See if pilot has enough fuel. */
   if (comm_pilot->fuel < 200.) {
      dialogue_msg( "Request Fuel",
            "\"Sorry, I don't have enough fuel to spare at the moment.\"" );
      return;
   }

   /* See if player can get refueled. */
   ret = comm_getNumber( &val, "refuel" );
   msg = comm_getString( "refuel_msg" );
   if ((ret != 0) || (msg == NULL)) {
      dialogue_msg( "Request Fuel", "\"Sorry, I'm busy now.\"" );
      return;
   }
   price = (int) val;

   /* Check to see if is already refueling. */
   if (pilot_isFlag(comm_pilot, PILOT_REFUELING)) {
      dialogue_msg( "Request Fuel", "Pilot is already refueling you." );
      return;
   }

   /* See if player really wants to pay. */
   if (price > 0) {
      ret = dialogue_YesNo( "Request Fuel", "%s\n\nPay %u credits?", msg, price );
      if (ret == 0) {
         dialogue_msg( "Request Fuel", "You decide not to pay." );
         return;
      }
   }
   else
      dialogue_msg( "Request Fuel", "%s", msg );

   /* Check if he has the money. */
   if (player->credits < price) {
      dialogue_msg( "Request Fuel", "You need %u more credits!",
            price - player->credits);
      return;
   }

   /* Take money. */
   player->credits      -= price;
   comm_pilot->credits  += price;

   /* Start refueling. */
   pilot_setFlag(comm_pilot, PILOT_REFUELING);
   ai_refuel( comm_pilot, player->id );

   /* Last message. */
   if (price > 0)
      dialogue_msg( "Request Fuel", "\"On my way.\"" );
}


/**
 * @brief Gets the amount the communicating pilot wants as a bribe.
 *
 * Valid targets for now are:
 *    - "bribe": amount pilot wants to be paid. 
 *    - "refuel": amount pilot wants to be paid for refueling the player.
 *
 *    @param[out] val Value of the number gotten.
 *    @param str Name of number to get.
 *    @return 0 for success, 1 on error (including not found).
 */
static int comm_getNumber( double *val, char* str )
{
   int ret;
   lua_State *L;

   /* Set up the state. */
   L = comm_pilot->ai->L;
   lua_getglobal( L, "mem" );

   /* Get number amount. */
   lua_getfield( L, -1, str );
   /* Check to see if it's a number. */
   if (!lua_isnumber(L, -1))
      ret = 1;
   else {
      *val = lua_tonumber(L, -1);
      ret = 0;
   }
   /* Clean up. */
   lua_pop(L, 2);
   return ret;
}


/**
 * @brief Gets a string from the pilot's memory.
 *
 * Valid targets are:
 *    - comm_no: message of communication failure.
 *    - bribe_no: unbribe message
 *    - bribe_prompt: bribe prompt
 *    - bribe_paid: paid message
 *
 *    @param str String to get.
 *    @return String matching str.
 */
static const char* comm_getString( char *str )
{
   lua_State *L;
   const char *ret;

   /* Get memory table. */
   L = comm_pilot->ai->L;
   lua_getglobal( L, "mem" );

   /* Get str message. */
   lua_getfield(L, -1, str );
   if (!lua_isstring(L, -1))
      ret = NULL;
   else
      ret = lua_tostring(L, -1);
   lua_pop(L, 2);

   return ret;
}

