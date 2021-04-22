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

/**
 * G33 - Hijacked this command to implement fancy calibration
 */
void GcodeSuite::G33() {

(void) probe;
(void) &do_blocking_move_to_z;
(void) servo;

  SERIAL_ECHOLNPGM("G33: Test12");
  do_blocking_move_to_z(10);
  return;

}

  const xy_pos_t pos1 = { 40.0f, 40.0f };


#if 0


points = [[40,40]]
do{
    

  deviation = level1 && level2 && level3 && level4
    if(!deviation)
    {
        break;
    }





}

const float z_limit = 0.02;

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


void move_knob(point point, float z_distance)
{
    if(z_distance > z_limit)
    {
        //G1 X40 Y0   F4000
        //M280 P1 S90
        //G1 X40 Y300 F4000
        //M280 P1 S0
    }
    else if(z_distance < z_limit)
    {

    }   
    else
    {
        // within range, no action required
    }
}
#endif


#endif