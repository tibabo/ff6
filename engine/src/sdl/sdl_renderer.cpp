#include <cmath>
#include "sdl/sdl_renderer.hpp"

#include <algorithm>
#include <cstdint>
#include <fstream>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

#include "constants.hpp"

#define FF6_APP_WINDOW_TITLE "FF6 native prototype"
#define FF6_APP_WINDOW_SCALE 3  // initial window size, in multiples of the logical screen
#define FF6_SDL_SCALE_NEAREST "0"  // value of SDL_HINT_RENDER_SCALE_QUALITY
#define FF6_SDL_CLEAR_RGBA 0, 0, 0, 255

namespace ff6 {
namespace {

void put32(std::vector<uint8_t>& out, uint32_t value, bool bigEndian) {
    for (int i = 0; i < 4; ++i) out.push_back(static_cast<uint8_t>(value >> (bigEndian ? 24 - 8 * i : 8 * i)));
}

uint32_t crc32(const uint8_t* data, size_t size, uint32_t crc = 0xFFFFFFFFu) {
    for (size_t i = 0; i < size; ++i) {
        crc ^= data[i];
        for (int bit = 0; bit < 8; ++bit) crc = (crc >> 1) ^ (0xEDB88320u & (0u - (crc & 1u)));
    }
    return crc;
}

void chunk(std::vector<uint8_t>& out, const char* type, const std::vector<uint8_t>& data) {
    put32(out, static_cast<uint32_t>(data.size()), true);
    std::vector<uint8_t> body(type, type + 4);
    body.insert(body.end(), data.begin(), data.end());
    out.insert(out.end(), body.begin(), body.end());
    put32(out, ~crc32(body.data(), body.size()), true);
}

// `rgb` is width x height x 3 bytes. A PNG without compression (stored blocks), which every viewer reads.
std::vector<uint8_t> encodePng(const std::vector<uint8_t>& rgb, int width, int height) {
    std::vector<uint8_t> raw;
    for (int y = 0; y < height; ++y) {
        raw.push_back(0);  // no filter
        raw.insert(raw.end(), rgb.begin() + static_cast<std::ptrdiff_t>(y) * width * 3, rgb.begin() + static_cast<std::ptrdiff_t>(y + 1) * width * 3);
    }
    std::vector<uint8_t> zlib = {0x78, 0x01};
    for (size_t at = 0; at < raw.size() || at == 0;) {
        const size_t size = std::min<size_t>(raw.size() - at, 65535);
        const bool last = at + size >= raw.size();
        zlib.push_back(last ? 1 : 0);
        zlib.push_back(static_cast<uint8_t>(size));
        zlib.push_back(static_cast<uint8_t>(size >> 8));
        zlib.push_back(static_cast<uint8_t>(~size));
        zlib.push_back(static_cast<uint8_t>((~size) >> 8));
        zlib.insert(zlib.end(), raw.begin() + static_cast<std::ptrdiff_t>(at), raw.begin() + static_cast<std::ptrdiff_t>(at + size));
        at += size;
        if (last) break;
    }
    uint32_t a = 1, b = 0;
    for (const uint8_t byte : raw) {
        a = (a + byte) % 65521;
        b = (b + a) % 65521;
    }
    put32(zlib, (b << 16) | a, true);
    std::vector<uint8_t> png = {0x89, 'P', 'N', 'G', 0x0D, 0x0A, 0x1A, 0x0A};
    std::vector<uint8_t> header;
    put32(header, static_cast<uint32_t>(width), true);
    put32(header, static_cast<uint32_t>(height), true);
    header.insert(header.end(), {8, 2, 0, 0, 0});  // 8 bits, RGB
    chunk(png, "IHDR", header);
    chunk(png, "IDAT", zlib);
    chunk(png, "IEND", {});
    return png;
}

// A BMP of 24 bits with the plain 40-byte header, the rows from the bottom, as every viewer expects.
std::vector<uint8_t> encodeBmp(const std::vector<uint8_t>& rgb, int width, int height) {
    const size_t stride = (static_cast<size_t>(width) * 3 + 3) & ~static_cast<size_t>(3);
    const uint32_t pixels = static_cast<uint32_t>(stride * static_cast<size_t>(height));
    std::vector<uint8_t> bmp = {'B', 'M'};
    put32(bmp, 54 + pixels, false);
    put32(bmp, 0, false);
    put32(bmp, 54, false);
    put32(bmp, 40, false);
    put32(bmp, static_cast<uint32_t>(width), false);
    put32(bmp, static_cast<uint32_t>(height), false);
    bmp.insert(bmp.end(), {1, 0, 24, 0});
    put32(bmp, 0, false);
    put32(bmp, pixels, false);
    put32(bmp, 2835, false);
    put32(bmp, 2835, false);
    put32(bmp, 0, false);
    put32(bmp, 0, false);
    for (int y = height - 1; y >= 0; --y) {
        for (int x = 0; x < width; ++x) {
            const uint8_t* pixel = &rgb[(static_cast<size_t>(y) * width + x) * 3];
            bmp.insert(bmp.end(), {pixel[2], pixel[1], pixel[0]});  // blue, green, red
        }
        bmp.insert(bmp.end(), stride - static_cast<size_t>(width) * 3, 0);
    }
    return bmp;
}

}  // namespace

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

void SdlRenderer::setWideAspect(bool wide) {
    wideAspect_ = wide && !offscreen_;
    SDL_RenderSetLogicalSize(renderer_.get(), FF6_SCREEN_WIDTH * (wideAspect_ ? FF6_WIDE_PIXEL_X : 1), FF6_SCREEN_HEIGHT * (wideAspect_ ? FF6_WIDE_PIXEL_Y : 1));
}

void SdlRenderer::setAresColors(bool ares) {
    aresColors_ = ares;
    for (int i = 0; i < 256; ++i) curve_[static_cast<size_t>(i)] = static_cast<uint8_t>(std::lround(255.0 * std::pow(i / 255.0, FF6_ARES_GAMMA)));
}

void SdlRenderer::updateTexture(TextureId texture, const uint8_t* rgba) {
    const TextureEntry& target = entry(texture);
    if (aresColors_ && !offscreen_) {
        int width = 0, height = 0;
        SDL_QueryTexture(target.texture.get(), nullptr, nullptr, &width, &height);
        const size_t bytes = static_cast<size_t>(width) * static_cast<size_t>(height) * FF6_BYTES_PER_PIXEL;
        curved_.assign(rgba, rgba + bytes);
        for (size_t i = 0; i < bytes; i += FF6_BYTES_PER_PIXEL) {
            for (size_t c = 0; c < FF6_ALPHA_OFFSET; ++c) curved_[i + c] = curve_[curved_[i + c]];
        }
        rgba = curved_.data();
    }
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
    const int sx = wideAspect_ ? FF6_WIDE_PIXEL_X : 1, sy = wideAspect_ ? FF6_WIDE_PIXEL_Y : 1;
    const SDL_Rect to{destination.x * sx, destination.y * sy, destination.w * sx, destination.h * sy};
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
    if (!surface || SDL_RenderReadPixels(renderer_.get(), nullptr, SDL_PIXELFORMAT_ARGB8888, surface->pixels, surface->pitch) != 0) {
        throw std::runtime_error(std::string("capture failed: ") + SDL_GetError());
    }
    std::vector<uint8_t> rgb;
    rgb.reserve(static_cast<size_t>(FF6_SCREEN_WIDTH) * FF6_SCREEN_HEIGHT * 3);
    for (int y = 0; y < FF6_SCREEN_HEIGHT; ++y) {
        const uint8_t* row = static_cast<const uint8_t*>(surface->pixels) + static_cast<size_t>(y) * surface->pitch;
        for (int x = 0; x < FF6_SCREEN_WIDTH; ++x) rgb.insert(rgb.end(), {row[x * 4 + 2], row[x * 4 + 1], row[x * 4]});  // bytes are B, G, R, A
    }
    const bool png = path.size() > 4 && path.compare(path.size() - 4, 4, ".png") == 0;
    const std::vector<uint8_t> file = png ? encodePng(rgb, FF6_SCREEN_WIDTH, FF6_SCREEN_HEIGHT) : encodeBmp(rgb, FF6_SCREEN_WIDTH, FF6_SCREEN_HEIGHT);
    std::ofstream out(path, std::ios::binary);
    out.write(reinterpret_cast<const char*>(file.data()), static_cast<std::streamsize>(file.size()));
    if (!out) throw std::runtime_error("capture failed: cannot write " + path);
}

}  // namespace ff6
