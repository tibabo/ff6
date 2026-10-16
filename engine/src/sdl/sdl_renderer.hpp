#pragma once

#include <SDL.h>

#include <memory>
#include <string>
#include <vector>

#include "renderer.hpp"

namespace ff6 {

// SDL2 implementation of Renderer. SDL's video subsystem must be initialised before construction.
class SdlRenderer final : public Renderer {
public:
    // offscreen: the window stays hidden and frames are drawn to a texture that saveFrame() can write out.
    explicit SdlRenderer(bool offscreen);

    TextureId createTexture(int width, int height, const uint8_t* rgba, TextureUsage usage) override;
    void updateTexture(TextureId texture, const uint8_t* rgba) override;
    void beginFrame() override;
    void draw(TextureId texture, const Rect& source, const Rect& destination) override;
    void endFrame() override;

    void toggleFullscreen();
    void toggleIntegerScale();
    // Writes the last frame as a BMP file. Offscreen renderers only.
    void saveFrame(const std::string& path);

private:
    struct Deleter {
        void operator()(SDL_Window* window) const { SDL_DestroyWindow(window); }
        void operator()(SDL_Renderer* renderer) const { SDL_DestroyRenderer(renderer); }
        void operator()(SDL_Texture* texture) const { SDL_DestroyTexture(texture); }
        void operator()(SDL_Surface* surface) const { SDL_FreeSurface(surface); }
    };
    struct TextureEntry {
        std::unique_ptr<SDL_Texture, Deleter> texture;
        int width;
    };
    TextureEntry& entry(TextureId id);

    // Declaration order matters: textures must be destroyed before the renderer, the renderer before the window.
    std::unique_ptr<SDL_Window, Deleter> window_;
    std::unique_ptr<SDL_Renderer, Deleter> renderer_;
    std::unique_ptr<SDL_Texture, Deleter> target_;  // offscreen only
    SDL_RendererInfo info_{};
    bool offscreen_;
    std::vector<TextureEntry> textures_;
};

}  // namespace ff6
