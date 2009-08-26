/*
 * See Licensing and Copyright notice in naev.h
 */

/**
 * @file news.c
 *
 * @brief Handles news generation.
 */


#include "news.h"

#include "naev.h"

#include <stdint.h>
#include <stdlib.h>

#include "log.h"
#include "nlua.h"
#include "nluadef.h"
#include "nlua_misn.h"
#include "nlua_faction.h"
#include "nlua_diff.h"
#include "nlua_var.h"
#include "ndata.h"
#include "toolkit.h"


#define LUA_NEWS     "dat/news.lua"


/*
 * News state.
 */
static lua_State *news_state  = NULL; /**< Lua news state. */


/*
 * News buffer.
 */
static news_t *news_buf       = NULL; /**< Buffer of news. */
static int news_nbuf          = 0; /**< Size of news buffer. */


/*
 * News line buffer.
 */
static unsigned int news_tick = 0; /**< Last news tick. */
static int news_drag          = 0; /**< Is dragging news? */
static double news_pos        = 0.; /**< Position of the news feed. */
static glFont *news_font      = &gl_defFont; /**< Font to use. */
static char **news_lines      = NULL; /**< Text per line. */
static int news_nlines        = 0; /**< Number of lines used. */
static int news_mlines        = 0; /**< Lines allocated. */


/*
 * Prototypes.
 */
static void news_cleanBuffer (void);
static void news_cleanLines (void);
static void news_render( double bx, double by, double w, double h, void *data );
static void news_mouse( unsigned int wid, SDL_Event *event, double mx, double my,
      double w, double h, void *mouse );


/**
 * @brief Renders a news widget.
 *
 *    @param bx Base X position to render at.
 *    @param by Base Y positoin to render at.
 *    @param w Width of the widget.
 *    @param h Height of the widget.
 */
static void news_render( double bx, double by, double w, double h, void *data )
{
   (void) data;
   int i;
   unsigned int t;
   double y, dt;

   t = SDL_GetTicks();

   /* Calculate offset. */
   if (!news_drag) {
      dt = (double)(t-news_tick)/1000.;
      news_pos += dt * 25.;
   }
   news_tick = t;

   /* Make sure user isn't silly and drags it to negative values. */
   if (news_pos < 0.)
      news_pos += (news_font->h + 5.) * news_nlines + h + 3;

   /* background */
   gl_renderRect( bx, by, w, h, &cBlack );

   /* Render the text. */
   i = (int)(news_pos / (news_font->h + 5.));
   if (i > news_nlines + (int)(h/(news_font->h + 5.)) + 3) {
      news_pos = 0.;
      return;
   }

   /* Get start position. */
   y = news_pos - (i+1) * (news_font->h + 5.) - 10.;

   /* Draw loop. */
   while (i >= 0) {

      /* Skip in line isn't valid. */
      if (i >= news_nlines) {
         i--;
         y += news_font->h + 5.;
         continue;
      }

      gl_printMidRaw( news_font, w-40.,
            bx+10 + (double)SCREEN_W/2., by+y + (double)SCREEN_H/2.,
            &cConsole, news_lines[i] );

      /* Increment line and position. */
      i--;
      y += news_font->h + 5.;
   }

}


/**
 * @brief wid Window recieving the mouse events.
 *
 *    @param event Mouse event being recieved.
 *    @param mx X position of the mouse.
 *    @param my Y position of the mouse.
 *    @param w Width of the widget.
 *    @param h Height of the widget.
 */
static void news_mouse( unsigned int wid, SDL_Event *event, double mx, double my,
      double w, double h, void *data )
{
   (void) wid;
   (void) data;

   switch (event->type) {
      case SDL_MOUSEBUTTONDOWN:
         /* Must be in bounds. */
         if ((mx < 0.) || (mx > w) || (my < 0.) || (my > h))
            return;

         if (event->button.button == SDL_BUTTON_WHEELUP)
            news_pos -= h/3.;
         else if (event->button.button == SDL_BUTTON_WHEELDOWN)
            news_pos += h/3.;
         else if (!news_drag)
            news_drag = 1;
         break;

      case SDL_MOUSEBUTTONUP:
         if (news_drag)
            news_drag = 0;
         break;

      case SDL_MOUSEMOTION:
         if (news_drag)
            news_pos -= event->motion.yrel;
         break;
   }
}


/**
 * @brief Creates a news widget.
 *
 *    @param wid Window to create news widget on.
 *    @param x X position of the widget to create.
 *    @param y Y position of the widget to create.
 *    @param w Width of the widget.
 *    @param h Height of the widget.
 */
void news_widget( unsigned int wid, int x, int y, int w, int h )
{
   int i, p, len;
   char buf[4096];

   /* Sane defaults. */
   news_pos    = h/3;
   news_tick   = SDL_GetTicks();

   /* Clean news lines. */
   news_cleanLines();

   /* Load up the news in a string. */
   p = 0;
   for (i=0; i<news_nbuf; i++) {
      p += snprintf( &buf[p], 2048-p,
            "%s\n\n"
            "%s\n\n\n\n"
            , news_buf[i].title, news_buf[i].desc );
   }
   len = p;

   /* Now load up the text. */
   p = 0;
   news_nlines = 0;
   while (p < len) {
      /* Get the length. */
      i = gl_printWidthForText( NULL, &buf[p], w-40 );

      /* Copy the line. */
      if (news_nlines+1 > news_mlines) {
         news_mlines += 128;
         news_lines = realloc( news_lines, sizeof(char*) * news_mlines );
      }
      news_lines[news_nlines] = malloc( i + 1 );
      strncpy( news_lines[news_nlines], &buf[p], i );
      news_lines[news_nlines][i] = '\0';

      p += i + 1; /* Move pointer. */
      news_nlines++; /* New line. */
   }

   /* Create the custom widget. */
   window_addCust( wid, x, y, w, h,
         "cstNews", 1, news_render, news_mouse, NULL );
}


/**
 * @brief Initializes the news.
 *
 *    @return 0 on success.
 */
int news_init (void)
{
   lua_State *L;
   char *buf;
   uint32_t bufsize;

   /* Already initialized. */
   if (news_state != NULL)
      return 0;

   /* Create the state. */
   news_state = nlua_newState();
   L = news_state;

   /* Load the libraries. */
   nlua_loadBasic(L);
   nlua_load(L,luaopen_string);
   nlua_loadStandard(L, 1);

   /* Load the news file. */
   buf = ndata_read( LUA_NEWS, &bufsize );
   if (luaL_dobuffer(news_state, buf, bufsize, LUA_NEWS) != 0) {
      WARN("Failed to load news file: %s\n"
           "%s\n"
           "Most likely Lua file has improper syntax, please check",
            LUA_NEWS, lua_tostring(L,-1));
      return -1;
   }
   free(buf);

   return 0;
}


/**
 * @brief Cleans the news buffer.
 */
static void news_cleanBuffer (void)
{
   int i;

   if (news_buf != NULL) {
      for (i=0; i<news_nbuf; i++) {
         free(news_buf[i].title);
         free(news_buf[i].desc);
      }
      free(news_buf);
      news_buf    = NULL;
      news_nbuf   = 0;
   }
}


/**
 * @brief Cleans the lines.
 */
static void news_cleanLines (void)
{
   int i;

   if (news_nlines != 0) {
      for (i=0; i<news_nlines; i++)
         free(news_lines[i]);
      news_nlines = 0;
   }
}


/**
 * @brief Cleans up the news stuff.
 */
void news_exit (void)
{
   /* Already freed. */
   if (news_state == NULL)
      return;

   /* Clean the buffers. */
   news_cleanBuffer();

   /* Clean the lines. */
   news_cleanLines();
   free(news_lines);
   news_lines  = NULL;
   news_mlines = 0;

   /* Clean up. */
   lua_close(news_state);
   news_state = NULL;
}


/**
 * @brief Gets a news sentence.
 */
const news_t *news_generate( int *ngen, int n )
{
   int i;
   lua_State *L;

   /* Lazy allocation. */
   if (news_state == NULL)
      news_init();
   L = news_state;

   /* Clean up the old buffer. */
   news_cleanBuffer();

   /* Allocate news. */
   news_buf = calloc( sizeof(news_t), n );
   if (ngen != NULL)
      (*ngen)  = 0;

   /* Run the function. */
   lua_getglobal(L, "news"); /* f */
   lua_pushnumber(L, n); /* f, n */
   if (lua_pcall(L, 1, 2, 0)) { /* error has occured */
      WARN("News: '%s' : %s", "news", lua_tostring(L,-1));
      lua_pop(L,2);
      return NULL;
   }
   /* str, t */

   /* Check to see if it's valid. */
   if (!lua_isstring(L, -2) || !lua_istable(L, -1)) { 
      WARN("News generated invalid output!");
      lua_pop(L,2);
      return NULL;
   }

   /* Create the title header. */
   news_buf[0].title = strdup("NEWS HEADLINES");
   news_buf[0].desc  = strdup( lua_tostring(L, -2) );

   /* Pull it out of the table. */
   i = 1;
   lua_pushnil(L); /* str, table, nil */
   while (lua_next(L,-2) != 0) {
      /* Pull out of the internal table the data. */
      lua_getfield(L, -1, "title"); /* str, table, key, val, str */
      news_buf[i].title = strdup( luaL_checkstring(L, -1) );
      lua_pop(L,1); /* str, table, key, val */
      lua_getfield(L, -1, "desc"); /* str, table, key, val, str */
      news_buf[i].desc = strdup( luaL_checkstring(L, -1) );
      lua_pop(L,1); /* str, table, key, val */
      /* Go to next element. */
      lua_pop(L,1); /* str, table, key */
      i++;
      if (i>=n)
         break;
   }

   if (i>=n) { /* Need to pop two extras. */
      lua_pop(L,1);
   }
   /* str, table */

   /* Clean up results. */
   lua_pop(L,2); /* */

   /* Save news found. */
   news_nbuf   = i;
   if (ngen != NULL)
      (*ngen)  = news_nbuf;
   
   return news_buf;
}


