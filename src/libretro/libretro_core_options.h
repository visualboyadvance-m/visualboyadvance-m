#ifndef LIBRETRO_CORE_OPTIONS_H__
#define LIBRETRO_CORE_OPTIONS_H__

#include <stdlib.h>
#include <string.h>

#include <libretro.h>
#include <retro_inline.h>

#ifndef HAVE_NO_LANGEXTRA
#include "libretro_core_options_intl.h"
#endif

/*
 ********************************
 * VERSION: 1.3
 ********************************
 *
 * - 1.3: Move translations to libretro_core_options_intl.h
 *        - libretro_core_options_intl.h includes BOM and utf-8
 *          fix for MSVC 2010-2013
 *        - Added HAVE_NO_LANGEXTRA flag to disable translations
 *          on platforms/compilers without BOM support
 * - 1.2: Use core options v1 interface when
 *        RETRO_ENVIRONMENT_GET_CORE_OPTIONS_VERSION is >= 1
 *        (previously required RETRO_ENVIRONMENT_GET_CORE_OPTIONS_VERSION == 1)
 * - 1.1: Support generation of core options v0 retro_core_option_value
 *        arrays containing options with a single value
 * - 1.0: First commit
*/

#ifdef __cplusplus
extern "C" {
#endif

/*
 ********************************
 * Core Option Definitions
 ********************************
*/

/* RETRO_LANGUAGE_ENGLISH */

/* Default language:
 * - All other languages must include the same keys and values
 * - Will be used as a fallback in the event that frontend language
 *   is not available
 * - Will be used as a fallback for any missing entries in
 *   frontend language definition */

struct retro_core_option_definition option_defs_us[] = {
    {
        "vbam_solarsensor",
        "Solar Sensor Level",
        "Adjusts simulated solar level in Boktai games. L2/R2 buttons can also be used to quickly change levels.",
        {
            { "0",  NULL },
            { "1",  NULL },
            { "2",  NULL },
            { "3",  NULL },
            { "4",  NULL },
            { "5",  NULL },
            { "6",  NULL },
            { "7",  NULL },
            { "8",  NULL },
            { "9",  NULL },
            { "10", NULL },
            { NULL, NULL },
        },
        "0"
    },
    {
        "vbam_usebios",
        "Use Official BIOS (If Available)",
        "Use official BIOS when available. Core needs to be restarted for changes to apply.",
        {
            { "disabled",  NULL },
            { "enabled",   NULL },
            { NULL, NULL },
        },
        "disabled"
    },
    {
        "vbam_forceRTCenable",
        "Force-Enable RTC",
        "Forces the internal real-time clock to be enabled regardless of rom. Usuable for rom patches that requires clock to be enabled (aka Pokemon).",
        {
            { "disabled",  NULL },
            { "enabled",   NULL },
            { NULL, NULL },
        },
        "disabled"
    },
    {
        "vbam_soundinterpolation",
        "Sound Interpolation",
        "Enable or disable sound filtering.",
        {
            { "disabled",  NULL },
            { "enabled",   NULL },
            { NULL, NULL },
        },
        "enabled"
    },
    {
        "vbam_soundfiltering",
        "Sound Filtering",
        "Sets the amount of filtering to use. Higher value reduces higher frequencies.",
        {
            { "0",  NULL },
            { "1",  NULL },
            { "2",  NULL },
            { "3",  NULL },
            { "4",  NULL },
            { "5",  NULL },
            { "6",  NULL },
            { "7",  NULL },
            { "8",  NULL },
            { "9",  NULL },
            { "10", NULL },
            { NULL, NULL },
        },
        "5"
    },
    {
        "vbam_palettes",
        "(GB) Color Palette",
        "Set Game Boy palettes to use.",
        {
            { "black and white", NULL },
            { "blue sea",     NULL },
            { "dark knight",  NULL },
            { "green forest", NULL },
            { "hot desert",   NULL },
            { "pink dreams",  NULL },
            { "wierd colors", NULL },
            { "original gameboy", NULL },
            { "gba sp",       NULL },
            { NULL, NULL },
        },
        "standard"
    },
    {
        "vbam_gbHardware",
        "(GB) Emulated Hardware (Needs Restart)",
        "Sets the Game Boy hardware type to emulate. Restart core to apply.",
        {
            { "gbc",  "Game Boy Color" },
            { "auto", "Automatic" },
            { "sgb",  "Super Game Boy" },
            { "gb",   "Game Boy" },
            { "gba",  "Game Boy Advance" },
            { "sgb2", "Super Game Boy 2" },
            { NULL, NULL },
        },
        "gbc"
    },
    {
        "vbam_allowcolorizerhack",
        "(GB) Enable Colorizer Hack (Needs Restart)",
        "Allows Colorizer hacked GB games (e.g. DX patched games) to normally run in GBC/GBA hardware type. This also disables the use of bios file. NOT RECOMMENDED for use on non-colorized games.",
        {
            { "disabled",  NULL },
            { "enabled",   NULL },
            { NULL, NULL },
        },
        "disabled"
    },
    {
        "vbam_showborders",
        "(GB) Show Borders",
        "When enabled, if loaded content is SGB compatible, this will show the border from the game if available.",
        {
            { "disabled",  NULL },
            { "enabled",   NULL },
            { "auto",      NULL },
            { NULL, NULL },
        },
        "disabled"
    },
    {
        "vbam_gbcoloroption",
        "(GB) Color Correction",
        "Applies color correction which fixes colors in some games.",
        {
            { "disabled",  NULL },
            { "enabled",   NULL },
            { NULL, NULL },
        },
        "disabled"
    },
    {
        "vbam_lcdfilter",
        "LCD Color Filter",
        "Darkens the onscreen colors by applying a screen filter.",
        {
            { "disabled",  NULL },
            { "enabled",   NULL },
            { NULL, NULL },
        },
        "disabled"
    },
    {
        "vbam_interframeblending",
        "Interframe Blending",
        "Simulates LCD ghosting effects. 'Smart' attempts to detect screen flickering, and only performs a 50:50 mix on affected pixels, while 'Motion Blur' mimics natural LCD response times by combining multiple buffered frames. 'Smart' blending is required when playing games that aggressively exploit LCD ghosting for transparency effects (Wave Race, Chikyuu Kaihou Gun ZAS, F-Zero, the Boktai series...).",
        {
            { "disabled",  NULL },
            { "smart",   "Smart" },
            { "motion blur",   "Motion Blur" },
            { NULL, NULL },
        },
        "disabled"
    },
    {
        "vbam_turboenable",
        "Enable Turbo Buttons",
        "Enable or disable gamepad turbo buttons.",
        {
            { "disabled", NULL },
            { "enabled",  NULL },
            { NULL, NULL },
        },
        "enabled"
    },
    {
        "vbam_turbodelay",
        "Turbo Delay (in frames)",
        "Repeat rate of turbo triggers in frames. Higher value triggers more.",
        {
            { "1",  NULL },
            { "2",  NULL },
            { "3",  NULL },
            { "4",  NULL },
            { "5",  NULL },
            { "6",  NULL },
            { "7",  NULL },
            { "8",  NULL },
            { "9",  NULL },
            { "10", NULL },
            { "11", NULL },
            { "12", NULL },
            { "13", NULL },
            { "14", NULL },
            { "15", NULL },
            { NULL, NULL },
        },
        "3"
    },
    {
        "vbam_astick_deadzone",
        "Sensors Deadzone (%)",
        "Set deadzone (in percent) of analog sticks.",
        {
            { "0",  NULL },
            { "5",  NULL },
            { "10", NULL },
            { "15", NULL },
            { "20", NULL },
            { "25", NULL },
            { "30", NULL },
            { NULL, NULL },
        },
        "15"
    },
    {
        "vbam_gyro_sensitivity",
        "Sensor Sensitivity (Gyroscope) (%)",
        "Default bind is left analog. Used to adjust sensitivity level for gyro-enabled games.",
        {
            { "10",  NULL },
            { "15",  NULL },
            { "20",  NULL },
            { "25",  NULL },
            { "30",  NULL },
            { "35",  NULL },
            { "40",  NULL },
            { "45",  NULL },
            { "50",  NULL },
            { "55",  NULL },
            { "60",  NULL },
            { "65",  NULL },
            { "70",  NULL },
            { "75",  NULL },
            { "80",  NULL },
            { "85",  NULL },
            { "90",  NULL },
            { "95",  NULL },
            { "100", NULL },
            { "105", NULL },
            { "110", NULL },
            { "115", NULL },
            { "120", NULL },
            { NULL, NULL },
        },
        "100"
    },
    {
        "vbam_tilt_sensitivity",
        "Sensor Sensitivity (Tilt) (%)",
        "Default bind is right analog. Used to adjust sensitivity level for gyro-enabled games.",
        {
            { "10",  NULL },
            { "15",  NULL },
            { "20",  NULL },
            { "25",  NULL },
            { "30",  NULL },
            { "35",  NULL },
            { "40",  NULL },
            { "45",  NULL },
            { "50",  NULL },
            { "55",  NULL },
            { "60",  NULL },
            { "65",  NULL },
            { "70",  NULL },
            { "75",  NULL },
            { "80",  NULL },
            { "85",  NULL },
            { "90",  NULL },
            { "95",  NULL },
            { "100", NULL },
            { "105", NULL },
            { "110", NULL },
            { "115", NULL },
            { "120", NULL },
            { NULL, NULL },
        },
        "100"
    },
    {
        "vbam_swap_astick",
        "Swap Left/Right Analog",
        "Swaps left and right analog stick function for gyro and tilt.",
        {
            { "disabled", NULL },
            { "enabled",  NULL },
            { NULL, NULL },
        },
        "disabled"
    },
    {
        "vbam_show_advanced_options",
        "Show Advanced Options",
        "Show advanced options which can enable or disable sound channels and graphics layers.",
        {
            { "disabled", NULL },
            { "enabled",  NULL },
            { NULL, NULL },
        },
        "disabled"
    },
    {
        "vbam_sound_1",
        "Sound Channel 1",
        "Enables or disables tone & sweep sound channel.",
        {
            { "disabled", NULL },
            { "enabled",  NULL },
            { NULL, NULL },
        },
        "enabled"
    },
    {
        "vbam_sound_2",
        "Sound Channel 2",
        "Enables or disables tone sound channel.",
        {
            { "disabled", NULL },
            { "enabled",  NULL },
            { NULL, NULL },
        },
        "enabled"
    },
    {
        "vbam_sound_3",
        "Sound Channel 3",
        "Enables or disables wave output sound channel.",
        {
            { "disabled", NULL },
            { "enabled",  NULL },
            { NULL, NULL },
        },
        "enabled"
    },
    {
        "vbam_sound_4",
        "Sound Channel 4",
        "Enables or disables noise audio channel.",
        {
            { "disabled", NULL },
            { "enabled",  NULL },
            { NULL, NULL },
        },
        "enabled"
    },
    {
        "vbam_sound_5",
        "Sound DMA Channel A",
        "Enables or disables DMA sound channel A.",
        {
            { "disabled", NULL },
            { "enabled",  NULL },
            { NULL, NULL },
        },
        "enabled"
    },
    {
        "vbam_sound_6",
        "Sound DMA Channel B",
        "Enables or disables DMA sound channel B.",
        {
            { "disabled", NULL },
            { "enabled",  NULL },
            { NULL, NULL },
        },
        "enabled"
    },
    {
        "vbam_layer_1",
        "Show Background Layer 1",
        "Shows or hides background layer 1.",
        {
            { "disabled", NULL },
            { "enabled",  NULL },
            { NULL, NULL },
        },
        "enabled"
    },
    {
        "vbam_layer_2",
        "Show Background Layer 2",
        "Shows or hides background layer 2.",
        {
            { "disabled", NULL },
            { "enabled",  NULL },
            { NULL, NULL },
        },
        "enabled"
    },
    {
        "vbam_layer_3",
        "Show Background Layer 3",
        "Shows or hides background layer 3.",
        {
            { "disabled", NULL },
            { "enabled",  NULL },
            { NULL, NULL },
        },
        "enabled"
    },
    {
        "vbam_layer_4",
        "Show Background Layer 4",
        "Shows or hides background layer 4.",
        {
            { "disabled", NULL },
            { "enabled",  NULL },
            { NULL, NULL },
        },
        "enabled"
    },
    {
        "vbam_layer_5",
        "Show Sprite Layer",
        "Shows or hides sprite layer.",
        {
            { "disabled", NULL },
            { "enabled",  NULL },
            { NULL, NULL },
        },
        "enabled"
    },
    {
        "vbam_layer_6",
        "Show Window Layer 1",
        "Shows or hides window layer 1.",
        {
            { "disabled", NULL },
            { "enabled",  NULL },
            { NULL, NULL },
        },
        "enabled"
    },
    {
        "vbam_layer_7",
        "Show Window Layer 2",
        "Shows or hides window layer 2.",
        {
            { "disabled", NULL },
            { "enabled",  NULL },
            { NULL, NULL },
        },
        "enabled"
    },
    {
        "vbam_layer_8",
        "Show Sprite Window Layer",
        "Shows or hides sprite window layer.",
        {
            { "disabled", NULL },
            { "enabled",  NULL },
            { NULL, NULL },
        },
        "enabled"
    },

    { NULL, NULL, NULL, {{0}}, NULL }
};

/*
 ********************************
 * Language Mapping
 ********************************
*/

#ifndef HAVE_NO_LANGEXTRA
struct retro_core_option_definition *option_defs_intl[RETRO_LANGUAGE_LAST] = {
   option_defs_us, /* RETRO_LANGUAGE_ENGLISH */
   NULL,           /* RETRO_LANGUAGE_JAPANESE */
   NULL,           /* RETRO_LANGUAGE_FRENCH */
   NULL,           /* RETRO_LANGUAGE_SPANISH */
   NULL,           /* RETRO_LANGUAGE_GERMAN */
   NULL,           /* RETRO_LANGUAGE_ITALIAN */
   NULL,           /* RETRO_LANGUAGE_DUTCH */
   NULL,           /* RETRO_LANGUAGE_PORTUGUESE_BRAZIL */
   NULL,           /* RETRO_LANGUAGE_PORTUGUESE_PORTUGAL */
   NULL,           /* RETRO_LANGUAGE_RUSSIAN */
   NULL,           /* RETRO_LANGUAGE_KOREAN */
   NULL,           /* RETRO_LANGUAGE_CHINESE_TRADITIONAL */
   NULL,           /* RETRO_LANGUAGE_CHINESE_SIMPLIFIED */
   NULL,           /* RETRO_LANGUAGE_ESPERANTO */
   NULL,           /* RETRO_LANGUAGE_POLISH */
   NULL,           /* RETRO_LANGUAGE_VIETNAMESE */
   NULL,           /* RETRO_LANGUAGE_ARABIC */
   NULL,           /* RETRO_LANGUAGE_GREEK */
   option_defs_tr, /* RETRO_LANGUAGE_TURKISH */
};
#endif

/*
 ********************************
 * Functions
 ********************************
*/

/* Handles configuration/setting of core options.
 * Should be called as early as possible - ideally inside
 * retro_set_environment(), and no later than retro_load_game()
 * > We place the function body in the header to avoid the
 *   necessity of adding more .c files (i.e. want this to
 *   be as painless as possible for core devs)
 */

static INLINE void libretro_set_core_options(retro_environment_t environ_cb)
{
   unsigned version = 0;

   if (!environ_cb)
      return;

   if (environ_cb(RETRO_ENVIRONMENT_GET_CORE_OPTIONS_VERSION, &version) && (version >= 1))
   {
#ifndef HAVE_NO_LANGEXTRA
      struct retro_core_options_intl core_options_intl;
      unsigned language = 0;

      core_options_intl.us    = option_defs_us;
      core_options_intl.local = NULL;

      if (environ_cb(RETRO_ENVIRONMENT_GET_LANGUAGE, &language) &&
          (language < RETRO_LANGUAGE_LAST) && (language != RETRO_LANGUAGE_ENGLISH))
         core_options_intl.local = option_defs_intl[language];

      environ_cb(RETRO_ENVIRONMENT_SET_CORE_OPTIONS_INTL, &core_options_intl);
#else
      environ_cb(RETRO_ENVIRONMENT_SET_CORE_OPTIONS, &option_defs_us);
#endif
   }
   else
   {
      size_t i;
      size_t num_options               = 0;
      struct retro_variable *variables = NULL;
      char **values_buf                = NULL;

      /* Determine number of options */
      while (true)
      {
         if (option_defs_us[num_options].key)
            num_options++;
         else
            break;
      }

      /* Allocate arrays */
      variables  = (struct retro_variable *)calloc(num_options + 1, sizeof(struct retro_variable));
      values_buf = (char **)calloc(num_options, sizeof(char *));

      if (!variables || !values_buf)
         goto error;

      /* Copy parameters from option_defs_us array */
      for (i = 0; i < num_options; i++)
      {
         const char *key                        = option_defs_us[i].key;
         const char *desc                       = option_defs_us[i].desc;
         const char *default_value              = option_defs_us[i].default_value;
         struct retro_core_option_value *values = option_defs_us[i].values;
         size_t buf_len                         = 3;
         size_t default_index                   = 0;

         values_buf[i] = NULL;

         if (desc)
         {
            size_t num_values = 0;

            /* Determine number of values */
            while (true)
            {
               if (values[num_values].value)
               {
                  /* Check if this is the default value */
                  if (default_value)
                     if (strcmp(values[num_values].value, default_value) == 0)
                        default_index = num_values;

                  buf_len += strlen(values[num_values].value);
                  num_values++;
               }
               else
                  break;
            }

            /* Build values string */
            if (num_values > 0)
            {
               size_t j;

               buf_len += num_values - 1;
               buf_len += strlen(desc);

               values_buf[i] = (char *)calloc(buf_len, sizeof(char));
               if (!values_buf[i])
                  goto error;

               strcpy(values_buf[i], desc);
               strcat(values_buf[i], "; ");

               /* Default value goes first */
               strcat(values_buf[i], values[default_index].value);

               /* Add remaining values */
               for (j = 0; j < num_values; j++)
               {
                  if (j != default_index)
                  {
                     strcat(values_buf[i], "|");
                     strcat(values_buf[i], values[j].value);
                  }
               }
            }
         }

         variables[i].key   = key;
         variables[i].value = values_buf[i];
      }

      /* Set variables */
      environ_cb(RETRO_ENVIRONMENT_SET_VARIABLES, variables);

error:

      /* Clean up */
      if (values_buf)
      {
         for (i = 0; i < num_options; i++)
         {
            if (values_buf[i])
            {
               free(values_buf[i]);
               values_buf[i] = NULL;
            }
         }

         free(values_buf);
         values_buf = NULL;
      }

      if (variables)
      {
         free(variables);
         variables = NULL;
      }
   }
}

#ifdef __cplusplus
}
#endif

#endif
