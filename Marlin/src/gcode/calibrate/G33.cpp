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

#if ENABLED(DELTA_AUTO_CALIBRATION)

#include "../gcode.h"
#include "../../module/delta.h"
#include "../../module/motion.h"
#include "../../module/stepper.h"
#include "../../module/endstops.h"
#include "../../lcd/ultralcd.h"

#if HAS_BED_PROBE
  #include "../../module/probe.h"
#endif

#if HOTENDS > 1
  #include "../../module/tool_change.h"
#endif

#if HAS_LEVELING
  #include "../../feature/bedlevel/bedlevel.h"
#endif


#include "../../module/servo.h"


enum FancyPoint { SOUTH_WEST = 0, NORTH_WEST = 1, NORTH_EAST = 2 , SOUTH_EAST = 3};
const xy_pos_t pos_nw = { 40.0f, 260.0f };
const xy_pos_t pos_ne = { 260.0f, 260.0f };
const xy_pos_t pos_se = { 260.0f, 40.0f };
const xy_pos_t pos_sw = { 40.0f, 40.0f };
const int servoIndexWest = 1;
const int servoIndexEast = 3;
const float z_max_deviation = 0.015;
const int SERVO_UP = 90;
const int SERVO_DOWN = 0;
const float KNOB_OFFSET = 30.0f ;//mm distance between knob center and start/end of movement

static xy_pos_t FancyPoint2XY(FancyPoint fp);
static int FancyPoint2ServoIndex(FancyPoint fp);
static xy_pos_t getKnobMovePos(FancyPoint fp, bool lowerPos);
static bool move_knob_if_needed(FancyPoint fp, float z_distance);
static xy_pos_t interpolate(xy_pos_t a, xy_pos_t b, float factor);
static bool fancy_bed_leveling_at(FancyPoint fp);
static FancyPoint getNextPoint(FancyPoint fp);

/**
 * G33 - Hijacked this command to implement fancy calibration
 */
void GcodeSuite::G33() {

  if(!all_axes_homed())
  {
    SERIAL_ECHOLNPGM("G33: needs homing");
    return;
  }

  int pointIndex = parser.intval('R', -1);
  if(pointIndex < -1 || pointIndex > 3)
  {
    return;
  }


  if(current_position.z < Z_CLEARANCE_DEPLOY_PROBE)
  {
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
    int step = 1;
    FancyPoint fp = SOUTH_WEST;
    while(numberOfKnobsDone < 4)
    {
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
      step += 1;
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
    pos.y -= KNOB_OFFSET; 
  }
  else
  { 
    pos.y += KNOB_OFFSET; 
  }

  return pos;
}

bool move_knob_if_needed(FancyPoint fp, float z_distance)
{
  const int servoIndex = FancyPoint2ServoIndex(fp);
  const bool direction = z_distance > z_max_deviation;
  const bool smallStep = fabs(z_distance) < (2.0 * z_max_deviation);

  if(fabs(z_distance) > z_max_deviation)
  {
    do_blocking_move_to(getKnobMovePos(fp, !direction));
    servo[servoIndex].move(SERVO_UP);
    if(smallStep)
    {
      do_blocking_move_to(interpolate(FancyPoint2XY(fp), getKnobMovePos(fp, direction), 0.6f));
    }
    else
    {
      do_blocking_move_to(getKnobMovePos(fp, direction));
    }
    servo[servoIndex].move(SERVO_DOWN);
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