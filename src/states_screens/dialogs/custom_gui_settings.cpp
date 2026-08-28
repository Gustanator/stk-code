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

#include "states_screens/dialogs/custom_gui_settings.hpp"

#include "config/user_config.hpp"
#include "guiengine/widgets/label_widget.hpp"
#include "guiengine/widgets/ribbon_widget.hpp"
#include "guiengine/widgets/spinner_widget.hpp"
#include "states_screens/state_manager.hpp"
#include "utils/string_utils.hpp"
#include "utils/translation.hpp"

#include <IGUIEnvironment.h>

// -----------------------------------------------------------------------------

CustomGuiSettingsDialog::CustomGuiSettingsDialog(const float w, const float h) :
        ModalDialog(w, h)
{
    m_self_destroy = false;
    loadFromFile("custom_gui_settings.stkgui");
}

// -----------------------------------------------------------------------------

CustomGuiSettingsDialog::~CustomGuiSettingsDialog()
{
}


// -----------------------------------------------------------------------------

void CustomGuiSettingsDialog::beforeAddingWidgets()
{
#ifndef SERVER_ONLY
    getWidget<LabelWidget>("editor_label")->setText(_("Edit your GUI"), false);

    SpinnerWidget* edit_selection = getWidget<SpinnerWidget>("edit_selection");
    assert(edit_selection != NULL);
    edit_selection->addLabel(_("Steering"));
    edit_selection->addLabel(_("Control buttons"));
    edit_selection->setValue(UserConfigParams::m_edit_selection);

    getWidget<SpinnerWidget>("scale")->setRange(0.5f, 1.5f, 0.05f);
    getWidget<SpinnerWidget>("position_x")->setRange(0.0f, 10.0f, 0.1f);
    getWidget<SpinnerWidget>("position_y")->setRange(0.0f, 10.0f, 0.1f);
    if (UserConfigParams::m_edit_selection == 1) // Steering editing
    {
        getWidget<SpinnerWidget>("scale")->setFloatValue(UserConfigParams::m_steering_btn_scale);
        getWidget<SpinnerWidget>("position_x")->setFloatValue(UserConfigParams::m_steering_btn_pos_x);
        getWidget<SpinnerWidget>("position_y")->setFloatValue(UserConfigParams::m_steering_btn_pos_y);
    }
    else // Control buttons editing
    {
        getWidget<SpinnerWidget>("scale")->setFloatValue(UserConfigParams::m_control_btn_scale);
        getWidget<SpinnerWidget>("position_x")->setFloatValue(UserConfigParams::m_control_btn_pos_x);
        getWidget<SpinnerWidget>("position_y")->setFloatValue(UserConfigParams::m_control_btn_pos_y);
    }
#endif
}

// -----------------------------------------------------------------------------

GUIEngine::EventPropagation CustomGuiSettingsDialog::processEvent(const std::string& eventSource)
{
#ifndef SERVER_ONLY
    if (eventSource == "buttons")
    {
        const std::string& selection = getWidget<RibbonWidget>("buttons")->
                                    getSelectionIDString(PLAYER_ID_GAME_MASTER);

        if (selection == "apply")
        {
            UserConfigParams::m_camera_fov = getWidget<SpinnerWidget>("fov")->getValue();
            UserConfigParams::m_camera_distance = getWidget<SpinnerWidget>("camera_distance")->getFloatValue();
            UserConfigParams::m_camera_forward_up_angle = getWidget<SpinnerWidget>("camera_angle")->getValue();
            UserConfigParams::m_camera_backward_distance = getWidget<SpinnerWidget>("backward_camera_distance")->getFloatValue();
            UserConfigParams::m_camera_backward_up_angle = getWidget<SpinnerWidget>("backward_camera_angle")->getValue();
            UserConfigParams::m_camera_forward_smooth_position = getWidget<SpinnerWidget>("smooth_position")->getFloatValue();
            UserConfigParams::m_camera_forward_smooth_rotation = getWidget<SpinnerWidget>("smooth_rotation")->getFloatValue();
            UserConfigParams::m_reverse_look_use_soccer_cam = getWidget<CheckBoxWidget>("use_soccer_camera")->getState();

            if (UserConfigParams::m_edit_selection == 1) // Steering editing
            {
                UserConfigParams::m_steering_btn_scale = getWidget<SpinnerWidget>("scale")->getFloatValue()
                UserConfigParams::m_steering_btn_pos_x = getWidget<SpinnerWidget>("position_x")->getFloatValue()
                UserConfigParams::m_steering_btn_pos_y = getWidget<SpinnerWidget>("position_y")->getFloatValue()
            }
            else // Control buttons editing
            {
                UserConfigParams::m_control_btn_scale = getWidget<SpinnerWidget>("scale")->getFloatValue()
                UserConfigParams::m_control_btn_pos_x = getWidget<SpinnerWidget>("position_x")->getFloatValue()
                UserConfigParams::m_control_btn_pos_y = getWidget<SpinnerWidget>("position_y")->getFloatValue()
            }
            m_self_destroy = true;
            return GUIEngine::EVENT_BLOCK;
        }
        else if (selection == "reset") // discard all the changes
        {
            if (UserConfigParams::m_edit_selection == 1) // Standard camera
            {
                UserConfigParams::m_steering_btn_scale = 1.0f;
                UserConfigParams::m_steering_btn_pos_x = 15.0f; // Place holder
                UserConfigParams::m_steering_btn_pos_y = 12.of; // Place holder
            }
            else if (UserConfigParams::m_camera_present == 2) // Drone chase camera
            {
                UserConfigParams::m_control_btn_scale = 1.0f;
                UserConfigParams::m_control_btn_pos_x = 15.0f; // Place holder
                UserConfigParams::m_control_btn_pos_y = 12.of; // Place holder
            }
        }
        else if (selection == "cancel")
        {
            ModalDialog::dismiss();
            return GUIEngine::EVENT_BLOCK;
        }
    }
#endif
    return GUIEngine::EVENT_LET;
}   // processEvent