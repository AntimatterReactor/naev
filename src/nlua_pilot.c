/*
 * See Licensing and Copyright notice in naev.h
 */

/**
 * @file nlua_space.c
 *
 * @brief Handles the Lua space bindings.
 *
 * These bindings control the planets and systems.
 */

#include "nlua_pilot.h"

#include "naev.h"

#include "lauxlib.h"

#include "nlua.h"
#include "nluadef.h"
#include "nlua_faction.h"
#include "nlua_vec2.h"
#include "nlua_ship.h"
#include "log.h"
#include "rng.h"
#include "pilot.h"
#include "player.h"
#include "space.h"


/*
 * From pilot.c
 */
extern Pilot** pilot_stack;
extern int pilot_nstack;


/*
 * Prototypes.
 */
/* External. */
extern int ai_pinit( Pilot *p, const char *ai );
extern void ai_destroy( Pilot* p );


/* Pilot metatable methods. */
static int pilotL_getPlayer( lua_State *L );
static int pilotL_addFleet( lua_State *L );
static int pilotL_clear( lua_State *L );
static int pilotL_toggleSpawn( lua_State *L );
static int pilotL_getPilots( lua_State *L );
static int pilotL_eq( lua_State *L );
static int pilotL_name( lua_State *L );
static int pilotL_id( lua_State *L );
static int pilotL_alive( lua_State *L );
static int pilotL_rename( lua_State *L );
static int pilotL_position( lua_State *L );
static int pilotL_velocity( lua_State *L );
static int pilotL_setPosition( lua_State *L );
static int pilotL_setVelocity( lua_State *L );
static int pilotL_broadcast( lua_State *L );
static int pilotL_comm( lua_State *L );
static int pilotL_setFaction( lua_State *L );
static int pilotL_setHostile( lua_State *L );
static int pilotL_setFriendly( lua_State *L );
static int pilotL_setInvincible( lua_State *L );
static int pilotL_disable( lua_State *L );
static int pilotL_addOutfit( lua_State *L );
static int pilotL_rmOutfit( lua_State *L );
static int pilotL_setFuel( lua_State *L );
static int pilotL_changeAI( lua_State *L );
static int pilotL_setHealth( lua_State *L );
static int pilotL_setNoboard( lua_State *L );
static int pilotL_getHealth( lua_State *L );
static int pilotL_shipName( lua_State *L );
static int pilotL_ship( lua_State *L );
static const luaL_reg pilotL_methods[] = {
   /* General. */
   { "player", pilotL_getPlayer },
   { "add", pilotL_addFleet },
   { "get", pilotL_getPilots },
   { "__eq", pilotL_eq },
   /* Info. */
   { "name", pilotL_name },
   { "id", pilotL_id },
   { "alive", pilotL_alive },
   { "rename", pilotL_rename },
   { "pos", pilotL_position },
   { "vel", pilotL_velocity },
   /* System. */
   { "clear", pilotL_clear },
   { "toggleSpawn", pilotL_toggleSpawn },
   /* Modify. */
   { "changeAI", pilotL_changeAI },
   { "setHealth", pilotL_setHealth },
   { "setNoboard", pilotL_setNoboard },
   { "getHealth", pilotL_getHealth },
   { "setPos", pilotL_setPosition },
   { "setVel", pilotL_setVelocity },
   { "setFaction", pilotL_setFaction },
   { "setHostile", pilotL_setHostile },
   { "setFriendly", pilotL_setFriendly },
   { "setInvincible", pilotL_setInvincible },
   { "disable", pilotL_disable },
   /* Talk. */
   { "broadcast", pilotL_broadcast },
   { "comm", pilotL_comm },
   /* Outfits. */
   { "addOutfit", pilotL_addOutfit },
   { "rmOutfit", pilotL_rmOutfit },
   { "setFuel", pilotL_setFuel },
   /* Ship. */
   { "shipName", pilotL_shipName },
   { "ship", pilotL_ship },
   {0,0}
}; /**< Pilot metatable methods. */




/**
 * @brief Loads the space library.
 *
 *    @param L State to load space library into.
 *    @return 0 on success.
 */
int nlua_loadPilot( lua_State *L, int readonly )
{
   if (readonly) /* Nothing is read only */
      return 0;

   /* Create the metatable */
   luaL_newmetatable(L, PILOT_METATABLE);

   /* Create the access table */
   lua_pushvalue(L,-1);
   lua_setfield(L,-2,"__index");

   /* Register the values */
   luaL_register(L, NULL, pilotL_methods);

   /* Clean up. */
   lua_setfield(L, LUA_GLOBALSINDEX, PILOT_METATABLE);

   /* Pilot always loads ship. */
   nlua_loadShip(L,readonly);

   return 0;
}


/**
 * @brief Lua bindings to interact with pilots.
 *
 * This will allow you to create and manipulate pilots in-game.
 *
 * An example would be:
 * @code
 * p = pilot.add( "Sml Trader Convoy" ) -- Create a trader convoy
 * for k,v in pairs(p) do
 *    v:setFriendly() -- Make it friendly
 * end
 * @endcode
 *
 * @luamod pilot
 */
/**
 * @brief Gets pilot at index.
 *
 *    @param L Lua state to get pilot from.
 *    @param ind Index position to find the pilot.
 *    @return Pilot found at the index in the state.
 */
LuaPilot* lua_topilot( lua_State *L, int ind )
{
   return (LuaPilot*) lua_touserdata(L,ind);
}
/**
 * @brief Gets pilot at index or raises error if there is no pilot at index.
 *
 *    @param L Lua state to get pilot from.
 *    @param ind Index position to find pilot.
 *    @return Pilot found at the index in the state.
 */
LuaPilot* luaL_checkpilot( lua_State *L, int ind )
{
   if (lua_ispilot(L,ind))
      return lua_topilot(L,ind);
   luaL_typerror(L, ind, PILOT_METATABLE);
   return NULL;
}
/**
 * @brief Pushes a pilot on the stack.
 *
 *    @param L Lua state to push pilot into.
 *    @param pilot Pilot to push.
 *    @return Newly pushed pilot.
 */
LuaPilot* lua_pushpilot( lua_State *L, LuaPilot pilot )
{
   LuaPilot *p;
   p = (LuaPilot*) lua_newuserdata(L, sizeof(LuaPilot));
   *p = pilot;
   luaL_getmetatable(L, PILOT_METATABLE);
   lua_setmetatable(L, -2);
   return p;
}
/**
 * @brief Checks to see if ind is a pilot.
 *
 *    @param L Lua state to check.
 *    @param ind Index position to check.
 *    @return 1 if ind is a pilot.
 */
int lua_ispilot( lua_State *L, int ind )
{
   int ret;

   if (lua_getmetatable(L,ind)==0)
      return 0;
   lua_getfield(L, LUA_REGISTRYINDEX, PILOT_METATABLE);

   ret = 0;
   if (lua_rawequal(L, -1, -2))  /* does it have the correct mt? */ 
      ret = 1;

   lua_pop(L, 2);  /* remove both metatables */ 
   return ret;
}

/**
 * @brief Gets the player's pilot.
 *
 * @usage player = pilot.player()
 *
 *    @luareturn Pilot pointing to the player.
 * @luafunc player()
 */
static int pilotL_getPlayer( lua_State *L )
{
   LuaPilot lp;

   if (player == NULL) {
      lua_pushnil(L);
      return 1;
   }

   lp.pilot = player->id;
   lua_pushpilot(L,lp);
   return 1;
}

/**
 * @brief Adds a fleet to the system.
 *
 * You can then iterate over the pilots to change parameters like so:
 * @code
 * p = pilot.add( "Sml Trader Convoy" )
 * for k,v in pairs(p) do
 *    v:setHostile()
 * end
 * @endcode
 *
 * @usage p = pilot.add( "Pirate Hyena" ) -- Just adds the pilot (will jump in).
 * @usage p = pilot.add( "Trader Llama", "dummy" ) -- Overrides AI with dummy ai.
 * @usage p = pilot.add( "Sml Trader Convoy", "def", vec2.new( 1000, 200 ) ) -- Pilot won't jump in, will just appear.
 * @usage p = pilot.add( "Empire Pacifier", "def", vec2.new( 1000, 1000 ), true ) -- Have the pilot jump in.
 *
 *    @luaparam fleetname Name of the fleet to add.
 *    @luaparam ai If set will override the standard fleet AI.  "def" means use default.
 *    @luaparam pos Position to create pilots around instead of choosing randomly.
 *    @luaparam jump true if pilots should jump in, false by default if pos is defined.
 *    @luareturn Table populated with all the pilots created.  The keys are ordered numbers.
 * @luafunc add( fleetname, ai, pos, jump )
 */
static int pilotL_addFleet( lua_State *L )
{
   Fleet *flt;
   const char *fltname, *fltai;
   int i, j, first;
   unsigned int p;
   double a;
   double d;
   Vector2d vv,vp, vn;
   FleetPilot *plt;
   LuaPilot lp;
   LuaVector *lv;
   int jump;
   unsigned int flags = 0;

   /* Default values. */
   flags = 0;
   vectnull(&vn); /* Need to determine angle. */

   /* Parse first argument - Fleet Name */
   fltname = luaL_checkstring(L,1);

   /* Parse second argument - Fleet AI Override */
   if (lua_gettop(L) > 1) {
      fltai = luaL_checkstring(L,2);
      if (strcmp(fltai, "def")==0) /* Check if set to default */
         fltai = NULL;
   }
   else
      fltai = NULL;

   /* Parse third argument - Position */
   if (lua_gettop(L) > 2) {
      lv = luaL_checkvector(L,3);
   }
   else
      lv = NULL;

   /* Parse Jump. */
   if (lua_gettop(L) > 3) {
      jump = lua_toboolean(L,4);
   }
   else {
      /* Only jump by default if not position was passed. */
      if (lv==NULL)
         jump = 1;
      else
         jump = 0;
   }

   /* pull the fleet */
   flt = fleet_get( fltname );
   if (flt == NULL) {
      NLUA_ERROR(L,"Fleet '%s' doesn't exist.", fltname);
      return 0;
   }

   /* Use position passed if possible. */
   if (lv != NULL) {
      if (!jump)
         vectcpy( &vp, &lv->vec );
      else {
         /* Pilot is jumping in, we'll only use the vector angle. */
         d = RNGF()*(HYPERSPACE_ENTER_MAX-HYPERSPACE_ENTER_MIN) + HYPERSPACE_ENTER_MIN;
         vect_pset( &vp, d, VANGLE(lv->vec) );
      }
   }
   else {
      d = RNGF()*(HYPERSPACE_ENTER_MAX-HYPERSPACE_ENTER_MIN) + HYPERSPACE_ENTER_MIN;
      vect_pset( &vp, d, RNGF() * 2.*M_PI);
   }

   /* Set velocity only if no position is set.. */
   if (lv != NULL) {
      if (jump) {
         a = vect_angle(&vp,&vn);
         vect_pset( &vv, HYPERSPACE_VEL, a );
         flags |= PILOT_HYP_END;
      }
      else {
         a = RNGF() * 2.*M_PI;
         vectnull( &vv );
      }
   }
   else { /* Entering via hyperspace. */
      a = vect_angle(&vp,&vn);
      vect_pset( &vv, HYPERSPACE_VEL, a );
      flags |= PILOT_HYP_END;
   }

   /* Make sure angle is sane. */
   a = fmod( a, 2.*M_PI );
   if (a < 0.)
      a += 2.*M_PI;

   /* now we start adding pilots and toss ids into the table we return */
   first = 1;
   j     = 0;
   lua_newtable(L);
   for (i=0; i<flt->npilots; i++) {

      plt = &flt->pilots[i];

      if (RNG(0,100) > plt->chance)
         continue;

      /* Fleet displacement - first ship is exact. */
      if (!first)
         vect_cadd(&vp, RNG(75,150) * (RNG(0,1) ? 1 : -1),
               RNG(75,150) * (RNG(0,1) ? 1 : -1));
      first = 0;

      /* Create the pilot. */
      p = fleet_createPilot( flt, plt, a, &vp, &vv, fltai, flags );

      /* we push each pilot created into a table and return it */
      lua_pushnumber(L,++j); /* index, starts with 1 */
      lp.pilot = p;
      lua_pushpilot(L,lp); /* value = LuaPilot */
      lua_rawset(L,-3); /* store the value in the table */
   }
   return 1;
}
/**
 * @brief Clears the current system of pilots.  Used for epic battles and such.
 *
 * @usage pilot.clear()
 * 
 * @luafunc clear()
 */
static int pilotL_clear( lua_State *L )
{
   (void) L;
   pilots_clean();
   return 0;
}
/**
 * @brief Disables or enables pilot spawning in the current system.
 *
 * If player jumps the spawn is enabled again automatically.
 *
 * @usage pilot.togglespawn( false )
 *
 *    @luaparam enable true enables spawn, false disables it.
 *    @luareturn The current spawn state.
 * @luafunc toggleSpawn( enable )
 */
static int pilotL_toggleSpawn( lua_State *L )
{
   /* Setting it directly. */
   if ((lua_gettop(L) > 0) && lua_isboolean(L,1))
      space_spawn = lua_toboolean(L,1);
   /* Toggling. */
   else
      space_spawn = !space_spawn;

   lua_pushboolean(L, space_spawn);
   return 1;
}
/**
 * @brief Gets the pilots available in the system by a certain criteria.
 *
 * @usage p = pilot.get() -- Gets all the pilots
 * @usage p = pilot.get( { faction.get("Empire") } ) -- Only gets empire pilots.
 *
 *    @luaparam f If f is a table of factions, it will only get pilots matching those factions.  Otherwise it gets all the pilots.
 *    @luareturn A table containing the pilots.
 * @luafunc get( f )
 */
static int pilotL_getPilots( lua_State *L )
{
   int i, j, k;
   int *factions;
   int nfactions;
   LuaFaction *f;
   LuaPilot p;

   /* Check for belonging to faction. */
   if (lua_istable(L,1)) {
      /* Get table length and preallocate. */
      nfactions = (int) lua_objlen(L,1);
      factions = malloc( sizeof(int) * nfactions );
      /* Load up the table. */
      lua_pushnil(L);
      i = 0;
      while (lua_next(L, -2) != 0) {
         f = lua_tofaction(L, -1);
         factions[i++] = f->f;
         lua_pop(L,1);
      }

      /* Now put all the matching pilots in a table. */
      lua_newtable(L);
      k = 1;
      for (i=0; i<pilot_nstack; i++) {
         for (j=0; j<nfactions; j++) {
            if ((pilot_stack[i]->faction == factions[j]) &&
                  !pilot_isDisabled(pilot_stack[i]) &&
                  !pilot_isFlag(pilot_stack[i], PILOT_DELETE)) {
               lua_pushnumber(L, k++); /* key */
               p.pilot = pilot_stack[i]->id;
               lua_pushpilot(L, p); /* value */
               lua_rawset(L,-3); /* table[key] = value */
               break; /* Continue to next pilot. */
            }
         }
      }

      /* clean up. */
      free(factions);
   }
   else {
      /* Now put all the matching pilots in a table. */
      lua_newtable(L);
      k = 1;
      for (i=0; i<pilot_nstack; i++) {
         if (!pilot_isDisabled(pilot_stack[i]) &&
               !pilot_isFlag(pilot_stack[i], PILOT_DELETE)) {
            lua_pushnumber(L, k++); /* key */
            p.pilot = pilot_stack[i]->id;
            lua_pushpilot(L, p); /* value */
            lua_rawset(L,-3); /* table[key] = value */
            break; /* Continue to next pilot. */
         }
      }
   }

   return 1;
}

/**
 * @brief Checks to see if pilot and p are the same.
 *
 * @usage if p == p2 then -- Pilot 'p' and 'p2' match.
 *
 *    @luaparam p Pilot to compare.
 *    @luaparam comp Pilot to compare against.
 *    @luareturn true if they are the same.
 * @luafunc __eq( p, comp )
 */
static int pilotL_eq( lua_State *L )
{
   LuaPilot *p1, *p2;

   /* Get parameters. */
   p1 = luaL_checkpilot(L,1);
   p2 = luaL_checkpilot(L,2);

   /* Push result. */
   lua_pushboolean(L, p1->pilot == p2->pilot);
   return 1;
}

/**
 * @brief Gets the pilot's current name.
 *
 * @usage name = p:name()
 *
 *    @luaparam p Pilot to get the name of.
 *    @luareturn The current name of the pilot.
 * @luafunc name( p )
 */
static int pilotL_name( lua_State *L )
{
   LuaPilot *p1;
   Pilot *p;

   /* Parse parameters. */
   p1 = luaL_checkpilot(L,1);
   p  = pilot_get( p1->pilot );

   /* Pilot must exist. */
   if (p == NULL) {
      NLUA_ERROR(L,"Pilot is invalid.");
      return 0;
   }

   /* Get name. */
   lua_pushstring(L, p->name);
   return 1;
}

/**
 * @brief Gets the ID of the pilot.
 *
 * @usage id = p:id()
 *
 *    @luaparam p Pilot to get the ID of.
 *    @luareturn The ID of the curent pilot.
 * @luafunc id( p )
 */
static int pilotL_id( lua_State *L )
{
   LuaPilot *p1;
   Pilot *p;

   /* Parse parameters. */
   p1 = luaL_checkpilot(L,1);
   p  = pilot_get( p1->pilot );

   /* Pilot must exist. */
   if (p == NULL) {
      NLUA_ERROR(L,"Pilot is invalid.");
      return 0;
   }

   /* Get name. */
   lua_pushnumber(L, p->id);
   return 1;
}

/**
 * @brief Checks to see if pilot is still alive.
 *
 * @usage if p:alive() then -- Pilot is still alive
 *
 *    @luaparam p Pilot to check to see if is still alive.
 *    @luareturn true if pilot is still alive.
 * @luafunc alive( p )
 */
static int pilotL_alive( lua_State *L )
{
   LuaPilot *lp;
   Pilot *p;

   /* Parse parameters. */
   lp = luaL_checkpilot(L,1);
   p  = pilot_get( lp->pilot );

   /* Check if is alive. */
   lua_pushboolean(L, p!=NULL);
   return 1;
}

/**
 * @brief Changes the pilot's name.
 *
 * @usage p:rename( "Black Beard" )
 *
 *    @luaparam p Pilot to change name of.
 *    @luaparam name Name to change to.
 * @luafunc rename( p, name )
 */
static int pilotL_rename( lua_State *L )
{
   LuaPilot *p1;
   const char *name;
   Pilot *p;

   /* Parse parameters */
   p1    = luaL_checkpilot(L,1);
   p     = pilot_get( p1->pilot );
   name  = luaL_checkstring(L,2);

   /* Pilot must exist. */
   if (p == NULL) {
      NLUA_ERROR(L,"Pilot is invalid.");
      return 0;
   }

   /* Change name. */
   if (p->name != NULL)
      free(p->name);
   p->name = strdup(name);

   return 0;
}

/**
 * @brief Gets the pilot's position.
 *
 * @usage v = p:pos()
 *
 *    @luaparam p Pilot to get the position of.
 *    @luareturn The pilot's current position as a vec2.
 * @luafunc pos( p )
 */
static int pilotL_position( lua_State *L )
{
   LuaPilot *p1;
   Pilot *p;
   LuaVector v;

   /* Parse parameters */
   p1 = luaL_checkpilot(L,1);
   p  = pilot_get( p1->pilot );

   /* Pilot must exist. */
   if (p == NULL) {
      NLUA_ERROR(L,"Pilot is invalid.");
      return 0;
   }

   /* Push position. */
   vectcpy( &v.vec, &p->solid->pos );
   lua_pushvector(L, v);
   return 1;
}

/**
 * @brief Gets the pilot's velocity.
 *
 * @usage vel = p:vel()
 *
 *    @luaparam p Pilot to get the velocity of.
 *    @luareturn The pilot's current velocity as a vec2.
 * @luafunc vel( p )
 */
static int pilotL_velocity( lua_State *L )
{
   LuaPilot *p1;
   Pilot *p;
   LuaVector v;

   /* Parse parameters */
   p1 = luaL_checkpilot(L,1);
   p  = pilot_get( p1->pilot );

   /* Pilot must exist. */
   if (p == NULL) {
      NLUA_ERROR(L,"Pilot is invalid.");
      return 0;
   }

   /* Push velocity. */
   vectcpy( &v.vec, &p->solid->vel );
   lua_pushvector(L, v);
   return 1;
}

/**
 * @brief Sets the pilot's position.
 *
 * @usage p:setPos( vec2.new( 300, 200 ) )
 *
 *    @luaparam p Pilot to set the position of.
 *    @luaparam pos Position to set.
 * @luafunc setPos( p, pos )
 */
static int pilotL_setPosition( lua_State *L )
{
   LuaPilot *p1;
   Pilot *p;
   LuaVector *v;

   /* Parse parameters */
   p1 = luaL_checkpilot(L,1);
   p  = pilot_get( p1->pilot );
   v  = luaL_checkvector(L,2);

   /* Pilot must exist. */
   if (p == NULL) {
      NLUA_ERROR(L,"Pilot is invalid.");
      return 0;
   }

   /* Warp pilot to new position. */
   vectcpy( &p->solid->pos, &v->vec );
   return 0;
}

/**
 * @brief Sets the pilot's velocity.
 *
 * @usage p:setVel( vec2.new( 300, 200 ) )
 *
 *    @luaparam p Pilot to set the velocity of.
 *    @luaparam vel Velocity to set.
 * @luafunc setVel( p, vel )
 */
static int pilotL_setVelocity( lua_State *L )
{
   LuaPilot *p1;
   Pilot *p;
   LuaVector *v;

   /* Parse parameters */
   p1 = luaL_checkpilot(L,1);
   p  = pilot_get( p1->pilot );
   v  = luaL_checkvector(L,2);

   /* Pilot must exist. */
   if (p == NULL) {
      NLUA_ERROR(L,"Pilot is invalid.");
      return 0;
   }

   /* Warp pilot to new position. */
   vectcpy( &p->solid->vel, &v->vec );
   return 0;
}

/**
 * @brief Makes the pilot broadcast a message.
 *
 * @usage p:broadcast( "Mayday! Requesting assistance!" )
 * @usage p:broadcast( "Help!", true ) -- Will ignore interference
 *
 *    @luaparam p Pilot to broadcast the message.
 *    @luaparam msg Message to broadcast.
 *    @luaparam ignore_int Whether or not it should ignore interference.
 * @luafunc broadcast( p, msg, ignore_int )
 */
static int pilotL_broadcast( lua_State *L )
{
   Pilot *p;
   LuaPilot *lp;
   const char *msg;
   int ignore_int;

   /* Parse parameters. */
   lp    = luaL_checkpilot(L,1);
   msg   = luaL_checkstring(L,2);
   ignore_int = lua_toboolean(L,3);

   /* Check to see if pilot is valid. */
   p = pilot_get(lp->pilot);
   if (p == NULL) {
      NLUA_ERROR(L,"Pilot is invalid.");
      return 0;
   }

   /* Broadcast message. */
   pilot_broadcast( p, msg, ignore_int );
   return 0;
}

/**
 * @brief Sends a message to the target or player if no target is passed.
 *
 * @usage p:comm( "How are you doing?" ) -- Messages the player
 * @usage p:comm( "You got this?", true ) -- Messages the player ignoring interference
 * @usage p:comm( target, "Heya!" ) -- Messages target
 * @usage p:comm( target, "Got this?", true ) -- Messages target ignoring interference
 *
 *    @luaparam p Pilot to message the player.
 *    @ulaparam target Target to send message to.
 *    @luaparam msg Message to send.
 * @luafunc comm( p, target, msg )
 */
static int pilotL_comm( lua_State *L )
{
   Pilot *p, *t;
   LuaPilot *lp, *target;
   const char *msg;
   int ignore_int;

   /* Parse parameters. */
   lp    = luaL_checkpilot(L,1);
   if (lua_isstring(L,2)) {
      target = NULL;
      msg   = luaL_checkstring(L,2);
      ignore_int = lua_toboolean(L,3);
   }
   else {
      target = luaL_checkpilot(L,2);
      msg   = luaL_checkstring(L,3);
      ignore_int = lua_toboolean(L,4);
   }

   /* Check to see if pilot is valid. */
   p = pilot_get(lp->pilot);
   if (p == NULL) {
      NLUA_ERROR(L,"Pilot param 1 not found in pilot stack!");
      return 0;
   }
   if (target == NULL)
      t = player;
   else {
      t = pilot_get(target->pilot);
      if (t == NULL) {
         NLUA_ERROR(L,"Pilot param 2 not found in pilot stack!");
         return 0;
      }
   }

   /* Broadcast message. */
   pilot_message( p, t->id, msg, ignore_int );
   return 0;
}

/**
 * @brief Sets the pilot's faction.
 *
 * @usage p:setFaction( "Empire" )
 * @usage p:setFaction( faction.get( "Dvaered" ) )
 *
 *    @luaparam p Pilot to change faction of.
 *    @luaparam faction Faction to set by name or faction.
 * @luafunc setFaction( p, faction )
 */
static int pilotL_setFaction( lua_State *L )
{
   Pilot *p;
   LuaPilot *lp;
   LuaFaction *f;
   int fid;
   const char *faction;

   /* Parse parameters. */
   lp = luaL_checkpilot(L,1);
   if (lua_isstring(L,2)) {
      faction = lua_tostring(L,2);
      fid = faction_get(faction);
   }
   else if (lua_isfaction(L,2)) {
      f = lua_tofaction(L,2);
      fid = f->f;
   }
   else NLUA_INVALID_PARAMETER();

   /* Get pilot/faction. */
   p = pilot_get(lp->pilot);
   if (p==NULL) {
      NLUA_ERROR(L,"Pilot is invalid.");
      return 0;
   }

   /* Set the new faction. */
   p->faction = fid;

   return 0;
}


/**
 * @brief Sets the pilot as hostile to player.
 *
 * @usage p:setHostile()
 *
 *    @luaparam p Pilot to set as hostile.
 * @luafunc setHostile( p )
 */
static int pilotL_setHostile( lua_State *L )
{
   LuaPilot *lp;
   Pilot *p;

   /* Get the pilot. */
   lp = luaL_checkpilot(L,1);
   p  = pilot_get(lp->pilot);
   if (p==NULL) {
      NLUA_ERROR(L,"Pilot is invalid.");
      return 0;
   }

   /* Set as hostile. */
   pilot_rmFlag(p, PILOT_FRIENDLY);
   pilot_setHostile(p);

   return 0;
}


/**
 * @brief Sets the pilot as friendly to player.
 *
 * @usage p:setFriendly()
 *
 *    @luaparam p Pilot to set as friendly.
 * @luafunc setFriendly( p )
 */
static int pilotL_setFriendly( lua_State *L )
{
   LuaPilot *lp;
   Pilot *p;

   /* Get the pilot. */
   lp = luaL_checkpilot(L,1);
   p = pilot_get(lp->pilot);
   if (p==NULL) {
      NLUA_ERROR(L,"Pilot is invalid.");
      return 0;
   }

   /* Remove hostile and mark as friendly. */
   pilot_setFriendly(p);

   return 0;
}


/**
 * @brief Sets the pilot's invincibility status.
 *
 * @usage p:setInvincible() -- p can not be hit anymore
 * @usage p:setInvincible(true) -- p can not be hit anymore
 * @usage p:setInvincible(false) -- p can be hit again
 *
 *    @luaparam p Pilot to set invincibility status of.
 *    @luaparam state State to set invincibility, if omitted defaults to true.
 * @luafunc setInvincible( p, state )
 */
static int pilotL_setInvincible( lua_State *L )
{
   LuaPilot *lp;
   Pilot *p;
   int state;

   /* Get the pilot. */
   lp = luaL_checkpilot(L,1);
   p = pilot_get(lp->pilot);
   if (p==NULL) {
      NLUA_ERROR(L,"Pilot is invalid.");
      return 0;
   }

   /* Get state. */
   if (lua_gettop(L) > 1)
      state = lua_toboolean(L, 2);
   else
      state = 1;

   /* Set status. */
   if (state)
      pilot_setFlag(p, PILOT_INVINCIBLE);
   else
      pilot_rmFlag(p, PILOT_INVINCIBLE);

   return 0;
}


/**
 * @brief Disables a pilot.
 *
 * @usage p:disable()
 *
 *    @luaparam p Pilot to disable.
 * @luafunc disable( p )
 */
static int pilotL_disable( lua_State *L )
{
   LuaPilot *lp;
   Pilot *p;

   /* Get the pilot. */
   lp = luaL_checkpilot(L,1);
   p  = pilot_get(lp->pilot);
   if (p==NULL) {
      NLUA_ERROR(L,"Pilot is invalid.");
      return 0;
   }

   /* Disable the pilot. */
   p->shield = 0.;
   p->armour = PILOT_DISABLED_ARMOR * p->ship->armour;
   pilot_setFlag( p, PILOT_DISABLED );

   return 0;
}


/**
 * @brief Adds an outfit to a pilot.
 *
 * @usage added = p:addOutfit( "Laser Cannon", 5 ) -- Adds 5 laser cannons to p
 *
 *    @luaparam p Pilot to add outfit to.
 *    @luaparam outfit Name of the outfit to add.
 *    @lusparam q Amount of the outfit to add (defaults to 1).
 *    @luareturn True is outfit was added successfully.
 * @luafunc addOutfit( p, outfit, q )
 */
static int pilotL_addOutfit( lua_State *L )
{
   int i;
   LuaPilot *lp;
   Pilot *p;
   const char *outfit;
   Outfit *o;
   int ret;
   int q;

   /* Get the pilot. */
   lp = luaL_checkpilot(L,1);
   p  = pilot_get(lp->pilot);
   if (p==NULL) {
      NLUA_ERROR(L,"Pilot is invalid.");
      return 0;
   }

   /* Get parameters. */
   outfit = luaL_checkstring(L,2);
   q      = 1;
   if (lua_gettop(L) > 2)
      q = luaL_checkint(L,3);

   /* Get the outfit. */
   o = outfit_get( outfit );
   if (o == NULL)
      return 0;
   
   /* Add outfit. */
   for (i=0; i<p->noutfits; i++) {
      /* Must still have to add outfit. */
      if (q <= 0)
         break;

      /* Must not have outfit already. */
      if (p->outfits[i]->outfit != NULL)
         continue;

      /* Must fit slot. */
      if (o->slot != p->outfits[i]->slot)
         continue;

      /* Test if can add outfit. */
      ret = pilot_addOutfitTest( p, o, p->outfits[i], 0 );
      if (ret) {
         lua_pushboolean(L, 0);
         return 1;
      }

      /* Add outfit - already tested. */
      ret = pilot_addOutfitRaw( p, o, p->outfits[i] );
      pilot_calcStats( p );

      /* Add ammo if needed. */
      if ((ret==0) && (outfit_ammo(o) != NULL))
         pilot_addAmmo( p, p->outfits[i], outfit_ammo(o), outfit_amount(o) );

      /* We added an outfit. */
      q--;
   }
   lua_pushboolean(L,!ret);
   return 1;
}


/**
 * @brief Removes an outfit from a pilot.
 *
 * "all" will remove all outfits.
 *
 * @usage p:rmOutfit( "all" ) -- Leaves the pilot naked.
 * @usage p:rmOutfit( "Neutron Disruptor" ) -- Removes a neutron disruptor.
 * @usage p:rmOutfit( "Neutron Disruptor", 2 ) -- Removes two neutron disruptor.
 *
 *    @luparam p Pilot to remove outfit from.
 *    @luaparam outfit Name of the outfit to remove.
 * @luafunc rmOutfit( p, outfit )
 */
static int pilotL_rmOutfit( lua_State *L )
{
   int i;
   LuaPilot *lp;
   Pilot *p;
   const char *outfit;
   Outfit *o;
   int q;

   /* Get the pilot. */
   lp = luaL_checkpilot(L,1);
   p  = pilot_get(lp->pilot);
   if (p==NULL) {
      NLUA_ERROR(L,"Pilot is invalid.");
      return 0;
   }

   /* Get parameters. */
   outfit = luaL_checkstring(L,2);
   q      = 1;
   if (lua_gettop(L) > 2)
      q = luaL_checkint(L,3);

   /* If outfit is "all", we remove everything. */
   if (strcmp(outfit,"all")==0) {
      for (i=0; i<p->noutfits; i++) {
         pilot_rmOutfitRaw( p, p->outfits[i] );
      }
      pilot_calcStats( p ); /* Recalculate stats. */
      return 0;
   }

   /* Get the outfit. */
   o = outfit_get( outfit );
   if (o == NULL) {
      NLUA_ERROR(L,"Outfit isn't found in outfit stack.");
      return 0;
   }
   
   /* Remove the outfit outfit. */
   for (i=0; i<p->noutfits; i++) {
      /* Must still need to remove. */
      if (q <= 0)
         break;

      /* Not found. */
      if (p->outfits[i]->outfit != o)
         continue;

      /* Remove outfit. */
      pilot_rmOutfit( p, p->outfits[i] );
      q--;
   }
   return 0;
}


/**
 * @Brief Sets the fuel of a pilot.
 *
 * @usage p:setFuel( true ) -- Sets fuel to max
 *
 *    @luaparam p Pilot to set fuel of.
 *    @luaparam f true sets fuel to max, false sets fuel to 0, a number sets
 *              fuel to that amount in units.
 *    @luareturn The amount of fuel the pilot has.
 * @luafunc setFuel( p, f )
 */
static int pilotL_setFuel( lua_State *L )
{
   LuaPilot *lp;
   Pilot *p;

   /* Get the pilot. */
   lp = luaL_checkpilot(L,1);
   p  = pilot_get(lp->pilot);
   if (p==NULL) {
      NLUA_ERROR(L,"Pilot is invalid.");
      return 0;
   }

   /* Get the parameter. */
   if (lua_isboolean(L,2)) {
      if (lua_toboolean(L,2))
         p->fuel = p->fuel_max;
      else
         p->fuel = 0.;
   }
   else if (lua_isnumber(L,2)) {
      p->fuel = CLAMP( 0., p->fuel_max, lua_tonumber(L,2) );
   }
   else
      NLUA_INVALID_PARAMETER();

   /* Return amount of fuel. */
   lua_pushnumber(L, p->fuel);
   return 1;
}


/**
 * @brief Changes the pilot's AI.
 *
 * @usage p:changeAI( "empire" ) -- set the pilot to use the Empire AI
 *
 * @luafunc changeAI( p, newai )
 */
static int pilotL_changeAI( lua_State *L )
{
   LuaPilot *lp;
   Pilot *p;
   const char *str;
   int ret;

   /* Get the pilot. */
   lp = luaL_checkpilot(L,1);
   p  = pilot_get(lp->pilot);
   if (p==NULL) {
      NLUA_ERROR(L,"Pilot is invalid.");
      return 0;
   }

   /* Get parameters. */
   str = luaL_checkstring(L,2);

   /* Get rid of current AI. */
   ai_destroy(p);

   /* Create the new AI. */
   ret = ai_pinit( p, str );
   lua_pushboolean(L, ret);
   return 1;
}


/**
 * @brief Sets the health of a pilot.
 *
 * This recovers the pilot's disabled state, although he may become disabled afterwards.
 *
 * @usage p:setHealth( 100, 100 ) -- Sets pilot to full health
 * @usage p:setHealth(  70,   0 ) -- Sets pilot to 70% shield
 *
 *    @luaparam p Pilot to set health of.
 *    @luaparam armour Value to set armour to, should be double from 0-100 (in percent).
 *    @luaparam shield Value to set shield to, should be double from 0-100 (in percent).
 * @luafunc setHealth( p, armour, shield )
 */
static int pilotL_setHealth( lua_State *L )
{
   LuaPilot *lp;
   Pilot *p;
   double a, s;

   /* Get the pilot. */
   lp = luaL_checkpilot(L,1);
   p  = pilot_get(lp->pilot);
   if (p==NULL) {
      NLUA_ERROR(L,"Pilot is invalid.");
      return 0;
   }

   /* Handle parameters. */
   a  = luaL_checknumber(L, 2);
   s  = luaL_checknumber(L, 3);
   a /= 100.;
   s /= 100.;

   /* Set health. */
   p->armour = a * p->armour_max;
   p->shield = s * p->shield_max;

   /* Undisable if was disabled. */
   pilot_rmFlag( p, PILOT_DISABLED );

   return 0;
}


/**
 * @brief Sets the ability to board the pilot.
 *
 * @usage p:setNoboard( true ) -- Pilot can not be boarded by anyone
 *
 *    @luaparam p Pilot to set disable boarding.
 *    @luaparam noboard If true it disallows boarding of the pilot, otherwise
 *              it allows boarding which is the default.
 * @luafunc setNoboard( p, noboard )
 */
static int pilotL_setNoboard( lua_State *L )
{
   LuaPilot *lp;
   Pilot *p;
   int disable;

   /* Get the pilot. */
   lp = luaL_checkpilot(L,1);
   p  = pilot_get(lp->pilot);
   if (p==NULL) {
      NLUA_ERROR(L,"Pilot is invalid.");
      return 0;
   }

   /* Handle parameters. */
   disable = !lua_toboolean(L, 2);

   /* See if should mark as boarded. */
   if (disable)
      pilot_setFlag(p, PILOT_NOBOARD);
   else
      pilot_rmFlag(p, PILOT_NOBOARD);

   return 0;
}


/**
 * @brief Gets the pilot's health.
 *
 * @usage armour, shield = p:getHealth()
 *
 *    @luaparam p Pilot to get health of.
 *    @luareturn The armour and shield of the pilot in % [0:100].
 * @luafunc getHealth( p )
 */
static int pilotL_getHealth( lua_State *L )
{
   LuaPilot *lp;
   Pilot *p;

   /* Get the pilot. */
   lp = luaL_checkpilot(L,1);
   p  = pilot_get(lp->pilot);
   if (p==NULL) {
      NLUA_ERROR(L,"Pilot is invalid.");
      return 0;
   }

   /* Return parameters. */
   lua_pushnumber(L, p->armour / p->armour_max * 100. );
   lua_pushnumber(L, p->shield / p->shield_max * 100. );

   return 2;
}


/**
 * @brief Gets the name of the pilot's ship.
 *
 * @usage shipname = p:shipName()
 *
 *    @luaparam p Pilot to get ship name.
 *    @luareturn The name of the pilot's ship.
 * @luafunc shipName( p )
 */
static int pilotL_shipName( lua_State *L )
{
   LuaPilot *lp;
   Pilot *p;

   /* Get the pilot. */
   lp = luaL_checkpilot(L,1);
   p  = pilot_get(lp->pilot);
   if (p==NULL) {
      NLUA_ERROR(L,"Pilot is invalid.");
      return 0;
   }

   /** Return the ship name. */
   lua_pushstring(L, p->ship->name);
   return 1;
}


/**
 * @brief Gets the pilot's ship.
 *
 * @usage s = p:ship()
 *
 *    @luaparam p Pilot to get ship of.
 *    @luareturn The ship of the pilot.
 * @luafunc ship( p )
 */
static int pilotL_ship( lua_State *L )
{
   LuaPilot *lp;
   Pilot *p;
   LuaShip ls;

   /* Get the pilot. */
   lp = luaL_checkpilot(L,1);
   p  = pilot_get(lp->pilot);
   if (p==NULL) {
      NLUA_ERROR(L,"Pilot is invalid.");
      return 0;
   }

   /* Create the ship. */
   ls.ship = p->ship;
   lua_pushship(L, ls);
   return 1;
}


