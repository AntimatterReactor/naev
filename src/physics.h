/*
 * See Licensing and Copyright notice in naev.h
 */
#pragma once

#include "vec2.h"

/*
 * Update options.
 */
#define SOLID_UPDATE_RK4      0 /**< Default Runge-Kutta 3-4 update. */
#define SOLID_UPDATE_EULER    1 /**< Simple Euler update. */

/**
 * @brief Represents a solid in the game.
 */
typedef struct Solid_ {
   double mass; /**< Solid's mass. */
   double dir; /**< Direction solid is facing in rad. */
   double dir_vel; /**< Velocity at which solid is rotating in rad/s. */
   vec2 vel; /**< Velocity of the solid. */
   vec2 pos; /**< Position of the solid. */
   double thrust; /**< Relative X force, basically simplified for our thrust model. */
   double speed_max; /**< Maximum speed. */
   void (*update)( struct Solid_*, double ); /**< Update method. */
} Solid;

/*
 * solid manipulation
 */
double solid_maxspeed( const Solid *s, double speed, double thrust );
void solid_init( Solid* dest, double mass, double dir,
      const vec2* pos, const vec2* vel, int update );
Solid* solid_create( double mass, double dir,
      const vec2* pos, const vec2* vel, int update );
void solid_free( Solid* src );

/*
 * misc
 */
double angle_diff( double ref, double a );
