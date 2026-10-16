#pragma once

#include <cstdint>

namespace ff6 {

struct Rect {
    int x = 0, y = 0, w = 0, h = 0;
};

// Handle to a texture owned by the renderer that created it.
using TextureId = int;

enum class TextureUsage {
    Static,   // uploaded once
    Dynamic,  // replaced often through updateTexture
};

// Everything the game asks of a graphics library; implement it to port the game to another one.
// Pixels are RGBA with one byte per channel and are drawn with alpha blending. Coordinates are in the
// logical FF6_SCREEN_WIDTH x FF6_SCREEN_HEIGHT screen, which the backend scales to its window.
// Failures are reported with std::runtime_error.
class Renderer {
public:
    virtual ~Renderer() = default;

    // rgba may be null when the content is uploaded later with updateTexture.
    virtual TextureId createTexture(int width, int height, const uint8_t* rgba, TextureUsage usage) = 0;
    // Replaces the whole texture.
    virtual void updateTexture(TextureId texture, const uint8_t* rgba) = 0;

    virtual void beginFrame() = 0;  // clears the screen to black
    virtual void draw(TextureId texture, const Rect& source, const Rect& destination) = 0;
    virtual void endFrame() = 0;  // shows the finished frame
};

}  // namespace ff6
