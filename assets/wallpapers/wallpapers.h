#pragma once
// Wallpaper pattern descriptors for RitOS

struct WallpaperDef {
    char code;         // single char code stored in settings.cfg
    const char* name;  // display name
    const char* desc;  // description
};

static const WallpaperDef WALLPAPERS[] = {
    {'P', "Picture",   "Half-block photo wallpaper"},
    {'G', "Grid",      "Subtle dot grid"},
    {'*', "Stars",     "Starfield pattern"},
    {'S', "Solid",     "Solid color"},
    {'C', "Checker",   "Checkerboard"},
    {'R', "Rain",      "Vertical rain lines"},
    {'W', "Wave",      "Wave pattern"},
    {'B', "Brick",     "Brick wall pattern"},
    {'N', "Network",   "Circuit board pattern"},
    {0, nullptr, nullptr}
};
