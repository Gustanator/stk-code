//
//  SuperTuxKart - a fun racing game with go-kart
//  Copyright (C) 2026 Gustavo Barreira
//
//  This program is free software; you can redistribute it and/or
//  modify it under the terms of the GNU General Public License
//  as published by the Free Software Foundation; either version 3
//  of the License, or (at your option) any later version.
//
//  This program is distributed in the hope that it will be useful,
//  but WITHOUT ANY WARRANTY; without even the implied warranty of
//  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
//  GNU General Public License for more details.
//
//  You should have received a copy of the GNU General Public License
//  along with this program; if not, write to the Free Software
//  Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.

#ifndef HEADER_RACE_GUI_EDITOR_HPP
#define HEADER_RACE_GUI_EDITOR_HPP

#include <map>
#include <string>
#include <vector>

#include <irrString.h>
#include <IrrlichtDevice.h>
using namespace irr;

class AbstractKart;
class InputMap;
class Material;
class RaceSetup;

/**
  * \brief Handles the editor of the in-race GUI
  * \ingroup states_screens
  */
class RaceGUIEditor
{
protected:
    // Desktop GUI related variables
    // ------------------------------------
    /** The position of the nitrometer in the screen. */
    core::vector2df     m_nitrometer_pos;

    /** The radius of the nitrometer in the screen. */
    int                 m_nitrometer_rad;

    /** The position of the speedometer in the screen. */
    core::vector2df     m_speedometer_pos;

    /** The radius of the speedometer in the screen. */
    int                 m_speedometer_rad;


public:

         RaceGUIEditor();;
};   // RaceGUIEditor

#endif
