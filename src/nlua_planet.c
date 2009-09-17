/*
 * See Licensing and Copyright notice in naev.h
 */

/**
 * @file nlua_planet.c
 *
 * @brief Lua planet module.
 */

#include "nlua_planet.h"

#include "naev.h"

#include "lauxlib.h"

#include "nlua.h"
#include "nluadef.h"
#include "nlua_faction.h"
#include "nlua_vec2.h"
#include "nlua_system.h"
#include "log.h"
#include "rng.h"
#include "land.h"
#include "map.h"


/* Planet metatable methods */
static int planetL_cur( lua_State *L );
static int planetL_get( lua_State *L );
static int planetL_eq( lua_State *L );
static int planetL_name( lua_State *L );
static int planetL_faction( lua_State *L );
static int planetL_class( lua_State *L );
static int planetL_position( lua_State *L );
static int planetL_hasServices( lua_State *L );
static int planetL_hasBasic( lua_State *L );
static int planetL_hasCommodities( lua_State *L );
static int planetL_hasOutfits( lua_State *L );
static int planetL_hasShipyard( lua_State *L );
static const luaL_reg planet_methods[] = {
   { "cur", planetL_cur },
   { "get", planetL_get },
   { "__eq", planetL_eq },
   { "__tostring", planetL_name },
   { "name", planetL_name },
   { "faction", planetL_faction },
   { "class", planetL_class },
   { "pos", planetL_position },
   { "hasServices", planetL_hasServices },
   { "hasBasic", planetL_hasBasic },
   { "hasCommodities", planetL_hasCommodities },
   { "hasOutfits", planetL_hasOutfits },
   { "hasShipyard", planetL_hasShipyard },
   {0,0}
}; /**< Planet metatable methods. */


/**
 * @brief Loads the planet library.
 *
 *    @param L State to load planet library into.
 *    @param readonly Load read only functions?
 *    @return 0 on success.
 */
int nlua_loadPlanet( lua_State *L, int readonly )
{
   (void) readonly;
   /* Create the metatable */
   luaL_newmetatable(L, PLANET_METATABLE);

   /* Create the access table */
   lua_pushvalue(L,-1);
   lua_setfield(L,-2,"__index");

   /* Register the values */
   luaL_register(L, NULL, planet_methods);

   /* Clean up. */
   lua_setfield(L, LUA_GLOBALSINDEX, PLANET_METATABLE);

   return 0; /* No error */
}


/**
 * @brief This module allows you to handle the planets from Lua.
 *
 * Generally you do something like:
 *
 * @code
 * p,s = planet.get() -- Get current planet and system
 * if p:services() > 0 then -- planet has services
 *    v = p:pos() -- Get the position
 *    -- Do other stuff
 * end
 * @endcode
 *
 * @luamod planet
 */
/**
 * @brief Gets planet at index.
 *
 *    @param L Lua state to get planet from.
 *    @param ind Index position to find the planet.
 *    @return Planet found at the index in the state.
 */
LuaPlanet* lua_toplanet( lua_State *L, int ind )
{
   return (LuaPlanet*) lua_touserdata(L,ind);
}
/**
 * @brief Gets planet at index raising an error if isn't a planet.
 *
 *    @param L Lua state to get planet from.
 *    @param ind Index position to find the planet.
 *    @return Planet found at the index in the state.
 */
LuaPlanet* luaL_checkplanet( lua_State *L, int ind )
{
   if (lua_isplanet(L,ind))
      return lua_toplanet(L,ind);
   luaL_typerror(L, ind, PLANET_METATABLE);
   return NULL;
}
/**
 * @brief Pushes a planet on the stack.
 *
 *    @param L Lua state to push planet into.
 *    @param planet Planet to push.
 *    @return Newly pushed planet.
 */
LuaPlanet* lua_pushplanet( lua_State *L, LuaPlanet planet )
{
   LuaPlanet *p;
   p = (LuaPlanet*) lua_newuserdata(L, sizeof(LuaPlanet));
   *p = planet;
   luaL_getmetatable(L, PLANET_METATABLE);
   lua_setmetatable(L, -2);
   return p;
}
/**
 * @brief Checks to see if ind is a planet.
 *
 *    @param L Lua state to check.
 *    @param ind Index position to check.
 *    @return 1 if ind is a planet.
 */
int lua_isplanet( lua_State *L, int ind )
{
   int ret;

   if (lua_getmetatable(L,ind)==0)
      return 0;
   lua_getfield(L, LUA_REGISTRYINDEX, PLANET_METATABLE);

   ret = 0;
   if (lua_rawequal(L, -1, -2))  /* does it have the correct mt? */ 
      ret = 1;

   lua_pop(L, 2);  /* remove both metatables */ 
   return ret;
}


/**
 * @brief Gets the current planet - MUST BE LANDED.
 *
 * @usage p,s = planet.cur() -- Gets current planet (assuming landed)
 *
 *    @luareturn The planet and system in belongs to.
 * @luafunc cur()
 */
static int planetL_cur( lua_State *L )
{
   LuaPlanet planet;
   LuaSystem sys;
   if (land_planet != NULL) {
      planet.p = land_planet;
      lua_pushplanet(L,planet);
      sys.s = system_get( planet_getSystem(land_planet->name) );
      lua_pushsystem(L,sys);
      return 2;
   }
   NLUA_ERROR(L,"Attempting to get landed planet when player not landed.");
   return 0; /* Not landed. */
}


/**
 * @brief Gets a planet.
 *
 * Possible values of param:
 *    - nil : Gets the current landed planet or nil if there is none.
 *    - bool : Gets a random planet.
 *    - faction : Gets random planet belonging to faction matching the number.
 *    - string : Gets the planet by name.
 *    - table : Gets random planet belonging to any of the factions in the
 *               table.
 *
 * @usage p,s = planet.get( "Anecu" ) -- Gets planet by name
 * @usage p,s = planet.get( faction.get( "Empire" ) ) -- Gets random Empire planet
 * @usage p,s = planet.get(true) -- Gets completely random planet
 * @usage p,s = planet.get( { faction.get("Empire"), faction.get("Dvaered") } ) -- Random planet belonging to Empire or Dvaered
 *    @luaparam param See description.
 *    @luareturn Returns the planet and the system it belongs to.
 * @luafunc get( param )
 */
static int planetL_get( lua_State *L )
{
   int i;
   int *factions;
   int nfactions;
   char **planets;
   int nplanets;
   const char *rndplanet;
   LuaPlanet planet;
   LuaSystem sys;
   LuaFaction *f;

   rndplanet = NULL;
   nplanets = 0;
  
   /* Get the landed planet */
   if (lua_gettop(L) == 0) {
      if (land_planet != NULL) {
         planet.p = land_planet;
         lua_pushplanet(L,planet);
         sys.s = system_get( planet_getSystem(land_planet->name) );
         lua_pushsystem(L,sys);
         return 2;
      }
      NLUA_ERROR(L,"Attempting to get landed planet when player not landed.");
      return 0; /* Not landed. */
   }

   /* If boolean return random. */
   else if (lua_isboolean(L,1)) {
      planet.p = planet_get( space_getRndPlanet() );
      lua_pushplanet(L,planet);
      sys.s = system_get( planet_getSystem(land_planet->name) );
      lua_pushsystem(L,sys);
      return 2;
   }

   /* Get a planet by faction */
   else if (lua_isfaction(L,1)) {
      f = lua_tofaction(L,1);
      planets = space_getFactionPlanet( &nplanets, &f->f, 1 );
   }

   /* Get a planet by name */
   else if (lua_isstring(L,1)) {
      rndplanet = lua_tostring(L,1);
   }

   /* Get a planet from faction list */
   else if (lua_istable(L,1)) {
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
      
      /* get the planets */
      planets = space_getFactionPlanet( &nplanets, factions, nfactions );
      free(factions);
   }
   else NLUA_INVALID_PARAMETER(); /* Bad Parameter */

   /* No suitable planet found */
   if ((rndplanet == NULL) && (nplanets == 0)) {
      free(planets);
      return 0;
   }
   /* Pick random planet */
   else if (rndplanet == NULL) {
      rndplanet = planets[RNG(0,nplanets-1)];
      free(planets);
   }

   /* Push the planet */
   planet.p = planet_get(rndplanet); /* The real planet */
   lua_pushplanet(L,planet);
   sys.s = system_get( planet_getSystem(rndplanet) );
   lua_pushsystem(L,sys);
   return 2;
}

/**
 * @brief You can use the '=' operator within Lua to compare planets with this.
 *
 * @usage if p.__eq( planet.get( "Anecu" ) ) then -- Do something
 * @usage if p == planet.get( "Anecu" ) then -- Do something
 *    @luaparam p Planet comparing.
 *    @luaparam comp planet to compare against.
 *    @luareturn true if both planets are the same.
 * @luafunc __eq( p, comp )
 */
static int planetL_eq( lua_State *L )
{
   LuaPlanet *a, *b;
   a = luaL_checkplanet(L,1);
   b = luaL_checkplanet(L,2);
   lua_pushboolean(L,(a->p == b->p));
   return 1;
}

/**
 * @brief Gets the planet's name.
 *
 * @usage name = p:name()
 *    @luaparam p Planet to get the name of.
 *    @luareturn The name of the planet.
 * @luafunc name( p )
 */
static int planetL_name( lua_State *L )
{
   LuaPlanet *p;
   p = luaL_checkplanet(L,1);
   lua_pushstring(L,p->p->name);
   return 1;
}

/**
 * @brief Gets the planet's faction.
 *
 * @usage f = p:faction()
 *    @luaparam p Planet to get the faction of.
 *    @luareturn The planet's faction.
 * @luafunc faction( p )
 */
static int planetL_faction( lua_State *L )
{
   LuaPlanet *p;
   LuaFaction f;
   p = luaL_checkplanet(L,1);
   if (p->p->faction < 0)
      return 0;
   f.f = p->p->faction;
   lua_pushfaction(L, f);
   return 1;
}

/**
 * @brief Gets the planet's class.
 *
 * Usually classes are characters for planets (see space.h) and numbers
 * for stations.
 *
 * @usage c = p:class()
 *    @luaparam p Planet to get the class of.
 *    @luareturn The class of the planet in a one char identifier.
 * @luafunc class( p )
 */
static int planetL_class(lua_State *L )
{
   char buf[2];
   LuaPlanet *p;
   p = luaL_checkplanet(L,1);
   buf[0] = planet_getClass(p->p);
   buf[1] = '\0';
   lua_pushstring(L,buf);
   return 1;
}


/**
 * @brief Checks if a planet has services (any flag besides SERVICE_LAND).
 *
 * @usage if p:hasServices() then -- Planet has services
 *    @luaparam p Planet to get the services of.
 *    @luareturn True f the planets has services.
 * @luafunc hasServices( p )
 */
static int planetL_hasServices( lua_State *L )
{
   LuaPlanet *p;
   p = luaL_checkplanet(L,1);
   lua_pushboolean(L, (p->p->services & (~PLANET_SERVICE_LAND)));
   return 1;
}


/**
 * @brief Checks if a planet has basic services (spaceport bar, mission computer).
 *
 * @usage if p:hasBasic() then -- Planet has basic service
 *    @luaparam p Planet to get the services of.
 *    @luareturn True f the planets has basic services.
 * @luafunc hasBasic( p )
 */
static int planetL_hasBasic( lua_State *L )
{
   LuaPlanet *p;
   p = luaL_checkplanet(L,1);
   lua_pushboolean(L, (p->p->services & PLANET_SERVICE_BASIC));
   return 1;
}


/**
 * @brief Checks if a planet has commodities exchange service.
 *
 * @usage if p:hasCommodities() then -- Planet has commodity exchange services
 *    @luaparam p Planet to get the services of.
 *    @luareturn True f the planets has commodity exchange service.
 * @luafunc hasCommodities( p )
 */
static int planetL_hasCommodities( lua_State *L )
{
   LuaPlanet *p;
   p = luaL_checkplanet(L,1);
   lua_pushboolean(L, (p->p->services & PLANET_SERVICE_COMMODITY));
   return 1;
}


/**
 * @brief Checks if a planet has outfit services.
 *
 * @usage if p:hasOutfits() then -- Planet has outfit services
 *    @luaparam p Planet to get the services of.
 *    @luareturn True f the planets has outfitting services.
 * @luafunc hasOutfits( p )
 */
static int planetL_hasOutfits( lua_State *L )
{
   LuaPlanet *p;
   p = luaL_checkplanet(L,1);
   lua_pushboolean(L, (p->p->services & PLANET_SERVICE_OUTFITS));
   return 1;
}


/**
 * @brief Checks if a planet has shipyard services.
 *
 * @usage if p:hasShipyard() then -- Planet has shipyard service
 *    @luaparam p Planet to get the services of.
 *    @luareturn True f the planets has shipyard services.
 * @luafunc hasShipyard( p )
 */
static int planetL_hasShipyard( lua_State *L )
{
   LuaPlanet *p;
   p = luaL_checkplanet(L,1);
   lua_pushboolean(L, (p->p->services & PLANET_SERVICE_SHIPYARD));
   return 1;
}


/**
 * @brief Gets the position of the planet in the system.
 *
 * @usage v = p:pos()
 *    @luaparam p Planet to get the position of.
 *    @luareturn The position of the planet in the system as a vec2.
 * @luafunc pos( p )
 */
static int planetL_position( lua_State *L )
{
   LuaPlanet *p;
   LuaVector v;
   p = luaL_checkplanet(L,1);
   vectcpy(&v.vec, &p->p->pos);
   lua_pushvector(L, v);
   return 1;
}

