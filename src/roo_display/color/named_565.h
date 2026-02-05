#pragma once

#include "roo_display/color/color.h"
#include "roo_display/color/color_modes.h"
#include "roo_display/color/named.h"

/// Defines 140 opaque HTML named colors, adjusted for RGB565.
///
/// Each color round-trips to itself when truncated to RGB565 and converted
/// back to ARGB8888.

namespace roo_display {

namespace color {

namespace rgb565 {

// Pink colors.

static constexpr auto Pink = TruncateColor<Rgb565>(color::Pink);
static constexpr auto LightPink = TruncateColor<Rgb565>(color::LightPink);
static constexpr auto HotPink = TruncateColor<Rgb565>(color::HotPink);
static constexpr auto DeepPink = TruncateColor<Rgb565>(color::DeepPink);
static constexpr auto PaleVioletRed =
    TruncateColor<Rgb565>(color::PaleVioletRed);
static constexpr auto MediumVioletRed =
    TruncateColor<Rgb565>(color::MediumVioletRed);

// Red colors.

static constexpr auto LightSalmon = TruncateColor<Rgb565>(color::LightSalmon);
static constexpr auto Salmon = TruncateColor<Rgb565>(color::Salmon);
static constexpr auto DarkSalmon = TruncateColor<Rgb565>(color::DarkSalmon);
static constexpr auto LightCoral = TruncateColor<Rgb565>(color::LightCoral);
static constexpr auto IndianRed = TruncateColor<Rgb565>(color::IndianRed);
static constexpr auto Crimson = TruncateColor<Rgb565>(color::Crimson);
static constexpr auto Firebrick = TruncateColor<Rgb565>(color::Firebrick);
static constexpr auto DarkRed = TruncateColor<Rgb565>(color::DarkRed);
static constexpr auto Red = TruncateColor<Rgb565>(color::Red);

// Orange colors.

static constexpr auto OrangeRed = TruncateColor<Rgb565>(color::OrangeRed);
static constexpr auto Tomato = TruncateColor<Rgb565>(color::Tomato);
static constexpr auto Coral = TruncateColor<Rgb565>(color::Coral);
static constexpr auto DarkOrange = TruncateColor<Rgb565>(color::DarkOrange);
static constexpr auto Orange = TruncateColor<Rgb565>(color::Orange);

// Yellow colors.

static constexpr auto Yellow = TruncateColor<Rgb565>(color::Yellow);
static constexpr auto LightYellow = TruncateColor<Rgb565>(color::LightYellow);
static constexpr auto LemonChiffon = TruncateColor<Rgb565>(color::LemonChiffon);
static constexpr auto LightGoldenrodYellow =
    TruncateColor<Rgb565>(color::LightGoldenrodYellow);
static constexpr auto PapayaWhip = TruncateColor<Rgb565>(color::PapayaWhip);
static constexpr auto Moccasin = TruncateColor<Rgb565>(color::Moccasin);
static constexpr auto PeachPuff = TruncateColor<Rgb565>(color::PeachPuff);
static constexpr auto PaleGoldenrod =
    TruncateColor<Rgb565>(color::PaleGoldenrod);
static constexpr auto Khaki = TruncateColor<Rgb565>(color::Khaki);
static constexpr auto DarkKhaki = TruncateColor<Rgb565>(color::DarkKhaki);
static constexpr auto Gold = TruncateColor<Rgb565>(color::Gold);

// Brown colors.

static constexpr auto Cornsilk = TruncateColor<Rgb565>(color::Cornsilk);
static constexpr auto BlanchedAlmond =
    TruncateColor<Rgb565>(color::BlanchedAlmond);
static constexpr auto Bisque = TruncateColor<Rgb565>(color::Bisque);
static constexpr auto NavajoWhite = TruncateColor<Rgb565>(color::NavajoWhite);
static constexpr auto Wheat = TruncateColor<Rgb565>(color::Wheat);
static constexpr auto Burlywood = TruncateColor<Rgb565>(color::Burlywood);
static constexpr auto Tan = TruncateColor<Rgb565>(color::Tan);
static constexpr auto RosyBrown = TruncateColor<Rgb565>(color::RosyBrown);
static constexpr auto SandyBrown = TruncateColor<Rgb565>(color::SandyBrown);
static constexpr auto Goldenrod = TruncateColor<Rgb565>(color::Goldenrod);
static constexpr auto DarkGoldenrod =
    TruncateColor<Rgb565>(color::DarkGoldenrod);
static constexpr auto Peru = TruncateColor<Rgb565>(color::Peru);
static constexpr auto Chocolate = TruncateColor<Rgb565>(color::Chocolate);
static constexpr auto SaddleBrown = TruncateColor<Rgb565>(color::SaddleBrown);
static constexpr auto Sienna = TruncateColor<Rgb565>(color::Sienna);
static constexpr auto Brown = TruncateColor<Rgb565>(color::Brown);
static constexpr auto Maroon = TruncateColor<Rgb565>(color::Maroon);

// Green colors.

static constexpr auto DarkOliveGreen =
    TruncateColor<Rgb565>(color::DarkOliveGreen);
static constexpr auto Olive = TruncateColor<Rgb565>(color::Olive);
static constexpr auto OliveDrab = TruncateColor<Rgb565>(color::OliveDrab);
static constexpr auto YellowGreen = TruncateColor<Rgb565>(color::YellowGreen);
static constexpr auto LimeGreen = TruncateColor<Rgb565>(color::LimeGreen);
static constexpr auto Lime = TruncateColor<Rgb565>(color::Lime);
static constexpr auto LawnGreen = TruncateColor<Rgb565>(color::LawnGreen);
static constexpr auto Chartreuse = TruncateColor<Rgb565>(color::Chartreuse);
static constexpr auto GreenYellow = TruncateColor<Rgb565>(color::GreenYellow);
static constexpr auto SpringGreen = TruncateColor<Rgb565>(color::SpringGreen);
static constexpr auto MediumSpringGreen =
    TruncateColor<Rgb565>(color::MediumSpringGreen);
static constexpr auto LightGreen = TruncateColor<Rgb565>(color::LightGreen);
static constexpr auto PaleGreen = TruncateColor<Rgb565>(color::PaleGreen);
static constexpr auto DarkSeaGreen = TruncateColor<Rgb565>(color::DarkSeaGreen);
static constexpr auto MediumAquamarine =
    TruncateColor<Rgb565>(color::MediumAquamarine);
static constexpr auto MediumSeaGreen =
    TruncateColor<Rgb565>(color::MediumSeaGreen);
static constexpr auto SeaGreen = TruncateColor<Rgb565>(color::SeaGreen);
static constexpr auto ForestGreen = TruncateColor<Rgb565>(color::ForestGreen);
static constexpr auto Green = TruncateColor<Rgb565>(color::Green);
static constexpr auto DarkGreen = TruncateColor<Rgb565>(color::DarkGreen);

// Cyan colors.

static constexpr auto Aqua = TruncateColor<Rgb565>(color::Aqua);
static constexpr auto Cyan = TruncateColor<Rgb565>(color::Cyan);
static constexpr auto LightCyan = TruncateColor<Rgb565>(color::LightCyan);
static constexpr auto PaleTurquoise =
    TruncateColor<Rgb565>(color::PaleTurquoise);
static constexpr auto Aquamarine = TruncateColor<Rgb565>(color::Aquamarine);
static constexpr auto Turquoise = TruncateColor<Rgb565>(color::Turquoise);
static constexpr auto MediumTurquoise =
    TruncateColor<Rgb565>(color::MediumTurquoise);
static constexpr auto DarkTurquoise =
    TruncateColor<Rgb565>(color::DarkTurquoise);
static constexpr auto LightSeaGreen =
    TruncateColor<Rgb565>(color::LightSeaGreen);
static constexpr auto CadetBlue = TruncateColor<Rgb565>(color::CadetBlue);
static constexpr auto DarkCyan = TruncateColor<Rgb565>(color::DarkCyan);
static constexpr auto Teal = TruncateColor<Rgb565>(color::Teal);

// Blue colors.

static constexpr auto LightSteelBlue =
    TruncateColor<Rgb565>(color::LightSteelBlue);
static constexpr auto PowderBlue = TruncateColor<Rgb565>(color::PowderBlue);
static constexpr auto LightBlue = TruncateColor<Rgb565>(color::LightBlue);
static constexpr auto SkyBlue = TruncateColor<Rgb565>(color::SkyBlue);
static constexpr auto LightSkyBlue = TruncateColor<Rgb565>(color::LightSkyBlue);
static constexpr auto DeepSkyBlue = TruncateColor<Rgb565>(color::DeepSkyBlue);
static constexpr auto DodgerBlue = TruncateColor<Rgb565>(color::DodgerBlue);
static constexpr auto CornflowerBlue =
    TruncateColor<Rgb565>(color::CornflowerBlue);
static constexpr auto SteelBlue = TruncateColor<Rgb565>(color::SteelBlue);
static constexpr auto RoyalBlue = TruncateColor<Rgb565>(color::RoyalBlue);
static constexpr auto Blue = TruncateColor<Rgb565>(color::Blue);
static constexpr auto MediumBlue = TruncateColor<Rgb565>(color::MediumBlue);
static constexpr auto DarkBlue = TruncateColor<Rgb565>(color::DarkBlue);
static constexpr auto Navy = TruncateColor<Rgb565>(color::Navy);
static constexpr auto MidnightBlue = TruncateColor<Rgb565>(color::MidnightBlue);

// Purple, violet, and magenta colors.

static constexpr auto Lavender = TruncateColor<Rgb565>(color::Lavender);
static constexpr auto Thistle = TruncateColor<Rgb565>(color::Thistle);
static constexpr auto Plum = TruncateColor<Rgb565>(color::Plum);
static constexpr auto Violet = TruncateColor<Rgb565>(color::Violet);
static constexpr auto Orchid = TruncateColor<Rgb565>(color::Orchid);
static constexpr auto Fuchsia = TruncateColor<Rgb565>(color::Fuchsia);
static constexpr auto Magenta = TruncateColor<Rgb565>(color::Magenta);
static constexpr auto MediumOrchid = TruncateColor<Rgb565>(color::MediumOrchid);
static constexpr auto MediumPurple = TruncateColor<Rgb565>(color::MediumPurple);
static constexpr auto BlueViolet = TruncateColor<Rgb565>(color::BlueViolet);
static constexpr auto DarkViolet = TruncateColor<Rgb565>(color::DarkViolet);
static constexpr auto DarkOrchid = TruncateColor<Rgb565>(color::DarkOrchid);
static constexpr auto DarkMagenta = TruncateColor<Rgb565>(color::DarkMagenta);
static constexpr auto Purple = TruncateColor<Rgb565>(color::Purple);
static constexpr auto Indigo = TruncateColor<Rgb565>(color::Indigo);
static constexpr auto DarkSlateBlue =
    TruncateColor<Rgb565>(color::DarkSlateBlue);
static constexpr auto SlateBlue = TruncateColor<Rgb565>(color::SlateBlue);
static constexpr auto MediumSlateBlue =
    TruncateColor<Rgb565>(color::MediumSlateBlue);

// White colors.

static constexpr auto White = TruncateColor<Rgb565>(color::White);
static constexpr auto Snow = TruncateColor<Rgb565>(color::Snow);
static constexpr auto Honeydew = TruncateColor<Rgb565>(color::Honeydew);
static constexpr auto MintCream = TruncateColor<Rgb565>(color::MintCream);
static constexpr auto Azure = TruncateColor<Rgb565>(color::Azure);
static constexpr auto AliceBlue = TruncateColor<Rgb565>(color::AliceBlue);
static constexpr auto GhostWhite = TruncateColor<Rgb565>(color::GhostWhite);
static constexpr auto WhiteSmoke = TruncateColor<Rgb565>(color::WhiteSmoke);
static constexpr auto Seashell = TruncateColor<Rgb565>(color::Seashell);
static constexpr auto Beige = TruncateColor<Rgb565>(color::Beige);
static constexpr auto OldLace = TruncateColor<Rgb565>(color::OldLace);
static constexpr auto FloralWhite = TruncateColor<Rgb565>(color::FloralWhite);
static constexpr auto Ivory = TruncateColor<Rgb565>(color::Ivory);
static constexpr auto AntiqueWhite = TruncateColor<Rgb565>(color::AntiqueWhite);
static constexpr auto Linen = TruncateColor<Rgb565>(color::Linen);
static constexpr auto LavenderBlush =
    TruncateColor<Rgb565>(color::LavenderBlush);
static constexpr auto MistyRose = TruncateColor<Rgb565>(color::MistyRose);

// Gray and black colors.

static constexpr auto Gainsboro = TruncateColor<Rgb565>(color::Gainsboro);
static constexpr auto LightGray = TruncateColor<Rgb565>(color::LightGray);
static constexpr auto Silver = TruncateColor<Rgb565>(color::Silver);
static constexpr auto DarkGray = TruncateColor<Rgb565>(color::DarkGray);
static constexpr auto Gray = TruncateColor<Rgb565>(color::Gray);
static constexpr auto DimGray = TruncateColor<Rgb565>(color::DimGray);
static constexpr auto LightSlateGray =
    TruncateColor<Rgb565>(color::LightSlateGray);
static constexpr auto SlateGray = TruncateColor<Rgb565>(color::SlateGray);
static constexpr auto DarkSlateGray =
    TruncateColor<Rgb565>(color::DarkSlateGray);
static constexpr auto Black = TruncateColor<Rgb565>(color::Black);

}  // namespace rgb565

}  // namespace color

}  // namespace roo_display
