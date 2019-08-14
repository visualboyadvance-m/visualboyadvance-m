#ifndef LIBRETRO_CORE_OPTIONS_INTL_H__
#define LIBRETRO_CORE_OPTIONS_INTL_H__

#if defined(_MSC_VER) && (_MSC_VER >= 1500 && _MSC_VER < 1900)
/* https://support.microsoft.com/en-us/kb/980263 */
#pragma execution_character_set("utf-8")
#pragma warning(disable:4566)
#endif

#include <libretro.h>

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

/* RETRO_LANGUAGE_JAPANESE */

/* RETRO_LANGUAGE_FRENCH */

/* RETRO_LANGUAGE_SPANISH */

/* RETRO_LANGUAGE_GERMAN */

/* RETRO_LANGUAGE_ITALIAN */

/* RETRO_LANGUAGE_DUTCH */

/* RETRO_LANGUAGE_PORTUGUESE_BRAZIL */

/* RETRO_LANGUAGE_PORTUGUESE_PORTUGAL */

/* RETRO_LANGUAGE_RUSSIAN */

/* RETRO_LANGUAGE_KOREAN */

/* RETRO_LANGUAGE_CHINESE_TRADITIONAL */

/* RETRO_LANGUAGE_CHINESE_SIMPLIFIED */

/* RETRO_LANGUAGE_ESPERANTO */

/* RETRO_LANGUAGE_POLISH */

/* RETRO_LANGUAGE_VIETNAMESE */

/* RETRO_LANGUAGE_ARABIC */

/* RETRO_LANGUAGE_GREEK */

/* RETRO_LANGUAGE_TURKISH */

struct retro_core_option_definition option_defs_tr[] = {
    {
        "vbam_solarsensor",
        "Solar Sensör Seviyesi",
        "Boktai oyunlarında simüle güneş seviyesini ayarlar. L2 / R2 düğmeleri ayrıca seviyeleri hızlıca değiştirmek için de kullanılabilir.",
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
        "Resmi BIOS'u kullanın (Varsa)",
        "Mümkün olduğunda resmi BIOS kullanın. Değişikliklerin uygulanabilmesi için çekirdeğin yeniden başlatılması gerekiyor.",
        {
            { "disabled",  NULL },
            { "enabled",   NULL },
            { NULL, NULL },
        },
        "disabled"
    },
    {
        "vbam_forceRTCenable",
        "RTC'yi Etkinleştir",
        "RAM'den bağımsız olarak dahili gerçek zamanlı saati etkinleştirmeye zorlar. Saatin etkinleştirilmesini gerektiren rom yamalar için kullanılabilir (Pokemon gibi).",
        {
            { "disabled",  NULL },
            { "enabled",   NULL },
            { NULL, NULL },
        },
        "disabled"
    },
    {
        "vbam_soundinterpolation",
        "Ses Enterpolasyonu",
        "Ses filtresini etkinleştirin veya devre dışı bırakın.",
        {
            { "disabled",  NULL },
            { "enabled",   NULL },
            { NULL, NULL },
        },
        "enabled"
    },
    {
        "vbam_soundfiltering",
        "Ses Filtreleme",
        "Kullanılacak filtreleme miktarını ayarlar. Yüksek değer, yüksek frekansları azaltır.",
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
        "(GB) Renk Paleti",
        "Game Boy paletlerini kullanmak için ayarlayın.",
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
        "(GB) Öykünülmüş Donanım (Yeniden Başlatılması Gerekiyor)",
        "Game Boy donanım tipini taklit edecek şekilde ayarlar. Uygulamak için çekirdeği yeniden başlatın.",
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
        "(GB) Colorizer Hack'i Etkinleştir (Yeniden Başlatılması Gerekiyor)",
        "Colorizer'ın saldırıya uğramış GB oyunlarının (örn. DX yamalı oyunlar) normalde GBC / GBA donanım türünde çalışmasına izin verir. Bu ayrıca bios dosyasının kullanımını da devre dışı bırakır. Renklendirilmemiş oyunlarda kullanılması tavsiye edilmez.",
        {
            { "disabled",  NULL },
            { "enabled",   NULL },
            { NULL, NULL },
        },
        "disabled"
    },
    {
        "vbam_showborders",
        "(GB) Sınırları Göster",
        "Etkinleştirildiğinde, yüklü içerik SGB uyumluysa, bu durum oyundaki sınırı gösterir.",
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
        "(GB) Renk Düzeltme",
        "Bazı oyunlarda renkleri düzelten renk düzeltmesini uygular.",
        {
            { "disabled",  NULL },
            { "enabled",   NULL },
            { NULL, NULL },
        },
        "disabled"
    },
    {
        "vbam_turboenable",
        "Turbo Düğmelerini Etkinleştir",
        "Gamepad turbo düğmelerini etkinleştirin veya devre dışı bırakın.",
        {
            { "disabled", NULL },
            { "enabled",  NULL },
            { NULL, NULL },
        },
        "enabled"
    },
    {
        "vbam_turbodelay",
        "Turbo Gecikme (kare cinsinden)",
        "Karelerde turbo tetikleyicilerin oranını tekrarlayın. Daha yüksek değer daha fazla tetikler.",
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
        "Sensörlerin Ölü Bölgesi (%)",
        "Analog çubukların ölü bölgesini (yüzde olarak) ayarlayın.",
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
        "Sensör Hassasiyeti (Jiroskop) (%)",
        "Varsayılan konumlandırma, sol analogdur. Gyro özellikli oyunlar için hassasiyet seviyesini ayarlamak için kullanılır.",
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
        "Sensör Hassasiyeti (Eğim) (%)",
        "Varsayılan konumlandırma sağ analogdur. Gyro özellikli oyunlar için hassasiyet seviyesini ayarlamak için kullanılır.",
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
        "Sol / Sağ Analog Değiştirme",
        "Döndürme ve eğme için sola ve sağa analog çubuk işlevini değiştirir.",
        {
            { "disabled", NULL },
            { "enabled",  NULL },
            { NULL, NULL },
        },
        "disabled"
    },
    {
        "vbam_show_advanced_options",
        "Gelişmiş Ayarları Göster",
        "Ses kanallarını ve grafik katmanlarını etkinleştirebilen veya devre dışı bırakabilen gelişmiş seçenekleri gösterin.",
        {
            { "disabled", NULL },
            { "enabled",  NULL },
            { NULL, NULL },
        },
        "disabled"
    },
    {
        "vbam_sound_1",
        "Ses Kanalı 1",
        "Tonlu ve tarama ses kanalını etkinleştirir veya devre dışı bırakır.",
        {
            { "disabled", NULL },
            { "enabled",  NULL },
            { NULL, NULL },
        },
        "enabled"
    },
    {
        "vbam_sound_2",
        "Ses Kanalı 2",
        "Tonlu ses kanalını etkinleştirir veya devre dışı bırakır.",
        {
            { "disabled", NULL },
            { "enabled",  NULL },
            { NULL, NULL },
        },
        "enabled"
    },
    {
        "vbam_sound_3",
        "Ses Kanalı 3",
        "Dalga çıkışı ses kanalını etkinleştirir veya devre dışı bırakır.",
        {
            { "disabled", NULL },
            { "enabled",  NULL },
            { NULL, NULL },
        },
        "enabled"
    },
    {
        "vbam_sound_4",
        "Ses Kanalı 4",
        "Gürültü ses kanalını etkinleştirir veya devre dışı bırakır.",
        {
            { "disabled", NULL },
            { "enabled",  NULL },
            { NULL, NULL },
        },
        "enabled"
    },
    {
        "vbam_sound_5",
        "DMA Ses Kanalı A",
        "DMA ses kanalı A'yı etkinleştirir veya devre dışı bırakır.",
        {
            { "disabled", NULL },
            { "enabled",  NULL },
            { NULL, NULL },
        },
        "enabled"
    },
    {
        "vbam_sound_6",
        "DMA Ses Kanalı B",
        "DMA ses kanalı B'yi etkinleştirir veya devre dışı bırakır.",
        {
            { "disabled", NULL },
            { "enabled",  NULL },
            { NULL, NULL },
        },
        "enabled"
    },
    {
        "vbam_layer_1",
        "Arka Plan Katmanını Göster 1",
        "1. arka plan katmanını gösterir veya gizler.",
        {
            { "disabled", NULL },
            { "enabled",  NULL },
            { NULL, NULL },
        },
        "enabled"
    },
    {
        "vbam_layer_2",
        "Arka Plan Katmanını Göster 2",
        "2. arka plan katmanını gösterir veya gizler.",
        {
            { "disabled", NULL },
            { "enabled",  NULL },
            { NULL, NULL },
        },
        "enabled"
    },
    {
        "vbam_layer_3",
        "Arka Plan Katmanını Göster 3",
        "3. arka plan katmanını gösterir veya gizler.",
        {
            { "disabled", NULL },
            { "enabled",  NULL },
            { NULL, NULL },
        },
        "enabled"
    },
    {
        "vbam_layer_4",
        "Arka Plan Katmanını Göster 4",
        "4. arka plan katmanını gösterir veya gizler.",
        {
            { "disabled", NULL },
            { "enabled",  NULL },
            { NULL, NULL },
        },
        "enabled"
    },
    {
        "vbam_layer_5",
        "Sprite Katmanını Göster",
        "Sprite katmanını gösterir veya gizler.",
        {
            { "disabled", NULL },
            { "enabled",  NULL },
            { NULL, NULL },
        },
        "enabled"
    },
    {
        "vbam_layer_6",
        "Pencere Katmanını Göster 1",
        "Pencere katmanı 1'i gösterir veya gizler.",
        {
            { "disabled", NULL },
            { "enabled",  NULL },
            { NULL, NULL },
        },
        "enabled"
    },
    {
        "vbam_layer_7",
        "Pencere Katmanını Göster 2",
        "Pencere katmanı 2'yi gösterir veya gizler.",
        {
            { "disabled", NULL },
            { "enabled",  NULL },
            { NULL, NULL },
        },
        "enabled"
    },
    {
        "vbam_layer_8",
        "Sprite Pencere Katmanını Göster",
        "Sprite pencere katmanını gösterir veya gizler.",
        {
            { "disabled", NULL },
            { "enabled",  NULL },
            { NULL, NULL },
        },
        "enabled"
    },

    { NULL, NULL, NULL, {{0}}, NULL }
};

#ifdef __cplusplus
}
#endif

#endif