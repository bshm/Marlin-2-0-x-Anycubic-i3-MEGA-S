/**
 * Marlin 3D Printer Firmware
 * Copyright (c) 2020 MarlinFirmware [https://github.com/MarlinFirmware/Marlin]
 *
 * Based on Sprinter and grbl.
 * Copyright (c) 2011 Camiel Gubbels / Erik van der Zalm
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 */

#include "../../inc/MarlinConfig.h"

#if ENABLED(FANCY_BED_LEVELING)

#include "../gcode.h"
#include "../../module/delta.h"
#include "../../module/motion.h"
#include "../../module/stepper.h"
#include "../../module/endstops.h"
#include "../../lcd/ultralcd.h"

#if HAS_BED_PROBE
  #include "../../module/probe.h"
#endif

#if HAS_LEVELING
  #include "../../feature/bedlevel/bedlevel.h"
#endif

#if HAS_SERVOS
  #include "../../module/servo.h"
#endif

/** Logical Position of the 4 level knob wheels. */
enum FancyPoint { SOUTH_WEST = 0, NORTH_WEST = 1, NORTH_EAST = 2 , SOUTH_EAST = 3};

/** Physical position of the knob wheel */
constexpr xy_pos_t pos_nw = { 40.0f, 260.0f };
/** Physical position of the knob wheel */
constexpr xy_pos_t pos_ne = { 260.0f, 260.0f };
/** Physical position of the knob wheel */
constexpr xy_pos_t pos_se = { 260.0f, 40.0f };
/** Physical position of the knob wheel */
constexpr xy_pos_t pos_sw = { 40.0f, 40.0f };

/** Index of the servo controlling the knobs on the left side */
constexpr int servoIndexWest = 1;
/** Index of the servo controlling the knobs on the right side */
constexpr int servoIndexEast = 3;

/** Fancy bed leveling will finish after all z probes are within this range */
constexpr float z_max_deviation_mm = 0.015;

/** commanded positon of the servo to move the wheel */
constexpr int SERVO_UP_DEG = 90;

/** commanded positon of the servo below the wheel */
constexpr int SERVO_DOWN_DEG = 0;

/** How far position before/after knob move from knob center on y axis */
constexpr float KNOB_OFFSET_MM = 35.0f ;

/** Returns the coordinates for a given fancy leveling point */
static xy_pos_t FancyPoint2XY(FancyPoint fp);

/** Returns the servo index for a given fancy leveling point */
static int FancyPoint2ServoIndex(FancyPoint fp);

/** Returns the start/end coordinates to move to in order to move the knob.
 * @param lowerPos set to false,true to move the knob CCW, true,false to move CW
*/
static xy_pos_t getKnobMovePos(FancyPoint fp, bool lowerPos);

/** Moves the knob given by fp if z_distance_mm is greater than z_max_deviation_mm */
static bool move_knob_if_needed(FancyPoint fp, float z_distance_mm);

/** linear interpolation between two coordinates.
 * @param factor must be within [0.1,1.0] */
static xy_pos_t interpolate(xy_pos_t a, xy_pos_t b, float factor);

/** performs a leveling step at position fp */
static bool fancy_bed_leveling_at(FancyPoint fp);

/** defines the visiting order of the leveling point */
static FancyPoint getNextPoint(FancyPoint fp);

/**
 * G36 - fancy bed leveling
 */
void GcodeSuite::G36() {

  if(!all_axes_homed())
  {
    SERIAL_ECHOLNPGM("G36: needs homing");
    return;
  }

  int pointIndex = parser.intval('R', -1);
  if(pointIndex < -1 || pointIndex > 3)
  {
    SERIAL_ECHOLNPGM("G36: R index out of range");
    return;
  }

  if(current_position.z < Z_CLEARANCE_DEPLOY_PROBE)
  {
    SERIAL_ECHOLNPGM("G36: raising print head before first xy movement starts");
    do_blocking_move_to_z(Z_CLEARANCE_DEPLOY_PROBE);
  }

  if(pointIndex != -1)
  {
    FancyPoint fp = (FancyPoint)pointIndex;
    fancy_bed_leveling_at(fp);
  }
  else
  {
    int numberOfKnobsDone = 0;
    int step = 0;
    FancyPoint fp = SOUTH_EAST;
    while(numberOfKnobsDone < 4)
    {
      step += 1;
      SERIAL_ECHOLNPAIR("starting step: ", step);

      fp = getNextPoint(fp);
      const bool moved = fancy_bed_leveling_at(fp);
      if(moved)
      {
        numberOfKnobsDone = 0;
      }
      else
      {
        numberOfKnobsDone += 1;
      }
      SERIAL_ECHOLNPAIR("numberOfKnobsDone: ", numberOfKnobsDone);
    }
    SERIAL_ECHOLNPAIR("Done after steps: ", step);
  }


}



xy_pos_t FancyPoint2XY(FancyPoint fp)
{
  switch(fp)
  {
    case NORTH_WEST:
      return pos_nw;
    case NORTH_EAST:
      return pos_ne;
    case SOUTH_EAST:
      return pos_se;
    case SOUTH_WEST:
      return pos_sw;
  }
  return xy_pos_t();
}

int FancyPoint2ServoIndex(FancyPoint fp)
{
  switch(fp)
  {
    case NORTH_WEST:
      return servoIndexWest;
    case NORTH_EAST:
      return servoIndexEast;
    case SOUTH_EAST:
      return servoIndexEast;
    case SOUTH_WEST:
      return servoIndexWest;
  }
  return 0;
}


xy_pos_t getKnobMovePos(FancyPoint fp, bool lowerPos)
{
  xy_pos_t pos;
  
  pos = FancyPoint2XY(fp);
  
  if(fp == NORTH_EAST || fp == SOUTH_EAST)
  {
    lowerPos = !lowerPos;
  }


  if(lowerPos)
  {
    pos.y -= KNOB_OFFSET_MM; 
  }
  else
  { 
    pos.y += KNOB_OFFSET_MM; 
  }

  return pos;
}

bool move_knob_if_needed(FancyPoint fp, float z_distance_mm)
{
  const int servoIndex = FancyPoint2ServoIndex(fp);
  const bool direction = z_distance_mm > z_max_deviation_mm;
  const bool smallStep = fabs(z_distance_mm) < (2.0 * z_max_deviation_mm);

  if(fabs(z_distance_mm) > z_max_deviation_mm)
  {
    do_blocking_move_to(getKnobMovePos(fp, !direction));
    servo[servoIndex].move(SERVO_UP_DEG);
    if(smallStep)
    {
      do_blocking_move_to(interpolate(FancyPoint2XY(fp), getKnobMovePos(fp, direction), 0.6f));
    }
    else
    {
      do_blocking_move_to(getKnobMovePos(fp, direction));
    }
    servo[servoIndex].move(SERVO_DOWN_DEG);
    servo[servoIndex].detach();
    return true;
  }
  else
  {
      // within range, no action required
    return false;
  }
}

xy_pos_t interpolate(xy_pos_t a, xy_pos_t b, float factor)
{
  xy_pos_t result;
  result.x = a.x * factor + b.x * (1.0f - factor);
  result.y = a.y * factor + b.y * (1.0f - factor);
  return result;
}

bool fancy_bed_leveling_at(FancyPoint fp)
{
  bool moved = false;

  #if HAS_LEVELING
    set_bed_leveling_enabled(false);
  #endif

  remember_feedrate_scaling_off();


  const float measured_z = probe.probe_at_point(FancyPoint2XY(fp), PROBE_PT_STOW, 1);


  restore_feedrate_and_scaling();

  if(!isnanf(measured_z))
  {
    SERIAL_ECHOLNPAIR("Bed X: ", FIXFLOAT(current_position.x), " Y: ", FIXFLOAT(current_position.y), " Z: ", FIXFLOAT(measured_z));
    moved = move_knob_if_needed(fp, measured_z);
  }

  return moved;
}

FancyPoint getNextPoint(FancyPoint fp)
{
  switch(fp)
  {
    case NORTH_WEST:
      return NORTH_EAST;
    case NORTH_EAST:
      return SOUTH_EAST;
    case SOUTH_EAST:
      return SOUTH_WEST;
    case SOUTH_WEST:
      return NORTH_WEST;
  }
  return SOUTH_WEST;
}


#endif