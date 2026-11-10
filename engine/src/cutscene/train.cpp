#include "cutscene/train.hpp"

#include <algorithm>

#include "data/pack.hpp"

namespace ff6 {
namespace {

constexpr int kSizes[12] = {16, 14, 12, 11, 10, 9, 8, 7, 6, 5, 4, 3};  // the twelve sizes of a tile, the largest first
constexpr int kGroupLayers[11] = {6, 4, 2, 2, 2, 1, 1, 1, 1, 1, 1};  // the layers that are drawn with each size, the farthest first
constexpr int kTileCount = 29;
constexpr int kMemory = 0x10000;
constexpr int kTilesStart = 0x2000;  // $7e2000: where the pictures of the tiles are
constexpr int kReversedStart = 0x68D9;  // the pictures of the first twelve tiles, turned around, take the place of the last twelve
constexpr int kBufferStart = 0x8000;
constexpr int kPictureStart = 0x9618;  // the first cell of the picture
constexpr int kTrainSong = FF6_SONG_SAVE_THEM;

int groupSize(int group) { return group == 10 ? 16 : group == 9 ? 14 : group + 4; }

}  // namespace

TrainRide::TrainRide(const TrainData& data, const std::vector<uint8_t>& rng, int counter, Sound& sound)
    : data_(data), rng_(rng), sound_(sound), counter_(counter & 0xFF) {
    memory_.assign(kMemory, 0);
    layOutTiles();
    // The map is the picture, and each character of the map is a solid color: the characters are as big as a cell.
    for (int tile = 0; tile < 256; ++tile) {
        for (int i = 0; i < 64; ++i) ppu_.vram[static_cast<size_t>(tile * 64 + i)] = static_cast<uint16_t>(tile << 8);
    }
    for (size_t i = 0; i < data_.palette.size() && i < ppu_.cgram.size(); ++i) ppu_.cgram[i] = data_.palette[i];
    ppu_.tm = 0x01;
    ppu_.ts = 0;
    ppu_.forceBlank = false;
    start();
}

// InitTrainGfx: the pictures of the tiles, two pixels to a byte with a palette for each picture, become a byte for each
// pixel; the first twelve tiles are turned around to make the right hand walls.
void TrainRide::layOutTiles() {
    const std::vector<uint8_t>& gfx = data_.gfx;
    size_t source = 0;
    size_t destination = kTilesStart;
    const auto next = [&]() { return source < gfx.size() ? gfx[source++] : static_cast<uint8_t>(0); };
    for (int tile = 0; tile < kTileCount; ++tile) {
        for (const int size : kSizes) {
            const int pixels = size * size;
            const uint8_t palette = next();
            for (int pair = 0; pair < pixels / 2; ++pair) {
                const uint8_t byte = next();
                memory_[destination++] = (byte & 0xF0) ? static_cast<uint8_t>((byte >> 4) | palette) : 0;
                memory_[destination++] = (byte & 0x0F) ? static_cast<uint8_t>((byte & 0x0F) | palette) : 0;
            }
            if (pixels & 1) {
                const uint8_t byte = next();
                memory_[destination++] = (byte & 0xF0) ? static_cast<uint8_t>((byte >> 4) | palette) : 0;
            }
        }
    }
    size_t reversed = kReversedStart;
    size_t from = kTilesStart;
    for (int tile = 0; tile < 12; ++tile) {
        for (const int size : kSizes) {
            const size_t count = static_cast<size_t>(size * size);
            for (size_t i = 0; i < count; ++i) memory_[reversed++] = memory_[from + count - 1 - i];
            from += count;
        }
    }
}

// MagitekTrain: the song, the first two stretches of the way, and the screen that fades in.
void TrainRide::start() {
    sound_.command(FF6_SOUND_PLAY_SONG, static_cast<uint8_t>(kTrainSong), FF6_SOUND_FULL_VOLUME);
    const int half = step_ & 0x20;
    loadStretch(half, courseOffset_);
    courseOffset_ += FF6_TRAIN_COURSE_RECORD_BYTES;
    loadStretch(0x20 - half, courseOffset_);
    courseOffset_ += FF6_TRAIN_COURSE_RECORD_BYTES;
    brightness_ = 0;
    targetBrightness_ = 15;
    frame_ = FF6_TRAIN_FRAMES_PER_STEP - 1;  // the first step comes at once
    fading_ = false;
    battleReady_ = false;
    waitingForBattle_ = false;
}

// _ee25f5: a record of the course (pitch, turn, walls, command) gives the 32 values of each table for 32 steps of the way.
void TrainRide::loadStretch(int half, int offset) {
    const std::vector<uint8_t>& course = data_.course;
    const auto field = [&](int n) { return offset + n < static_cast<int>(course.size()) ? course[static_cast<size_t>(offset + n)] : static_cast<uint8_t>(0); };
    const auto copy = [&](const std::vector<uint8_t>& table, int type, uint8_t* to) {
        for (int i = 0; i < 32; ++i) {
            const size_t at = static_cast<size_t>(type * 32 + i);
            to[i] = at < table.size() ? table[at] : static_cast<uint8_t>(0);
        }
    };
    copy(data_.pitch1, field(0), &pitch1_[static_cast<size_t>(half)]);
    copy(data_.pitch2, field(0), &pitch2_[static_cast<size_t>(half)]);
    copy(data_.yaw, field(1), &yaw_[static_cast<size_t>(half)]);
    copy(data_.background, field(2), &background_[static_cast<size_t>(half)]);
    copy(data_.yawMultiplier, field(3), yawMultiplier_.data());
    command_ = field(4);
}

void TrainRide::tick(const InputState&) {
    if (done_ || waitingForBattle_) return;
    if (++frame_ >= FF6_TRAIN_FRAMES_PER_STEP) {
        frame_ = 0;
        update();
    }
}

// One step: the command of the stretch, the way, the picture; then the screen fades and the battle comes.
void TrainRide::update() {
    execCommand();
    drawTiles();
    if (battleReady_) {
        waitingForBattle_ = true;
        return;
    }
    if (fading_ && brightness_ == 0) {
        done_ = true;
        return;
    }
    if (brightness_ < targetBrightness_) ++brightness_;
    if (brightness_ > targetBrightness_) --brightness_;
}

// ExecTrainCmd: when a stretch of 32 steps starts, the next one is loaded; the battles come from the course.
void TrainRide::execCommand() {
    if (step_ == 0 || step_ == 0x20) {
        loadStretch(0x20 - step_, courseOffset_);
        courseOffset_ += FF6_TRAIN_COURSE_RECORD_BYTES;
    }
    const int command = command_;
    if (command == FF6_TRAIN_MARKER_BATTLE_1 || command == FF6_TRAIN_MARKER_BATTLE_2) {
        const int roll = rng_.empty() ? 0 : rng_[static_cast<size_t>(counter_ & 0xFF)];
        counter_ = (counter_ + 1) & 0xFF;
        battleGroup_ = command == FF6_TRAIN_MARKER_BATTLE_1 ? FF6_TRAIN_BATTLE_GROUP_1 : FF6_TRAIN_BATTLE_GROUP_2;
        battleSecond_ = roll >= FF6_TRAIN_SECOND_BATTLE_ROLL;
        battleBackground_ = FF6_TRAIN_BATTLE_BG;
        battleReady_ = true;
    } else if (command == FF6_TRAIN_MARKER_BOSS) {
        battleGroup_ = FF6_TRAIN_BOSS_GROUP;
        battleSecond_ = false;
        battleBackground_ = FF6_TRAIN_BATTLE_BG;
        battleReady_ = true;
    } else if (command == FF6_TRAIN_MARKER_END) {
        command_ = 0;
        targetBrightness_ = 0;
        fading_ = true;
    } else if (command >= 0x40 && command < 0x80) {  // a jump forward in the course
        courseOffset_ += (command - 0x3F) * FF6_TRAIN_COURSE_RECORD_BYTES;
        command_ = 0;
    } else if (command < 0xE0) {
        command_ = 0;
    }
    advance();
}

// _ee235b: where each layer of the tunnel is, by the pitch and the turn of the way, then the tiles of each layer.
void TrainRide::advance() {
    const std::vector<uint8_t>& reduce = data_.reduce;
    const auto reduction = [&](int at) { return at >= 0 && at < static_cast<int>(reduce.size()) ? reduce[static_cast<size_t>(at)] : static_cast<uint8_t>(0); };
    const auto negate = [](int value) { return static_cast<uint8_t>(-value); };
    // The height of each layer.
    uint8_t sign = 0;
    const uint8_t base = static_cast<uint8_t>(pitch2_[static_cast<size_t>(step_)] + 0x28);
    uint8_t factor = static_cast<uint8_t>(0x32 - base);
    if (factor & 0x80) {
        factor = negate(factor);
        sign = 1;
    }
    uint8_t last = pitch1_[static_cast<size_t>(step_)];
    int index = step_;
    for (int layer = FF6_TRAIN_LAYERS; layer > 0; --layer) {
        const uint8_t difference = static_cast<uint8_t>(pitch1_[static_cast<size_t>(index)] - last);
        const uint8_t high = static_cast<uint8_t>((factor * reduction(0x27 + layer)) >> 8);
        uint8_t value = sign ? static_cast<uint8_t>((0xFF ^ high) + 1) : high;
        value = static_cast<uint8_t>(value - difference + base);
        layerY_[static_cast<size_t>(index)] = value;
        index = (index + 1) & 0x3F;
        if (index == 0 || index == 0x20) last = static_cast<uint8_t>(pitch1_[static_cast<size_t>(index)] - difference);
    }
    // The side of each layer.
    sign = 0;
    factor = yaw_[static_cast<size_t>(step_)];
    if (factor & 0x80) {
        factor = negate(factor);
        ++sign;
    }
    index = step_;
    for (int layer = 0; layer < FF6_TRAIN_LAYERS; ++layer) {
        uint8_t multiplier = yawMultiplier_[static_cast<size_t>(layer)];
        if (multiplier & 0x80) {
            multiplier = negate(multiplier);
            --sign;
        }
        const uint8_t high = static_cast<uint8_t>((factor * multiplier) >> 8);
        layerX_[static_cast<size_t>(index)] = static_cast<uint8_t>((sign == 0 ? high : static_cast<uint8_t>((0xFF ^ high) + 1)) + 0x40);
        index = (index + 1) & 0x3F;
    }
    // The tiles of the layers that show, the farthest first, twelve for each; a tile that is not there has no place.
    records_.clear();
    index = (step_ + 0x17) & 0x3F;
    for (int layer = FF6_TRAIN_DRAWN_LAYERS; layer > 0; --layer) {
        const uint8_t centerX = layerX_[static_cast<size_t>(index)], centerY = layerY_[static_cast<size_t>(index)];
        const uint8_t scale = reduction(layer - 1);
        const size_t first = static_cast<size_t>(background_[static_cast<size_t>(index)]) * 48;
        for (int tile = 0; tile < FF6_TRAIN_TILES_PER_LAYER; ++tile) {
            Record record;
            const size_t at = first + static_cast<size_t>(tile) * 4;
            if (at + 4 <= data_.tiles.size() && (data_.tiles[at] != 0 || data_.tiles[at + 1] != 0)) {
                const auto place = [&](uint8_t position, uint8_t center) {
                    uint8_t distance = static_cast<uint8_t>(position - center);
                    bool below = false;
                    if (distance & 0x80) {
                        distance = negate(distance);
                        below = true;
                    }
                    const uint8_t high = static_cast<uint8_t>((scale * distance) >> 8);
                    return static_cast<uint8_t>((below ? static_cast<uint8_t>((0xFF ^ high) + 1) : high) + center);
                };
                record.x = place(data_.tiles[at], centerX);
                record.y = place(data_.tiles[at + 1], centerY);
                record.tile = static_cast<uint16_t>(data_.tiles[at + 2] | (data_.tiles[at + 3] << 8));
            }
            records_.push_back(record);
        }
        index = (index - 1) & 0x3F;
    }
    step_ = (step_ + 1) & 0x3F;
}

// UpdateTrainGfx: the picture is cleared, then each tile is copied where it goes, the pixels that are not clear only; each
// picture of a tile is drawn from its bottom up, centered on its place.
void TrainRide::drawTiles() {
    for (int row = 0; row < FF6_TRAIN_BUFFER; ++row) {
        const size_t at = static_cast<size_t>(kPictureStart - kBufferStart + row * 256);
        std::fill(buffer_.begin() + static_cast<std::ptrdiff_t>(at), buffer_.begin() + static_cast<std::ptrdiff_t>(at) + FF6_TRAIN_BUFFER, static_cast<uint8_t>(0));
    }
    size_t record = 0;
    for (int group = 0; group < 11; ++group) {
        const int size = groupSize(group);
        for (int i = 0; i < kGroupLayers[group] * FF6_TRAIN_TILES_PER_LAYER && record < records_.size(); ++i, ++record) {
            const Record& tile = records_[record];
            const int position = tile.x | (tile.y << 8);
            if (position == 0) continue;
            const size_t pointerIndex = static_cast<size_t>((tile.tile >> 1) + 10 - group);
            if (pointerIndex >= data_.tilePointers.size()) continue;
            size_t pixel = data_.tilePointers[pointerIndex];
            int destination = kBufferStart - ((size - 1) >> 1) + position;
            for (int row = 0; row < size; ++row) {
                for (int column = 0; column < size; ++column) {
                    const uint8_t color = pixel < memory_.size() ? memory_[pixel] : static_cast<uint8_t>(0);
                    ++pixel;
                    const int address = destination + column - kBufferStart;
                    if (color != 0 && address >= 0 && address < static_cast<int>(buffer_.size())) buffer_[static_cast<size_t>(address)] = color;
                }
                destination -= 0x100;
            }
        }
    }
}

bool TrainRide::takeBattle(int* group, bool* second, int* background) {
    if (!battleReady_) return false;
    battleReady_ = false;
    *group = battleGroup_;
    *second = battleSecond_;
    *background = battleBackground_;
    return true;
}

// MagitekTrain after a battle: the stretch that asked for it is loaded again.
void TrainRide::resume() {
    if (!waitingForBattle_) return;
    courseOffset_ -= FF6_TRAIN_COURSE_RECORD_BYTES;
    start();
}

void TrainRide::draw(uint8_t* rgba) {
    for (int row = 0; row < FF6_TRAIN_BUFFER; ++row) {
        for (int column = 0; column < FF6_TRAIN_BUFFER; ++column) {
            uint16_t& word = ppu_.vram[static_cast<size_t>((FF6_TRAIN_BUFFER_Y + row) * 128 + FF6_TRAIN_BUFFER_X + column)];
            word = static_cast<uint16_t>((word & 0xFF00) | buffer_[static_cast<size_t>(kPictureStart - kBufferStart + row * 256 + column)]);
        }
    }
    for (PpuLine& line : ppu_.lines) {
        line.bgMode = 7;
        line.m7a = line.m7d = FF6_TRAIN_SCALE;
        line.m7b = line.m7c = 0;
    }
    ppu_.m7x = 0x200;
    ppu_.m7y = 0x1E0;
    ppu_.m7hofs = 0x180;
    ppu_.m7vofs = 0x170;
    ppu_.brightness = static_cast<uint8_t>(brightness_);
    ppu_.render(rgba);
    // The window of the game hides the columns at the sides of the screen.
    uint8_t backdrop[FF6_BYTES_PER_PIXEL];
    colorToRgba(ppu_.cgram[0], backdrop);
    for (int c = 0; c < FF6_ALPHA_OFFSET; ++c) backdrop[c] = static_cast<uint8_t>(backdrop[c] * ppu_.brightness / 15);
    backdrop[FF6_ALPHA_OFFSET] = 255;
    for (int y = 0; y < FF6_SCREEN_HEIGHT; ++y) {
        for (int x = 0; x < FF6_SCREEN_WIDTH; ++x) {
            if (x >= FF6_WORLD_BORDER && x < FF6_SCREEN_WIDTH - FF6_WORLD_BORDER) continue;
            std::copy(backdrop, backdrop + FF6_BYTES_PER_PIXEL, rgba + (static_cast<size_t>(y) * FF6_SCREEN_WIDTH + x) * FF6_BYTES_PER_PIXEL);
        }
    }
}

}  // namespace ff6
