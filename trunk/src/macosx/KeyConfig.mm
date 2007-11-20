#include <stdio.h>
#include <stdlib.h>
#include <SDL.h>
#include "MainClass.h"

int stopPoll = 0;

#define GETHATCODE(event) ((event.jhat.which+1)<<12)|(event.jhat.hat<<2)|\
  (event.jhat.value & SDL_HAT_UP ? 0 : event.jhat.value & SDL_HAT_DOWN ? 1 : event.jhat.value & SDL_HAT_RIGHT ? 2 : event.jhat.value & SDL_HAT_LEFT ? 3 : 0)

#define GETBUTTONCODE(event) ((event.jbutton.which+1)<<12)|(event.jbutton.button+0x80)

#define GETAXISCODE(event) ((event.jaxis.which+1)<<12)|(event.jaxis.axis<<1)|(event.jaxis.value > 16384 ? 1 : event.jaxis.value < -16384 ? 0 : 0)

void calibrate( void )
{
  SDL_Event event;
  while(SDL_PollEvent(&event)) {
    switch(event.type) {
    case SDL_JOYHATMOTION:
      break;
    case SDL_JOYBUTTONDOWN:
      //stopPoll = 1;
      break;
    case SDL_JOYAXISMOTION:
      break;
    case SDL_KEYDOWN:
      stopPoll = 1;
      break;
    case SDL_KEYUP:
      break;
    }
  }
}

int poll()
{
  SDL_Event event;
  while(SDL_PollEvent(&event)) {
    switch(event.type) {
    case SDL_JOYHATMOTION:
      if (event.jhat.value != 0)
      {
      stopPoll = 1;
      return GETHATCODE(event);
      }
      break;
    case SDL_JOYBUTTONDOWN:
      stopPoll = 1;
      return GETBUTTONCODE(event);
      break;
    case SDL_JOYAXISMOTION:
      if (event.jaxis.value < -23000 || event.jaxis.value > 23000)
        {
        stopPoll = 1;
        return GETAXISCODE(event);
        }
      break;
    case SDL_KEYDOWN:
      stopPoll = 1;
      return event.key.keysym.sym;
      break;
    case SDL_KEYUP:
      break;
    case SDL_MOUSEBUTTONDOWN:
      stopPoll = 1;
      break;
    }
  }
  return 0;
}

int config_main()
{

  if(SDL_Init(SDL_INIT_TIMER|SDL_INIT_VIDEO|SDL_INIT_NOPARACHUTE)) {
    printf("Failed to init SDL: %s\n", SDL_GetError());
    exit(-1);
  }

  if(SDL_InitSubSystem(SDL_INIT_JOYSTICK)) {
    printf("Failed to init joystick: %s\n", SDL_GetError());
  }

//  SDL_Surface *surface = SDL_SetVideoMode(0,0,0,
//                                          SDL_ANYFORMAT);
//  if (NULL == surface) {
//     printf("Failed to set video mode: %s\n", SDL_GetError());
//  }

  int numJoy = SDL_NumJoysticks();

  int i;
  for(i = 0; i < numJoy; i++) {
    SDL_JoystickOpen(i);
  }
  SDL_JoystickEventState(SDL_ENABLE);

  /*printf("%i joysticks were found.\n\n", SDL_NumJoysticks() );
  printf("The names of the joysticks are:\n");

    for( i=0; i < SDL_NumJoysticks(); i++ )
    {
        printf("    %s\n", SDL_JoystickName(i));
    }*/

    return SDL_NumJoysticks();
}
