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
// Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.

#ifndef VBAM_SDL_INPUT_H
#define VBAM_SDL_INPUT_H

#include <SDL.h>

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

/**
 * Init the joysticks needed by the keymap. Verify that the keymap is compatible
 * with the joysticks. If it's not the case, revert to the default keymap.
 */
void inputInitJoysticks();

/**
 * Define which key controls an emulated joypad button
 * @param joy Emulated joypad index (there may be up to 4 joypads for the SGB)
 * @param key Emulated joypad button
 * @param code Code defining an actual joypad / keyboard button
 */
void inputSetKeymap(int joy, EKey key, uint16_t code);

/**
 * Define which keys control motion detection emulation
 * @param key Emulated joypad button
 * @param code Code defining an actual joypad / keyboard button
 */
void inputSetMotionKeymap(EKey key, uint16_t code);

/**
 * Toggle Auto fire for the specified button. Only A, B, R, L are supported.
 * @param key Emulated joypad button
 * @return Auto fire enabled 
 */
bool inputToggleAutoFire(EKey key);

/**
 * Update the emulated pads state with a SDL event
 * @param SDL_Event An event that has just occured
 */
void inputProcessSDLEvent(const SDL_Event &event);

/**
 * Get the keymap code corresponding to a SDL event
 * @param SDL_Event An event that has just occured
 * @return Keymap code
 */
uint16_t inputGetEventCode(const SDL_Event &event);

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

#endif // VBAM_SDL_INPUT_H
