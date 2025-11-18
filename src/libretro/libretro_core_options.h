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
 * VERSION: 2.0
 ********************************
 *
 * - 2.0: Add support for core options v2 interface
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

struct retro_core_option_v2_category option_cats_us[] = {
   {
      "system",
      "System",
      "Configure system options."
   },
   {
      "video",
      "Video",
      "Configure video options."
   },
   {
      "audio",
      "Audio",
      "Configure audio options."
   },
   {
      "input",
      "Input",
      "Configure input options."
   },
   {
      "advanced",
      "Advanced options",
      "Configure advanced options which can enable or disable sound channels and graphics layers."
   },
   { NULL, NULL, NULL },
};

struct retro_core_option_v2_definition option_defs_us[] = {
    {
        "vbam_usebios",
        "Use Official BIOS (If Available)",
        NULL,
        "Use official BIOS when available. Core needs to be restarted for changes to apply.",
        NULL,
        "system",
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
        NULL,
        "Forces the internal real-time clock to be enabled regardless of rom. Usuable for rom patches that requires clock to be enabled (aka Pokemon).",
        NULL,
        "system",
        {
            { "disabled",  NULL },
            { "enabled",   NULL },
            { NULL, NULL },
        },
        "disabled"
    },
    {
        "vbam_gbHardware",
        "(GB) Emulated Hardware (Needs Restart)",
        NULL,
        "Sets the Game Boy hardware type to emulate. Restart core to apply.",
        NULL,
        "system",
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
        NULL,
        "Allows Colorizer hacked GB games (e.g. DX patched games) to normally run in GBC/GBA hardware type. This also disables the use of bios file. NOT RECOMMENDED for use on non-colorized games.",
        NULL,
        "system",
        {
            { "disabled",  NULL },
            { "enabled",   NULL },
            { NULL, NULL },
        },
        "disabled"
    },
    {
        "vbam_palettes",
        "(GB) Color Palette",
        NULL,
        "Set Game Boy palettes to use.",
        NULL,
        "video",
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
        "vbam_showborders",
        "(GB) Show Borders",
        NULL,
        "When enabled, if loaded content is SGB compatible, this will show the border from the game if available.",
        NULL,
        "video",
        {
            { "disabled",  NULL },
            { "enabled",   NULL },
            { "auto",      NULL },
            { NULL, NULL },
        },
        "disabled"
    },
    {
        "vbam_lcdfilter",
        "LCD Color Filter",
        NULL,
        "Darkens the onscreen colors by applying a screen filter.",
        NULL,
        "video",
        {
            { "disabled",  NULL },
            { "enabled",   NULL },
            { NULL, NULL },
        },
        "disabled"
    },
    {
        "vbam_lcdfilter_type",
        "LCD Color Filter Type",
        NULL,
        "Screen filter type for onscreen colors.",
        NULL,
        "video",
        {
            { "sRGB",  NULL },
            { "DCI",   NULL },
            { "Rec2020", NULL },
            { NULL, NULL },
        },
        "sRGB"
    },
    {
        "vbam_color_change",
        "LCD Color Lighten/Darken",
        NULL,
        "Darken GBA or lighten GBC in %.",
        NULL,
        "video",
        {
            { "0",  NULL },
            { "5",  NULL },
            { "10",   NULL },
            { "15", NULL },
            { "20",   NULL },
            { "25", NULL },
            { "30",   NULL },
            { "35", NULL },
            { "40",   NULL },
            { "45", NULL },
            { "50",   NULL },
            { "55", NULL },
            { "60",   NULL },
            { "65", NULL },
            { "70",   NULL },
            { "75", NULL },
            { "80",   NULL },
            { "85", NULL },
            { "90",   NULL },
            { "95", NULL },
            { "100",   NULL },
            { NULL, NULL },
        },
        "0"
    },
    {
        "vbam_interframeblending",
        "Interframe Blending",
        NULL,
        "Simulates LCD ghosting effects. 'Smart' attempts to detect screen flickering, and only performs a 50:50 mix on affected pixels, while 'Motion Blur' mimics natural LCD response times by combining multiple buffered frames. 'Smart' blending is required when playing games that aggressively exploit LCD ghosting for transparency effects (Wave Race, Chikyuu Kaihou Gun ZAS, F-Zero, the Boktai series...).",
        NULL,
        "video",
        {
            { "disabled",  NULL },
            { "smart",   "Smart" },
            { "motion blur",   "Motion Blur" },
            { NULL, NULL },
        },
        "disabled"
    },
    {
        "vbam_soundinterpolation",
        "(GBA) Sound Interpolation",
        NULL,
        "Applies low-pass filtering to smooth high-frequency noise in PCM audio.",
        NULL,
        "audio",
        {
            { "disabled",  NULL },
            { "enabled",   NULL },
            { NULL, NULL },
        },
        "enabled"
    },
    {
        "vbam_soundfiltering",
        "(GBA) Sound Filtering",
        NULL,
        "Adjusts the intensity of the low-pass filter applied to audio (0 = none, 10 = maximum).",
        NULL,
        "audio",
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
        "vbam_gb_effects_enabled",
        "(GB) Sound Effects",
        NULL,
        "Enables or disabled all Game Boy sound effects.",
        NULL,
        "audio",
        {
            { "disabled",  NULL },
            { "enabled", NULL },
            { NULL, NULL },
        },
        "disabled"
    },
    {
        "vbam_gb_effects_echo_enabled",
        "(GB) Echo Effects",
        NULL,
        "Mix level for the reflected (echo) signal (0 = none, 10 = maximum).",
        NULL,
        "audio",
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
            { "10",  NULL },
            { NULL, NULL },
        },
        "2"
    },
    {
        "vbam_gb_effects_stereo_enabled",
        "(GB) Stereo Effects",
        NULL,
        "Adjusts stereo effect strength — higher values widen left/right placement (0 = none, 10 = maximum)",
        NULL,
        "audio",
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
            { "10",  NULL },
            { NULL, NULL },
        },
        "1"
    },
    {
        "vbam_gb_effects_surround_enabled",
        "(GB) Surround Effects",
        NULL,
        "Adds a surround effect, placing some channels behind the listener for a wider soundstage.",
        NULL,
        "audio",
        {
            { "disabled",  NULL },
            { "enabled", NULL },
            { NULL, NULL },
        },
        "disabled"
    },
    {
        "vbam_turboenable",
        "Enable Turbo Buttons",
        NULL,
        "Enable or disable gamepad turbo buttons.",
        NULL,
        "input",
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
        NULL,
        "Repeat rate of turbo triggers in frames. Higher value triggers more.",
        NULL,
        "input",
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
        "vbam_solarsensor",
        "Solar Sensor Level",
        NULL,
        "Adjusts simulated solar level in Boktai games. L2/R2 buttons can also be used to quickly change levels.",
        NULL,
        "input",
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
        "vbam_astick_deadzone",
        "Sensors Deadzone (%)",
        NULL,
        "Set deadzone (in percent) of analog sticks.",
        NULL,
        "input",
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
        NULL,
        "Default bind is left analog. Used to adjust sensitivity level for gyro-enabled games.",
        NULL,
        "input",
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
        NULL,
        "Default bind is right analog. Used to adjust sensitivity level for gyro-enabled games.",
        NULL,
        "input",
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
        NULL,
        "Swaps left and right analog stick function for gyro and tilt.",
        NULL,
        "input",
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
        NULL,
        "Enables or disables tone & sweep sound channel.",
        NULL,
        "advanced",
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
        NULL,
        "Enables or disables tone sound channel.",
        NULL,
        "advanced",
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
        NULL,
        "Enables or disables wave output sound channel.",
        NULL,
        "advanced",
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
        NULL,
        "Enables or disables noise audio channel.",
        NULL,
        "advanced",
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
        NULL,
        "Enables or disables DMA sound channel A.",
        NULL,
        "advanced",
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
        NULL,
        "Enables or disables DMA sound channel B.",
        NULL,
        "advanced",
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
        NULL,
        "Shows or hides background layer 1.",
        NULL,
        "advanced",
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
        NULL,
        "Shows or hides background layer 2.",
        NULL,
        "advanced",
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
        NULL,
        "Shows or hides background layer 3.",
        NULL,
        "advanced",
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
        NULL,
        "Shows or hides background layer 4.",
        NULL,
        "advanced",
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
        NULL,
        "Shows or hides sprite layer.",
        NULL,
        "advanced",
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
        NULL,
        "Shows or hides window layer 1.",
        NULL,
        "advanced",
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
        NULL,
        "Shows or hides window layer 2.",
        NULL,
        "advanced",
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
        NULL,
        "Shows or hides sprite window layer.",
        NULL,
        "advanced",
        {
            { "disabled", NULL },
            { "enabled",  NULL },
            { NULL, NULL },
        },
        "enabled"
    },
    { NULL, NULL, NULL, NULL, NULL, NULL, { { NULL, NULL } }, NULL },
};

struct retro_core_options_v2 options_us = {
   option_cats_us,
   option_defs_us
};

/*
 ********************************
 * Language Mapping
 ********************************
*/

#ifndef HAVE_NO_LANGEXTRA
struct retro_core_options_v2 *options_intl[RETRO_LANGUAGE_LAST] = {
   &options_us,    /* RETRO_LANGUAGE_ENGLISH */
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
   &options_tr,    /* RETRO_LANGUAGE_TURKISH */
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

static INLINE void libretro_set_core_options(retro_environment_t environ_cb,
      bool *categories_supported)
{
   unsigned version  = 0;
#ifndef HAVE_NO_LANGEXTRA
   unsigned language = 0;
#endif

   if (!environ_cb || !categories_supported)
      return;

   *categories_supported = false;

   if (!environ_cb(RETRO_ENVIRONMENT_GET_CORE_OPTIONS_VERSION, &version))
      version = 0;

   if (version >= 2)
   {
#ifndef HAVE_NO_LANGEXTRA
      struct retro_core_options_v2_intl core_options_intl;

      core_options_intl.us    = &options_us;
      core_options_intl.local = NULL;

      if (environ_cb(RETRO_ENVIRONMENT_GET_LANGUAGE, &language) &&
          (language < RETRO_LANGUAGE_LAST) && (language != RETRO_LANGUAGE_ENGLISH))
         core_options_intl.local = options_intl[language];

      *categories_supported = environ_cb(RETRO_ENVIRONMENT_SET_CORE_OPTIONS_V2_INTL,
            &core_options_intl);
#else
      *categories_supported = environ_cb(RETRO_ENVIRONMENT_SET_CORE_OPTIONS_V2,
            &options_us);
#endif
   }
   else
   {
      size_t i, j;
      size_t option_index              = 0;
      size_t num_options               = 0;
      struct retro_core_option_definition
            *option_v1_defs_us         = NULL;
#ifndef HAVE_NO_LANGEXTRA
      size_t num_options_intl          = 0;
      struct retro_core_option_v2_definition
            *option_defs_intl          = NULL;
      struct retro_core_option_definition
            *option_v1_defs_intl       = NULL;
      struct retro_core_options_intl
            core_options_v1_intl;
#endif
      struct retro_variable *variables = NULL;
      char **values_buf                = NULL;

      /* Determine total number of options */
      while (true)
      {
         if (option_defs_us[num_options].key)
            num_options++;
         else
            break;
      }

      if (version >= 1)
      {
         /* Allocate US array */
         option_v1_defs_us = (struct retro_core_option_definition *)
               calloc(num_options + 1, sizeof(struct retro_core_option_definition));

         /* Copy parameters from option_defs_us array */
         for (i = 0; i < num_options; i++)
         {
            struct retro_core_option_v2_definition *option_def_us = &option_defs_us[i];
            struct retro_core_option_value *option_values         = option_def_us->values;
            struct retro_core_option_definition *option_v1_def_us = &option_v1_defs_us[i];
            struct retro_core_option_value *option_v1_values      = option_v1_def_us->values;

            option_v1_def_us->key           = option_def_us->key;
            option_v1_def_us->desc          = option_def_us->desc;
            option_v1_def_us->info          = option_def_us->info;
            option_v1_def_us->default_value = option_def_us->default_value;

            /* Values must be copied individually... */
            while (option_values->value)
            {
               option_v1_values->value = option_values->value;
               option_v1_values->label = option_values->label;

               option_values++;
               option_v1_values++;
            }
         }

#ifndef HAVE_NO_LANGEXTRA
         if (environ_cb(RETRO_ENVIRONMENT_GET_LANGUAGE, &language) &&
             (language < RETRO_LANGUAGE_LAST) && (language != RETRO_LANGUAGE_ENGLISH) &&
             options_intl[language])
            option_defs_intl = options_intl[language]->definitions;

         if (option_defs_intl)
         {
            /* Determine number of intl options */
            while (true)
            {
               if (option_defs_intl[num_options_intl].key)
                  num_options_intl++;
               else
                  break;
            }

            /* Allocate intl array */
            option_v1_defs_intl = (struct retro_core_option_definition *)
                  calloc(num_options_intl + 1, sizeof(struct retro_core_option_definition));

            /* Copy parameters from option_defs_intl array */
            for (i = 0; i < num_options_intl; i++)
            {
               struct retro_core_option_v2_definition *option_def_intl = &option_defs_intl[i];
               struct retro_core_option_value *option_values           = option_def_intl->values;
               struct retro_core_option_definition *option_v1_def_intl = &option_v1_defs_intl[i];
               struct retro_core_option_value *option_v1_values        = option_v1_def_intl->values;

               option_v1_def_intl->key           = option_def_intl->key;
               option_v1_def_intl->desc          = option_def_intl->desc;
               option_v1_def_intl->info          = option_def_intl->info;
               option_v1_def_intl->default_value = option_def_intl->default_value;

               /* Values must be copied individually... */
               while (option_values->value)
               {
                  option_v1_values->value = option_values->value;
                  option_v1_values->label = option_values->label;

                  option_values++;
                  option_v1_values++;
               }
            }
         }

         core_options_v1_intl.us    = option_v1_defs_us;
         core_options_v1_intl.local = option_v1_defs_intl;

         environ_cb(RETRO_ENVIRONMENT_SET_CORE_OPTIONS_INTL, &core_options_v1_intl);
#else
         environ_cb(RETRO_ENVIRONMENT_SET_CORE_OPTIONS, option_v1_defs_us);
#endif
      }
      else
      {
         /* Allocate arrays */
         variables  = (struct retro_variable *)calloc(num_options + 1,
               sizeof(struct retro_variable));
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

            variables[option_index].key   = key;
            variables[option_index].value = values_buf[i];
            option_index++;
         }

         /* Set variables */
         environ_cb(RETRO_ENVIRONMENT_SET_VARIABLES, variables);
      }

error:
      /* Clean up */

      if (option_v1_defs_us)
      {
         free(option_v1_defs_us);
         option_v1_defs_us = NULL;
      }

#ifndef HAVE_NO_LANGEXTRA
      if (option_v1_defs_intl)
      {
         free(option_v1_defs_intl);
         option_v1_defs_intl = NULL;
      }
#endif

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
