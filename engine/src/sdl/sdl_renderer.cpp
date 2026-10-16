#include "sdl/sdl_renderer.hpp"

#include <stdexcept>
#include <utility>

#include "constants.hpp"

#define FF6_APP_WINDOW_TITLE "FF6 native prototype"
#define FF6_APP_WINDOW_SCALE 3  // initial window size, in multiples of the logical screen
#define FF6_SDL_SCALE_NEAREST "0"  // value of SDL_HINT_RENDER_SCALE_QUALITY
#define FF6_SDL_CLEAR_RGBA 0, 0, 0, 255

namespace ff6 {

SdlRenderer::SdlRenderer(bool offscreen) : offscreen_(offscreen) {
    SDL_SetHint(SDL_HINT_RENDER_SCALE_QUALITY, FF6_SDL_SCALE_NEAREST);
    window_.reset(SDL_CreateWindow(FF6_APP_WINDOW_TITLE, SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
                                   FF6_SCREEN_WIDTH * FF6_APP_WINDOW_SCALE, FF6_SCREEN_HEIGHT * FF6_APP_WINDOW_SCALE,
                                   SDL_WINDOW_RESIZABLE | SDL_WINDOW_ALLOW_HIGHDPI |
                                       (offscreen ? SDL_WINDOW_HIDDEN : 0)));
    if (!window_) throw std::runtime_error(SDL_GetError());
    renderer_.reset(SDL_CreateRenderer(window_.get(), -1,
                                       offscreen ? SDL_RENDERER_TARGETTEXTURE
                                                 : SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC));
    if (!renderer_) throw std::runtime_error(SDL_GetError());
    SDL_RenderSetLogicalSize(renderer_.get(), FF6_SCREEN_WIDTH, FF6_SCREEN_HEIGHT);
    SDL_GetRendererInfo(renderer_.get(), &info_);
}

TextureId SdlRenderer::createTexture(int width, int height, const uint8_t* rgba, TextureUsage usage) {
    if (info_.max_texture_width && (width > info_.max_texture_width || height > info_.max_texture_height)) {
        throw std::runtime_error("texture is larger than the GPU texture limit");
    }
    const int access = usage == TextureUsage::Dynamic ? SDL_TEXTUREACCESS_STREAMING : SDL_TEXTUREACCESS_STATIC;
    std::unique_ptr<SDL_Texture, Deleter> texture(
        SDL_CreateTexture(renderer_.get(), SDL_PIXELFORMAT_ABGR8888, access, width, height));
    if (!texture) throw std::runtime_error(SDL_GetError());
    SDL_SetTextureBlendMode(texture.get(), SDL_BLENDMODE_BLEND);
    if (rgba && SDL_UpdateTexture(texture.get(), nullptr, rgba, width * FF6_BYTES_PER_PIXEL) != 0) {
        throw std::runtime_error(SDL_GetError());
    }
    textures_.push_back({std::move(texture), width});
    return static_cast<TextureId>(textures_.size() - 1);
}

SdlRenderer::TextureEntry& SdlRenderer::entry(TextureId id) { return textures_.at(static_cast<size_t>(id)); }

void SdlRenderer::updateTexture(TextureId texture, const uint8_t* rgba) {
    const TextureEntry& target = entry(texture);
    if (SDL_UpdateTexture(target.texture.get(), nullptr, rgba, target.width * FF6_BYTES_PER_PIXEL) != 0) {
        throw std::runtime_error(SDL_GetError());
    }
}

void SdlRenderer::beginFrame() {
    if (offscreen_) {
        if (!target_) {
            target_.reset(SDL_CreateTexture(renderer_.get(), SDL_PIXELFORMAT_ARGB8888, SDL_TEXTUREACCESS_TARGET,
                                            FF6_SCREEN_WIDTH, FF6_SCREEN_HEIGHT));
            if (!target_) throw std::runtime_error(SDL_GetError());
        }
        if (SDL_SetRenderTarget(renderer_.get(), target_.get()) != 0) throw std::runtime_error(SDL_GetError());
    }
    SDL_SetRenderDrawColor(renderer_.get(), FF6_SDL_CLEAR_RGBA);
    SDL_RenderClear(renderer_.get());
}

void SdlRenderer::draw(TextureId texture, const Rect& source, const Rect& destination) {
    const SDL_Rect from{source.x, source.y, source.w, source.h};
    const SDL_Rect to{destination.x, destination.y, destination.w, destination.h};
    SDL_RenderCopy(renderer_.get(), entry(texture).texture.get(), &from, &to);
}

void SdlRenderer::endFrame() {
    if (!offscreen_) SDL_RenderPresent(renderer_.get());
}

void SdlRenderer::toggleFullscreen() {
    const bool full = SDL_GetWindowFlags(window_.get()) & SDL_WINDOW_FULLSCREEN_DESKTOP;
    SDL_SetWindowFullscreen(window_.get(), full ? 0 : SDL_WINDOW_FULLSCREEN_DESKTOP);
}

void SdlRenderer::toggleIntegerScale() {
    SDL_RenderSetIntegerScale(renderer_.get(), SDL_RenderGetIntegerScale(renderer_.get()) ? SDL_FALSE : SDL_TRUE);
}

void SdlRenderer::saveFrame(const std::string& path) {
    if (!offscreen_ || !target_) throw std::runtime_error("no offscreen frame to save");
    std::unique_ptr<SDL_Surface, Deleter> surface(SDL_CreateRGBSurfaceWithFormat(
        0, FF6_SCREEN_WIDTH, FF6_SCREEN_HEIGHT, FF6_BITS_PER_PIXEL, SDL_PIXELFORMAT_ARGB8888));
    if (!surface || SDL_RenderReadPixels(renderer_.get(), nullptr, SDL_PIXELFORMAT_ARGB8888, surface->pixels,
                                         surface->pitch) != 0 ||
        SDL_SaveBMP(surface.get(), path.c_str()) != 0) {
        throw std::runtime_error(std::string("capture failed: ") + SDL_GetError());
    }
}

}  // namespace ff6
