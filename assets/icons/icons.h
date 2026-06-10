#pragma once
#include <stdint.h>
// 6-wide x 4-tall character icons for RitOS applications
// Each icon: 24 chars + 24 fg color bytes
// Colors: LightCyan(11) borders, varied content

struct IconDef {
    const char* data;    // 24 chars (6x4 grid, row-major)
    const uint8_t* fg;   // 24 foreground color indices
    const char* label;   // short label (max 6 chars)
};

// MONITOR/SYSMON: Screen with bar chart
static const char ICON_SYSMON_DATA[24] = {
    '\xC9','\xCD','\xCD','\xCD','\xCD','\xBB',  // ╔════╗
    '\xBA','\xB0','\xB1','\xB2','\xB1','\xBA',  // ║░▒▓▒║
    '\xC8','\xCD','\xC2','\xC4','\xC4','\xBC',  // ╚═╤──╝
    '\x20','\x20','\xC1','\xC4','\xC4','\x20'   //   ╧──
};
static const uint8_t ICON_SYSMON_FG[24] = {
    11,11,11,11,11,11,
    11,10,10,10,10,11,
    11,11,11,11,11,11,
     0, 0,11,11,11, 0
};

// CALCULATOR: Math symbols + digits display
static const char ICON_CALC_DATA[24] = {
    '\xC9','\xCD','\xCD','\xCD','\xCD','\xBB',
    '\xBA','\xF1','\xF6','\x2B','\x3D','\xBA',  // ║+-+=║
    '\xBA','\x37','\x38','\x39','\x30','\xBA',  // ║7890║
    '\xC8','\xCD','\xCD','\xCD','\xCD','\xBC'
};
static const uint8_t ICON_CALC_FG[24] = {
    11,11,11,11,11,11,
    11,14,14,14,14,11,
    11,14,14,14,14,11,
    11,11,11,11,11,11
};

// TEXT EDITOR: Document lines + cursor
static const char ICON_EDITOR_DATA[24] = {
    '\xC9','\xCD','\xCD','\xCD','\xCD','\xBB',
    '\xBA','\xC4','\xC4','\xC4','\xC4','\xBA',  // ║────║
    '\xBA','\xC4','\xC4','\xC4','\xDB','\xBA',  // ║───█║  (cursor)
    '\xC8','\xCD','\xCD','\xCD','\xCD','\xBC'
};
static const uint8_t ICON_EDITOR_FG[24] = {
    11,11,11,11,11,11,
    11, 7, 7, 7, 7,11,
    11, 7, 7, 7,15,11,
    11,11,11,11,11,11
};

// FILE EXPLORER: Grid of file diamonds
static const char ICON_FILES_DATA[24] = {
    '\xC9','\xCD','\xCD','\xCD','\xCD','\xBB',
    '\xBA','\x04','\x20','\x04','\x20','\xBA',  // ║diamond diamond║
    '\xBA','\x20','\x04','\x20','\x04','\xBA',  // ║ diamond diamond║
    '\xC8','\xCD','\xCD','\xCD','\xCD','\xBC'
};
static const uint8_t ICON_FILES_FG[24] = {
    11,11,11,11,11,11,
    11,14, 0,14, 0,11,
    11, 0,14, 0,14,11,
    11,11,11,11,11,11
};

// CLOCK: Circular face with hands
static const char ICON_CLOCK_DATA[24] = {
    '\xC9','\xCD','\xCD','\xCD','\xCD','\xBB',
    '\xBA','\x09','\x20','\x18','\x20','\xBA',  // ║○ up ║
    '\xBA','\x20','\x20','\x1A','\x20','\xBA',  // ║  right║
    '\xC8','\xCD','\xCD','\xCD','\xCD','\xBC'
};
static const uint8_t ICON_CLOCK_FG[24] = {
    11,11,11,11,11,11,
    11,13, 0,13, 0,11,
    11, 0, 0,13, 0,11,
    11,11,11,11,11,11
};

// CALENDAR: Date display with grid
static const char ICON_CAL_DATA[24] = {
    '\xC9','\xCD','\xCD','\xCD','\xCD','\xBB',
    '\xBA','\x33','\x31','\x20','\x20','\xBA',  // ║31  ║
    '\xBA','\xB0','\xB0','\xB0','\xB0','\xBA',  // ║░░░░║
    '\xC8','\xCD','\xCD','\xCD','\xCD','\xBC'
};
static const uint8_t ICON_CAL_FG[24] = {
    11,11,11,11,11,11,
    11,12,12, 0, 0,11,
    11,12,12,12,12,11,
    11,11,11,11,11,11
};

// SETTINGS: Gear/sun pattern
static const char ICON_SETTINGS_DATA[24] = {
    '\xC9','\xCD','\xCD','\xCD','\xCD','\xBB',
    '\xBA','\x0F','\x20','\x0F','\x20','\xBA',  // ║sun sun║
    '\xBA','\x20','\x0F','\x20','\x0F','\xBA',  // ║ sun sun║
    '\xC8','\xCD','\xCD','\xCD','\xCD','\xBC'
};
static const uint8_t ICON_SETTINGS_FG[24] = {
    11,11,11,11,11,11,
    11,13, 0,13, 0,11,
    11, 0,13, 0,13,11,
    11,11,11,11,11,11
};

// TERMINAL: Prompt display
static const char ICON_TERM_DATA[24] = {
    '\xC9','\xCD','\xCD','\xCD','\xCD','\xBB',
    '\xBA','\x3E','\x5F','\x20','\x20','\xBA',  // ║>_  ║
    '\xBA','\x20','\x20','\x20','\x20','\xBA',  // ║    ║
    '\xC8','\xCD','\xCD','\xCD','\xCD','\xBC'
};
static const uint8_t ICON_TERM_FG[24] = {
    11,11,11,11,11,11,
    11,10,10, 0, 0,11,
    11, 0, 0, 0, 0,11,
    11,11,11,11,11,11
};

// IMAGE VIEWER: Frame with picture content
static const char ICON_IMGVIEW_DATA[24] = {
    '\xC9','\xCD','\xCD','\xCD','\xCD','\xBB',
    '\xBA','\xB0','\xB2','\xB2','\xB0','\xBA',  // ║░▓▓░║
    '\xBA','\xB2','\xB1','\xB1','\xB2','\xBA',  // ║▓▒▒▓║
    '\xC8','\xCD','\xCD','\xCD','\xCD','\xBC'
};
static const uint8_t ICON_IMGVIEW_FG[24] = {
    11,11,11,11,11,11,
    11,10,10,12,10,11,
    11,13,14,14,13,11,
    11,11,11,11,11,11
};
