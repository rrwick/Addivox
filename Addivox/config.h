#ifndef ADDIVOX_DEMO
#define ADDIVOX_DEMO 0
#endif

#if ADDIVOX_DEMO
#define PLUG_NAME "AddivoxDemo"
#else
#define PLUG_NAME "Addivox"
#endif
#define PLUG_MFR "rrwick"
#define PLUG_VERSION_HEX 0x00010000
#define PLUG_VERSION_STR "1.0.0"
#if ADDIVOX_DEMO
#define PLUG_UNIQUE_ID 'AdvD'
#else
#define PLUG_UNIQUE_ID 'Advx'
#endif
#define PLUG_MFR_ID 'RWic'
#define PLUG_URL_STR "https://rrwick.github.io/Addivox"
#define PLUG_URL_DISPLAY_STR "rrwick.github.io/Addivox"
#define PLUG_EMAIL_STR "addivox.support@gmail.com"
#define PLUG_COPYRIGHT_STR "copyright 2026 Ryan Wick"
#define PLUG_CLASS_NAME Addivox

#if ADDIVOX_DEMO
#define BUNDLE_NAME "AddivoxDemo"
#else
#define BUNDLE_NAME "Addivox"
#endif
#define BUNDLE_MFR "rrwick"
#define BUNDLE_DOMAIN "com"
#if defined(OS_IOS)
#define APP_GROUP_ID "group.com.rrwick.Addivox"
#endif

#define PLUG_CHANNEL_IO "0-2"
#define SHARED_RESOURCES_SUBPATH "Addivox"

#define PLUG_LATENCY 0
#define PLUG_TYPE 1
#define PLUG_DOES_MIDI_IN 1
#define PLUG_DOES_MIDI_OUT 1
#define PLUG_DOES_MPE 0
#define PLUG_DOES_STATE_CHUNKS 1
#define PLUG_HAS_UI 1
#define PLUG_WIDTH 1000
#define PLUG_HEIGHT 750
#define PLUG_FPS 60
#define PLUG_SHARED_RESOURCES 0
#define PLUG_HOST_RESIZE 0

#define AUV2_ENTRY Addivox_Entry
#define AUV2_ENTRY_STR "Addivox_Entry"
#define AUV2_FACTORY Addivox_Factory
#define AUV2_VIEW_CLASS Addivox_View
#define AUV2_VIEW_CLASS_STR "Addivox_View"

#if ADDIVOX_DEMO
#define AAX_TYPE_IDS 'AvD1', 'AvD2'
#else
#define AAX_TYPE_IDS 'Avx1', 'Avx2'
#endif
#define AAX_PLUG_MFR_STR "RWic"
#if ADDIVOX_DEMO
#define AAX_PLUG_NAME_STR "AddivoxDemo\nAdvD"
#else
#define AAX_PLUG_NAME_STR "Addivox\nAdvx"
#endif
#define AAX_DOES_AUDIOSUITE 0
#define AAX_PLUG_CATEGORY_STR "Synth"

#define VST3_SUBCATEGORY "Instrument|Synth"
#define CLAP_MANUAL_URL "https://rrwick.github.io/Addivox"
#define CLAP_SUPPORT_URL "https://rrwick.github.io/Addivox/support_and_feedback"
#if ADDIVOX_DEMO
#define CLAP_DESCRIPTION "Addivox Demo"
#else
#define CLAP_DESCRIPTION "Addivox"
#endif
#define CLAP_FEATURES "instrument"//, "synth"

#define APP_NUM_CHANNELS 2
#define APP_N_VECTOR_WAIT 0
#define APP_MULT 1
#define APP_COPY_AUV3 0
#define APP_SIGNAL_VECTOR_SIZE 64

#define ROBOTO_FN "Roboto-Regular.ttf"
#define ROBOTO_BOLD_FN "Roboto-Bold.ttf"
#define ROBOTO_BLACK_FN "Roboto-Black.ttf"
#define BACKGROUND_FN "background.svg"
#define KNOB_FN "knob.svg"
#define KNOB_FIXED_FN "knob-fixed.svg"
#define KNOB_ROTATING_FN "knob-rotating.svg"
#define ABOUT_LOGO_FN "about-logo.svg"
#define GEAR_FN "gear.svg"
