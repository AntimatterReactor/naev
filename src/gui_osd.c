/*
 * See Licensing and Copyright notice in naev.h
 */
/** @cond */
#include <stdlib.h>

#include "naev.h"
/** @endcond */

#include "gui_osd.h"

#include "array.h"
#include "font.h"
#include "log.h"
#include "nstring.h"
#include "opengl.h"

/**
 * @brief On Screen Display element.
 */
typedef struct OSD_s {
   unsigned int id;  /**< OSD id. */
   int priority;     /**< Priority level. */
   char *title;      /**< Title of the OSD. */
   char **titlew;    /**< Wrapped version of the title. */

   char **msg;       /**< Array (array.h): Stored messages. */
   char ***items;    /**< Array of array (array.h) of allocated strings. */

   unsigned int active; /**< Active item. */
} OSD_t;

/*
 * OSD array.
 */
static unsigned int osd_idgen = 0; /**< ID generator for OSD. */
static OSD_t *osd_list        = NULL; /**< Array (array.h) for OSD. */

/*
 * Dimensions.
 */
static int osd_x = 0;
static int osd_y = 0;
static int osd_w = 0;
static int osd_h = 0;
static int osd_lines = 0;
static int osd_rh = 0;
static int osd_tabLen = 0;
static int osd_hyphenLen = 0;

/*
 * Prototypes.
 */
static OSD_t *osd_get( unsigned int osd );
static int osd_free( OSD_t *osd );
static void osd_calcDimensions (void);
/* Sort. */
static int osd_sortCompare( const void * arg1, const void * arg2 );
static void osd_sort (void);
static void osd_wordwrap( OSD_t* osd );

static int osd_sortCompare( const void *arg1, const void *arg2 )
{
   const OSD_t *osd1, *osd2;
   int ret, m;

   osd1 = (OSD_t*)arg1;
   osd2 = (OSD_t*)arg2;

   /* Compare priority. */
   if (osd1->priority > osd2->priority)
      return +1;
   else if (osd1->priority < osd2->priority)
      return -1;

   /* Compare name. */
   ret = strcmp( osd1->title, osd2->title );
   if (ret != 0)
      return ret;

   /* Compare items. */
   m = MIN(array_size(osd1->items), array_size(osd2->items));
   for (int i=0; i<m; i++) {
      ret = strcmp( osd1->msg[i], osd2->msg[i] );
      if (ret != 0)
         return ret;
   }

   /* Compare on length. */
   if (array_size(osd1->items) > array_size(osd2->items))
      return +1;
   if (array_size(osd1->items) < array_size(osd2->items))
      return -1;

   /* Compare ID. */
   if (osd1->id > osd2->id)
      return +1;
   else if (osd1->id < osd2->id)
      return -1;
   return 0;
}

/**
 * @brief Sorts the OSD list.
 */
static void osd_sort (void)
{
   qsort( osd_list, array_size(osd_list), sizeof(OSD_t), osd_sortCompare );
}

/**
 * @brief Creates an on-screen display.
 *
 *    @param title Title of the display.
 *    @param nitems Number of items in the display.
 *    @param items Items in the display.
 *    @return ID of newly created OSD.
 */
unsigned int osd_create( const char *title, int nitems, const char **items, int priority )
{
   int id;
   OSD_t *osd;

   /* Create. */
   if (osd_list == NULL)
      osd_list = array_create( OSD_t );
   osd = &array_grow( &osd_list );
   memset( osd, 0, sizeof(OSD_t) );
   osd->id = id = ++osd_idgen;
   osd->active = 0;

   /* Copy text. */
   osd->title  = strdup(title);
   osd->priority = priority;
   osd->msg = array_create_size( char*, nitems );
   osd->items = array_create_size( char**, nitems );
   osd->titlew = array_create( char* );
   for (int i=0; i<nitems; i++) {
      array_push_back( &osd->msg, strdup( items[i] ) );
      array_push_back( &osd->items, array_create(char*) );
   }

   osd_wordwrap( osd );
   osd_sort(); /* THIS INVALIDATES THE osd POINTER. */
   osd_calcDimensions();

   return id;
}

/**
 * @brief Calculates the word-wrapped osd->items from osd->msg.
 */
void osd_wordwrap( OSD_t* osd )
{
   glPrintLineIterator iter;

   /* Do title. */
   for (int i=0; i<array_size(osd->titlew); i++)
      free(osd->titlew[i]);
   array_resize( &osd->titlew, 0 );
   gl_printLineIteratorInit( &iter, &gl_smallFont, osd->title, osd_w );
   while (gl_printLineIteratorNext( &iter )) {
      /* Copy text over. */
      int chunk_len = iter.l_end - iter.l_begin + 1;
      char *chunk = malloc( chunk_len );
      snprintf( chunk, chunk_len, "%s", &iter.text[iter.l_begin] );
      array_push_back( &osd->titlew, chunk );
   }

   /* Do items. */
   for (int i=0; i<array_size(osd->items); i++) {
      int msg_len, w, has_tab;
      const char *chunk_fmt;
      for (int l=0; l<array_size(osd->items[i]); l++)
         free(osd->items[i][l]);
      array_resize( &osd->items[i], 0 );

      msg_len = strlen(osd->msg[i]);
      if (msg_len == 0)
         continue;

      /* Test if tabbed. */
      has_tab = !!(osd->msg[i][0] == '\t');
      w = osd_w - (has_tab ? osd_tabLen : osd_hyphenLen);
      gl_printLineIteratorInit( &iter, &gl_smallFont, &osd->msg[i][has_tab], w );
      chunk_fmt = has_tab ? "   %s" : "- %s";

      while (gl_printLineIteratorNext( &iter )) {
         /* Copy text over. */
         int chunk_len = iter.l_end - iter.l_begin + strlen( chunk_fmt ) - 1;
         char *chunk = malloc( chunk_len );
         snprintf( chunk, chunk_len, chunk_fmt, &iter.text[iter.l_begin] );
         array_push_back( &osd->items[i], chunk );
         chunk_fmt = has_tab ? "   %s" : "%s";
         iter.width = has_tab ? osd_w - osd_tabLen - osd_hyphenLen : osd_w - osd_hyphenLen;
      }
   }
}

/**
 * @brief Gets an OSD by ID.
 *
 *    @param osd ID of the OSD to get.
 */
static OSD_t *osd_get( unsigned int osd )
{
   for (int i=0; i<array_size(osd_list); i++) {
      OSD_t *ll = &osd_list[i];
      if (ll->id == osd)
         return ll;
   }
   WARN(_("OSD '%d' not found."), osd);
   return NULL;
}

/**
 * @brief Frees an OSD struct.
 */
static int osd_free( OSD_t *osd )
{
   free(osd->title);
   for (int i=0; i<array_size(osd->items); i++) {
      free( osd->msg[i] );
      for (int j=0; j<array_size(osd->items[i]); j++)
         free(osd->items[i][j]);
      array_free(osd->items[i]);
   }
   array_free(osd->msg);
   array_free(osd->items);
   for (int i=0; i<array_size(osd->titlew); i++)
      free(osd->titlew[i]);
   array_free(osd->titlew);

   return 0;
}

/**
 * @brief Destroys an OSD.
 *
 *    @param osd ID of the OSD to destroy.
 */
int osd_destroy( unsigned int osd )
{
   for (int i=0; i<array_size( osd_list ); i++) {
      OSD_t *ll = &osd_list[i];
      if (ll->id != osd)
         continue;

      /* Clean up. */
      osd_free( &osd_list[i] );

      /* Remove. */
      array_erase( &osd_list, &osd_list[i], &osd_list[i+1] );

      /* Recalculate dimensions. */
      osd_calcDimensions();

      /* Remove the OSD, if empty. */
      if (array_size(osd_list) == 0)
         osd_exit();

      /* Done here. */
      return 0;
   }

   WARN(_("OSD '%u' not found to destroy."), osd );
   return 0;
}

/**
 * @brief Makes an OSD message active.
 *
 *    @param osd OSD to change active message.
 *    @param msg Message to make active in OSD.
 *    @return 0 on success.
 */
int osd_active( unsigned int osd, int msg )
{
   OSD_t *o = osd_get(osd);
   if (o == NULL)
      return -1;

   if ((msg < 0) || (msg >= array_size(o->items))) {
      WARN(_("OSD '%s' only has %d items (requested %d)"), o->title, array_size(o->items), msg );
      return -1;
   }

   o->active = msg;
   osd_calcDimensions();
   return 0;
}

/**
 * @brief Gets the active OSD MESSAGE>
 *
 *    @param osd OSD to get active message.
 *    @return The active OSD message or -1 on error.
 */
int osd_getActive( unsigned int osd )
{
   OSD_t *o = osd_get(osd);
   if (o == NULL)
      return -1;

   return o->active;
}

/**
 * @brief Sets up the OSD window.
 *
 *    @param x X position to render at.
 *    @param y Y position to render at.
 *    @param w Width to render.
 *    @param h Height to render.
 */
int osd_setup( int x, int y, int w, int h )
{
   /* Set offsets. */
   int must_rewrap = (osd_w != w) && (osd_list != NULL);
   osd_x = x;
   osd_y = y;
   osd_w = w;
   osd_lines = h / (gl_smallFont.h+5);
   osd_h = h - h % (gl_smallFont.h+5);

   /* Calculate some font things. */
   osd_tabLen = gl_printWidthRaw( &gl_smallFont, "   " );
   osd_hyphenLen = gl_printWidthRaw( &gl_smallFont, "- " );

   if (must_rewrap)
      for (int i=0; i<array_size(osd_list); i++)
         osd_wordwrap( &osd_list[i] );
   osd_calcDimensions();

   return 0;
}

/**
 * @brief Destroys all the OSD.
 */
void osd_exit (void)
{
   for (int i=0; i<array_size(osd_list); i++) {
      OSD_t *ll = &osd_list[i];
      osd_free( ll );
   }

   array_free( osd_list );
   osd_list = NULL;
}

/**
 * @brief Renders all the OSD.
 */
void osd_render (void)
{
   double p;
   int l;
   int *ignore;
   int nignore;
   char title[STRMAX_SHORT];

   /* Nothing to render. */
   if (osd_list == NULL)
      return;

   /* TODO this ignore stuff and memory allocation should be computed only when the OSD changes. */
   nignore = array_size(osd_list);
   ignore  = calloc( nignore, sizeof( int ) );

   /* Background. */
   gl_renderRect( osd_x-5., osd_y-(osd_rh+5.), osd_w+10., osd_rh+10, &cBlackHilight );

   /* Render each thingy. */
   p = osd_y-gl_smallFont.h;
   l = 0;
   for (int k=0; k<array_size(osd_list); k++) {
      int x, w, duplicates;
      OSD_t *ll;

      if (ignore[k])
         continue;

      ll = &osd_list[k];
      x = osd_x;
      w = osd_w;

      /* Check how many duplicates we have, mark duplicates for ignoring */
      duplicates = 0;
      for (int m=k+1; m<array_size(osd_list); m++) {
         if ((strcmp(osd_list[m].title, ll->title) == 0) &&
               (array_size(osd_list[m].items) == array_size(ll->items)) &&
               (osd_list[m].active == ll->active)) {
            int is_duplicate = 1;
            for (int i=osd_list[m].active; i<array_size(osd_list[m].items); i++) {
               if (array_size(osd_list[m].items[i]) == array_size(ll->items[i])) {
                  for (int j=0; j<array_size(osd_list[m].items[i]); j++) {
                     if (strcmp(osd_list[m].items[i][j], ll->items[i][j]) != 0 ) {
                        is_duplicate = 0;
                        break;
                     }
                  }
               } else {
                  is_duplicate = 0;
               }
               if (!is_duplicate)
                  break;
            }
            if (is_duplicate) {
               duplicates++;
               ignore[m] = 1;
            }
         }
      }

      /* Print title. */
      for (int i=0; i<array_size(ll->titlew); i++) {
         if ((duplicates > 0) && (i==array_size(ll->titlew)-1)) {
            snprintf( title, sizeof(title), "%s #b(%dx)#0", ll->titlew[i], duplicates+1 );
            gl_printMaxRaw( &gl_smallFont, w, x, p, NULL, -1., title);
         }
         else
            gl_printMaxRaw( &gl_smallFont, w, x, p, NULL, -1., ll->titlew[i]);
         p -= gl_smallFont.h + 5.;
         l++;
      }
      if (l >= osd_lines) {
         free(ignore);
         return;
      }

      /* Print items. */
      for (int i=ll->active; i<array_size(ll->items); i++) {
         const glColour *c = (i == (int)ll->active) ? &cFontWhite : &cFontGrey;
         x = osd_x;
         w = osd_w;
         for (int j=0; j<array_size(ll->items[i]); j++) {
            gl_printMaxRaw( &gl_smallFont, w, x, p,
                  c, -1., ll->items[i][j] );
            if (j==0) {
               w = osd_w - osd_hyphenLen;
               x = osd_x + osd_hyphenLen;
            }
            p -= gl_smallFont.h + 5.;
            l++;
            if (l >= osd_lines) {
               free(ignore);
               return;
            }
         }
      }
   }

   free(ignore);
}

/**
 * @brief Calculates and sets the length of the OSD.
 */
static void osd_calcDimensions (void)
{
   /* TODO decrease code duplication with osd_render */
   OSD_t *ll;
   double len;
   int *ignore;
   int nignore;
   int is_duplicate, duplicates;

   /* Nothing to render. */
   if (osd_list == NULL)
      return;

   nignore = array_size(osd_list);
   ignore  = calloc( nignore, sizeof( int ) );

   /* Render each thingy. */
   len = 0;
   for (int k=0; k<array_size(osd_list); k++) {
      if (ignore[k])
         continue;

      ll = &osd_list[k];

      /* Check how many duplicates we have, mark duplicates for ignoring */
      duplicates = 0;
      for (int m=k+1; m<array_size(osd_list); m++) {
         if ((strcmp(osd_list[m].title, ll->title) == 0) &&
               (array_size(osd_list[m].items) == array_size(ll->items)) &&
               (osd_list[m].active == ll->active)) {
            is_duplicate = 1;
            for (int i=osd_list[m].active; i<array_size(osd_list[m].items); i++) {
               if (array_size(osd_list[m].items[i]) == array_size(ll->items[i])) {
                  for (int j=0; j<array_size(osd_list[m].items[i]); j++) {
                     if (strcmp(osd_list[m].items[i][j], ll->items[i][j]) != 0 ) {
                        is_duplicate = 0;
                        break;
                     }
                  }
               } else {
                  is_duplicate = 0;
               }
               if (!is_duplicate)
                  break;
            }
            if (is_duplicate) {
               duplicates++;
               ignore[m] = 1;
            }
         }
      }

      /* Print title. */
      len += gl_smallFont.h + 5.;

      /* Print items. */
      for (int i=ll->active; i<array_size(ll->items); i++)
         for (int j=0; j<array_size(ll->items[i]); j++)
            len += gl_smallFont.h + 5.;
   }
   osd_rh = MIN( len, osd_h );
   free(ignore);
}

/**
 * @brief Gets the title of an OSD.
 *
 *    @param osd OSD to get title of.
 *    @return Title of the OSD.
 */
char *osd_getTitle( unsigned int osd )
{
   OSD_t *o = osd_get(osd);
   if (o == NULL)
      return NULL;

   return o->title;
}

/**
 * @brief Gets the items of an OSD.
 *
 *    @param osd OSD to get items of.
 *    @return Array (array.h) of OSD strings.
 */
char **osd_getItems( unsigned int osd )
{
   OSD_t *o = osd_get(osd);
   if (o == NULL)
      return NULL;
   return o->msg;
}
