#pragma once

#include "roo_display/color/color.h"

/// Defines 140 opaque HTML named colors.
///
/// See https://www.w3schools.com/colors/colors_groups.asp.

namespace roo_display {

namespace color {

// Pink colors.

static constexpr auto Pink = Color(0xFF, 0xC0, 0xCB);
static constexpr auto LightPink = Color(0xFF, 0xB6, 0xC1);
static constexpr auto HotPink = Color(0xFF, 0x69, 0xB4);
static constexpr auto DeepPink = Color(0xFF, 0x14, 0x93);
static constexpr auto PaleVioletRed = Color(0xDB, 0x70, 0x93);
static constexpr auto MediumVioletRed = Color(0xC7, 0x15, 0x85);

// Red colors.

static constexpr auto LightSalmon = Color(0xFF, 0xA0, 0x7A);
static constexpr auto Salmon = Color(0xFA, 0x80, 0x72);
static constexpr auto DarkSalmon = Color(0xE9, 0x96, 0x7A);
static constexpr auto LightCoral = Color(0xF0, 0x80, 0x80);
static constexpr auto IndianRed = Color(0xCD, 0x5C, 0x5C);
static constexpr auto Crimson = Color(0xDC, 0x14, 0x3C);
static constexpr auto Firebrick = Color(0xB2, 0x22, 0x22);
static constexpr auto DarkRed = Color(0x8B, 0x00, 0x00);
static constexpr auto Red = Color(0xFF, 0x00, 0x00);

// Orange colors.

static constexpr auto OrangeRed = Color(0xFF, 0x45, 0x00);
static constexpr auto Tomato = Color(0xFF, 0x63, 0x47);
static constexpr auto Coral = Color(0xFF, 0x7F, 0x50);
static constexpr auto DarkOrange = Color(0xFF, 0x8C, 0x00);
static constexpr auto Orange = Color(0xFF, 0xA5, 0x00);

// Yellow colors.

static constexpr auto Yellow = Color(0xFF, 0xFF, 0x00);
static constexpr auto LightYellow = Color(0xFF, 0xFF, 0xE0);
static constexpr auto LemonChiffon = Color(0xFF, 0xFA, 0xCD);
static constexpr auto LightGoldenrodYellow = Color(0xFA, 0xFA, 0xD2);
static constexpr auto PapayaWhip = Color(0xFF, 0xEF, 0xD5);
static constexpr auto Moccasin = Color(0xFF, 0xE4, 0xB5);
static constexpr auto PeachPuff = Color(0xFF, 0xDA, 0xB9);
static constexpr auto PaleGoldenrod = Color(0xEE, 0xE8, 0xAA);
static constexpr auto Khaki = Color(0xF0, 0xE6, 0x8C);
static constexpr auto DarkKhaki = Color(0xBD, 0xB7, 0x6B);
static constexpr auto Gold = Color(0xFF, 0xD7, 0x00);

// Brown colors.

static constexpr auto Cornsilk = Color(0xFF, 0xF8, 0xDC);
static constexpr auto BlanchedAlmond = Color(0xFF, 0xEB, 0xCD);
static constexpr auto Bisque = Color(0xFF, 0xE4, 0xC4);
static constexpr auto NavajoWhite = Color(0xFF, 0xDE, 0xAD);
static constexpr auto Wheat = Color(0xF5, 0xDE, 0xB3);
static constexpr auto Burlywood = Color(0xDE, 0xB8, 0x87);
static constexpr auto Tan = Color(0xD2, 0xB4, 0x8C);
static constexpr auto RosyBrown = Color(0xBC, 0x8F, 0x8F);
static constexpr auto SandyBrown = Color(0xF4, 0xA4, 0x60);
static constexpr auto Goldenrod = Color(0xDA, 0xA5, 0x20);
static constexpr auto DarkGoldenrod = Color(0xB8, 0x86, 0x0B);
static constexpr auto Peru = Color(0xCD, 0x85, 0x3F);
static constexpr auto Chocolate = Color(0xD2, 0x69, 0x1E);
static constexpr auto SaddleBrown = Color(0x8B, 0x45, 0x13);
static constexpr auto Sienna = Color(0xA0, 0x52, 0x2D);
static constexpr auto Brown = Color(0xA5, 0x2A, 0x2A);
static constexpr auto Maroon = Color(0x80, 0x00, 0x00);

// Green colors.

static constexpr auto DarkOliveGreen = Color(0x55, 0x6B, 0x2F);
static constexpr auto Olive = Color(0x80, 0x80, 0x00);
static constexpr auto OliveDrab = Color(0x6B, 0x8E, 0x23);
static constexpr auto YellowGreen = Color(0x9A, 0xCD, 0x32);
static constexpr auto LimeGreen = Color(0x32, 0xCD, 0x32);
static constexpr auto Lime = Color(0x00, 0xFF, 0x00);
static constexpr auto LawnGreen = Color(0x7C, 0xFC, 0x00);
static constexpr auto Chartreuse = Color(0x7F, 0xFF, 0x00);
static constexpr auto GreenYellow = Color(0xAD, 0xFF, 0x2F);
static constexpr auto SpringGreen = Color(0x00, 0xFF, 0x7F);
static constexpr auto MediumSpringGreen = Color(0x00, 0xFA, 0x9A);
static constexpr auto LightGreen = Color(0x90, 0xEE, 0x90);
static constexpr auto PaleGreen = Color(0x98, 0xFB, 0x98);
static constexpr auto DarkSeaGreen = Color(0x8F, 0xBC, 0x8F);
static constexpr auto MediumAquamarine = Color(0x66, 0xCD, 0xAA);
static constexpr auto MediumSeaGreen = Color(0x3C, 0xB3, 0x71);
static constexpr auto SeaGreen = Color(0x2E, 0x8B, 0x57);
static constexpr auto ForestGreen = Color(0x22, 0x8B, 0x22);
static constexpr auto Green = Color(0x00, 0x80, 0x00);
static constexpr auto DarkGreen = Color(0x00, 0x64, 0x00);

// Cyan colors.

static constexpr auto Aqua = Color(0x00, 0xFF, 0xFF);
static constexpr auto Cyan = Color(0x00, 0xFF, 0xFF);
static constexpr auto LightCyan = Color(0xE0, 0xFF, 0xFF);
static constexpr auto PaleTurquoise = Color(0xAF, 0xEE, 0xEE);
static constexpr auto Aquamarine = Color(0x7F, 0xFF, 0xD4);
static constexpr auto Turquoise = Color(0x40, 0xE0, 0xD0);
static constexpr auto MediumTurquoise = Color(0x48, 0xD1, 0xCC);
static constexpr auto DarkTurquoise = Color(0x00, 0xCE, 0xD1);
static constexpr auto LightSeaGreen = Color(0x20, 0xB2, 0xAA);
static constexpr auto CadetBlue = Color(0x5F, 0x9E, 0xA0);
static constexpr auto DarkCyan = Color(0x00, 0x8B, 0x8B);
static constexpr auto Teal = Color(0x00, 0x80, 0x80);

// Blue colors.

static constexpr auto LightSteelBlue = Color(0xB0, 0xC4, 0xDE);
static constexpr auto PowderBlue = Color(0xB0, 0xE0, 0xE6);
static constexpr auto LightBlue = Color(0xAD, 0xD8, 0xE6);
static constexpr auto SkyBlue = Color(0x87, 0xCE, 0xEB);
static constexpr auto LightSkyBlue = Color(0x87, 0xCE, 0xFA);
static constexpr auto DeepSkyBlue = Color(0x00, 0xBF, 0xFF);
static constexpr auto DodgerBlue = Color(0x1E, 0x90, 0xFF);
static constexpr auto CornflowerBlue = Color(0x64, 0x95, 0xED);
static constexpr auto SteelBlue = Color(0x46, 0x82, 0xB4);
static constexpr auto RoyalBlue = Color(0x41, 0x69, 0xE1);
static constexpr auto Blue = Color(0x00, 0x00, 0xFF);
static constexpr auto MediumBlue = Color(0x00, 0x00, 0xCD);
static constexpr auto DarkBlue = Color(0x00, 0x00, 0x8B);
static constexpr auto Navy = Color(0x00, 0x00, 0x80);
static constexpr auto MidnightBlue = Color(0x19, 0x19, 0x70);

// Purple, violet, and magenta colors.

static constexpr auto Lavender = Color(0xE6, 0xE6, 0xFA);
static constexpr auto Thistle = Color(0xD8, 0xBF, 0xD8);
static constexpr auto Plum = Color(0xDD, 0xA0, 0xDD);
static constexpr auto Violet = Color(0xEE, 0x82, 0xEE);
static constexpr auto Orchid = Color(0xDA, 0x70, 0xD6);
static constexpr auto Fuchsia = Color(0xFF, 0x00, 0xFF);
static constexpr auto Magenta = Color(0xFF, 0x00, 0xFF);
static constexpr auto MediumOrchid = Color(0xBA, 0x55, 0xD3);
static constexpr auto MediumPurple = Color(0x93, 0x70, 0xDB);
static constexpr auto BlueViolet = Color(0x8A, 0x2B, 0xE2);
static constexpr auto DarkViolet = Color(0x94, 0x00, 0xD3);
static constexpr auto DarkOrchid = Color(0x99, 0x32, 0xCC);
static constexpr auto DarkMagenta = Color(0x8B, 0x00, 0x8B);
static constexpr auto Purple = Color(0x80, 0x00, 0x80);
static constexpr auto Indigo = Color(0x4B, 0x00, 0x82);
static constexpr auto DarkSlateBlue = Color(0x48, 0x3D, 0x8B);
static constexpr auto SlateBlue = Color(0x6A, 0x5A, 0xCD);
static constexpr auto MediumSlateBlue = Color(0x7B, 0x68, 0xEE);

// White colors.

static constexpr auto White = Color(0xFF, 0xFF, 0xFF);
static constexpr auto Snow = Color(0xFF, 0xFA, 0xFA);
static constexpr auto Honeydew = Color(0xF0, 0xFF, 0xF0);
static constexpr auto MintCream = Color(0xF5, 0xFF, 0xFA);
static constexpr auto Azure = Color(0xF0, 0xFF, 0xFF);
static constexpr auto AliceBlue = Color(0xF0, 0xF8, 0xFF);
static constexpr auto GhostWhite = Color(0xF8, 0xF8, 0xFF);
static constexpr auto WhiteSmoke = Color(0xF5, 0xF5, 0xF5);
static constexpr auto Seashell = Color(0xFF, 0xF5, 0xEE);
static constexpr auto Beige = Color(0xF5, 0xF5, 0xDC);
static constexpr auto OldLace = Color(0xFD, 0xF5, 0xE6);
static constexpr auto FloralWhite = Color(0xFF, 0xFA, 0xF0);
static constexpr auto Ivory = Color(0xFF, 0xFF, 0xF0);
static constexpr auto AntiqueWhite = Color(0xFA, 0xEB, 0xD7);
static constexpr auto Linen = Color(0xFA, 0xF0, 0xE6);
static constexpr auto LavenderBlush = Color(0xFF, 0xF0, 0xF5);
static constexpr auto MistyRose = Color(0xFF, 0xE4, 0xE1);

// Gray and black colors.

static constexpr auto Gainsboro = Color(0xDC, 0xDC, 0xDC);
static constexpr auto LightGray = Color(0xD3, 0xD3, 0xD3);
static constexpr auto Silver = Color(0xC0, 0xC0, 0xC0);
static constexpr auto DarkGray = Color(0xA9, 0xA9, 0xA9);
static constexpr auto Gray = Color(0x80, 0x80, 0x80);
static constexpr auto DimGray = Color(0x69, 0x69, 0x69);
static constexpr auto LightSlateGray = Color(0x77, 0x88, 0x99);
static constexpr auto SlateGray = Color(0x70, 0x80, 0x90);
static constexpr auto DarkSlateGray = Color(0x2F, 0x4F, 0x4F);
static constexpr auto Black = Color(0x00, 0x00, 0x00);

}  // namespace color

}  // namespace roo_display
