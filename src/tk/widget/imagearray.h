/*
 * See Licensing and Copyright notice in naev.h
 */



#ifndef WGT_IMAGEARRAY_H
#  define WGT_IMAGEARRAY_H


#include "opengl.h"
#include "font.h"
#include "colour.h"


/**
 * @brief The image array widget data.
 */
typedef struct WidgetImageArrayData_ {
   glTexture **images; /**< Image array. */
   char **captions; /**< Corresponding caption array. */
   char **alts; /**< Alt text when mouse over. */
   char **quantity; /**< Number in top-left corner. */
   int nelements; /**< Number of elements. */
   int xelem; /**< Number of horizontal elements. */
   int yelem; /**< Number of vertical elements. */
   int selected; /**< Currently selected element. */
   int alt; /**< Currently displaying alt text. */
   int altx; /**< Alt x position. */
   int alty; /**< Alt y position. */
   double pos; /**< Current y position. */
   int iw; /**< Image width to use. */
   int ih; /**< Image height to use. */
   void (*fptr) (unsigned int,char*); /**< Modify callback - triggered on selection. */
} WidgetImageArrayData;


/* Required functions. */
void window_addImageArray( const unsigned int wid,
      const int x, const int y, /* position */
      const int w, const int h, /* size */
      char* name, const int iw, const int ih, /* name and image sizes */
      glTexture** tex, char** caption, int nelem, /* elements */    
      void (*call) (unsigned int,char*) );

/* Misc functions. */
char* toolkit_getImageArray( const unsigned int wid, const char* name );
int toolkit_getImageArrayPos( const unsigned int wid, const char* name );
int toolkit_setImageArrayPos( const unsigned int wid, const char* name, int pos );
double toolkit_getImageArrayOffset( const unsigned int wid, const char* name );
int toolkit_setImageArrayOffset( const unsigned int wid, const char* name, double off );
int toolkit_setImageArrayAlt( const unsigned int wid, const char* name, char **alt );
int toolkit_setImageArrayQuantity( const unsigned int wid, const char* name,
      char **quantity );


#endif /* WGT_IMAGEARRAY_H */

