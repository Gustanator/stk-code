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

#include "states_screens/race_gui_editor.hpp"

using namespace irr;

#include <algorithm>
#include <limits>

#include "config/user_config.hpp"
#include "font/font_drawer.hpp"
#include "graphics/irr_driver.hpp"
#include "io/file_manager.hpp"
#include "modes/linear_world.hpp"
#include "modes/world.hpp"
#include "race/race_manager.hpp"
#include "states_screens/race_gui_multitouch.hpp"
#include "tracks/track.hpp"

#include <algorithm>

#include <IrrlichtDevice.h>

RaceGUIEditor::RaceGUIEditor() {
    // Attribute the variables of the nitrometer in the screen.
    m_nitrometer_rad   = 94;
    m_nitrometer_pos.X = 9.5f;
    m_nitrometer_pos.Y = 11.5f;

    // Attribute the variables of the speedometer in the screen.
    m_speedometer_rad   = 128;
    m_speedometer_pos.X = 24.0f;
    m_speedometer_pos.Y = 10.0f;
}
