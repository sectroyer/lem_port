// Builds the terrain bitmap of a level from its pieces.
#pragma once

#include "graphicset.h"
#include "level.h"

namespace lem {

constexpr int kLevelWidth = 1600;
constexpr int kLevelHeight = 160;
constexpr int kSpecialOffsetX = 304;

// Returns a kLevelWidth x kLevelHeight bitmap; kTransparent = no terrain.
Bitmap buildTerrain(const Level& lvl, const GraphicSet& gs, const SpecialSet* spec);

// Draws `src` onto `dst` at (x, y), honouring the object placement modifiers.
// `terrain` is the terrain-only bitmap used for the modifier tests.
void blitObject(Bitmap& dst, const Bitmap& terrain, const Bitmap& src, int x, int y,
                bool noOverwrite, bool onlyOnTerrain, bool upsideDown);

// Static snapshot: terrain plus every object in its preview frame.
Bitmap renderLevelPreview(const Level& lvl, const GraphicSet& gs, const SpecialSet* spec);

} // namespace lem
