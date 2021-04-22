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


enum FancyPoint { NORTH_WEST, NORTH_EAST, SOUTH_EAST, SOUTH_WEST};
const xy_pos_t pos_nw = { 40.0f, 260.0f };
const xy_pos_t pos_ne = { 260.0f, 260.0f };
const xy_pos_t pos_se = { 260.0f, 40.0f };
const xy_pos_t pos_sw = { 40.0f, 40.0f };
const int servoIndexWest = 1;
const int servoIndexEast = 3;
const float z_max_deviation = 0.02;
const int SERVO_UP = 90;
const int SERVO_DOWN = 0;
const float KNOB_OFFSET = 30.0f ;//mm distance between knob center and start/end of movement

static xy_pos_t FancyPoint2XY(FancyPoint fp);
static int FancyPoint2ServoIndex(FancyPoint fp);
static xy_pos_t getKnobMovePos(FancyPoint fp, bool lowerPos);
static void move_knob_if_needed(FancyPoint fp, float z_distance);

/**
 * G33 - Hijacked this command to implement fancy calibration
 */
void GcodeSuite::G33() {


  SERIAL_ECHOLNPGM("G33: Test12");
  do_blocking_move_to_z(10);


  #if HAS_LEVELING
    set_bed_leveling_enabled(false);
  #endif

  remember_feedrate_scaling_off();


  FancyPoint fp = SOUTH_WEST;
  const float measured_z = probe.probe_at_point(FancyPoint2XY(fp), PROBE_PT_STOW, 1);


  restore_feedrate_and_scaling();

  if(!isnanf(measured_z))
  {
    SERIAL_ECHOLNPAIR("Bed X: ", FIXFLOAT(current_position.x), " Y: ", FIXFLOAT(current_position.y), " Z: ", FIXFLOAT(measured_z));
    move_knob_if_needed(fp, measured_z);
  }

  return;

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


void move_knob_if_needed(FancyPoint fp, float z_distance)
{
  const int servoIndex = FancyPoint2ServoIndex(fp);
    if(z_distance > z_max_deviation)
    {
      do_blocking_move_to(getKnobMovePos(fp, true));
      servo[servoIndex].move(SERVO_UP);
      do_blocking_move_to(getKnobMovePos(fp, false));
      servo[servoIndex].move(SERVO_DOWN);
    }
    else if(z_distance < -z_max_deviation)
    {
      do_blocking_move_to(getKnobMovePos(fp, false));
      servo[servoIndex].move(SERVO_UP);
      do_blocking_move_to(getKnobMovePos(fp, true));
      servo[servoIndex].move(SERVO_DOWN);
    }   
    else
    {
        // within range, no action required
    }
}

#if 0


do{
    

  deviation = level1 && level2 && level3 && level4
    if(!deviation)
    {
        break;
    }





}



void level(point, bool* probed, bool* leveled)
{
    int tries = 0;
    do
    {
        const float z_distance = probe(point);
        *probed = true;
        move_knob(point, z_distance)
        tries += 1;
    }
    while(fabs(distance) > z_limit || tries > 10)
    *leveled = fabs(distance) <= z_limit;
}



#endif


#endif