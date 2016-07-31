// VBA-M, A Nintendo Handheld Console Emulator
// Copyright (C) 2008 VBA-M development team
//
// This program is free software; you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation; either version 2, or(at your option)
// any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program; if not, write to the Free Software Foundation,
// Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.

#ifndef VBAM_SDL_INPUT_H
#define VBAM_SDL_INPUT_H

#include "SDL.h"

enum EKey {
    KEY_LEFT,
    KEY_RIGHT,
    KEY_UP,
    KEY_DOWN,
    KEY_BUTTON_A,
    KEY_BUTTON_B,
    KEY_BUTTON_START,
    KEY_BUTTON_SELECT,
    KEY_BUTTON_L,
    KEY_BUTTON_R,
    KEY_BUTTON_SPEED,
    KEY_BUTTON_CAPTURE,
    KEY_BUTTON_AUTO_A,
    KEY_BUTTON_AUTO_B
};

enum EPad { PAD_MAIN,
    PAD_1 = PAD_MAIN,
    PAD_2,
    PAD_3,
    PAD_4,
    PAD_DEFAULT };

/**
 * Init the joysticks needed by the keymap. Verify that the keymap is compatible
 * with the joysticks. If it's not the case, revert to the default keymap.
 */
void inputInitJoysticks();

/**
 * Define which key controls an emulated joypad button
 * @param pad Emulated joypad index (there may be up to 4 joypads for the SGB)
 * @param key Emulated joypad button
 * @param code Code defining an actual joypad / keyboard button
 */
void inputSetKeymap(EPad pad, EKey key, uint32_t code);

/**
 * Get which key is associated to which emulated joypad button
 * @param pad Emulated joypad index (there may be up to 4 joypads for the SGB)
 * @param key Emulated joypad button
 * @retunr Code defining an actual joypad / keyboard button
 */
uint32_t inputGetKeymap(EPad pad, EKey key);

/**
 * Define which keys control motion detection emulation
 * @param key Emulated joypad button
 * @param code Code defining an actual joypad / keyboard button
 */
void inputSetMotionKeymap(EKey key, uint32_t code);

/**
 * Toggle Auto fire for the specified button. Only A, B, R, L are supported.
 * @param key Emulated joypad button
 * @return Auto fire enabled
 */
bool inputToggleAutoFire(EKey key);

/**
 * Get Auto fire status for the specified button. Only A, B, R, L are supported.
 * @param key Emulated joypad button
 * @return Auto fire enabled
 */
bool inputGetAutoFire(EKey key);

/**
 * Update the emulated pads state with a SDL event
 * @param SDL_Event An event that has just occured
 */
void inputProcessSDLEvent(const SDL_Event& event);

/**
 * Get the keymap code corresponding to a SDL event
 * @param SDL_Event An event that has just occured
 * @return Keymap code
 */
uint32_t inputGetEventCode(const SDL_Event& event);

/**
 * Read the state of an emulated joypad
 * @param which Emulated joypad index
 * @return Joypad state
 */
uint32_t inputReadJoypad(int which);

/**
 * Compute the motion sensor X and Y values
 */
void inputUpdateMotionSensor();

/**
 * Get the motion sensor X value
 * @return motion sensor X value
 */
int inputGetSensorX();

/**
 * Get the motion sensor Y value
 * @return motion sensor Y value
 */
int inputGetSensorY();

/**
 * Set which joypad configuration use when the core doesn't ask for a specific
 * @param pad Default pad
 */
void inputSetDefaultJoypad(EPad pad);

/**
 * Get the default joypad
 * pad
 */
EPad inputGetDefaultJoypad();

#endif // VBAM_SDL_INPUT_H
