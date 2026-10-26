#pragma once

// Named constants shared by the game code. Values come from the SNES game or from the
// .ff6scene layout written by tools/scene.mjs. Graphics libraries never appear here.

// ---- Screen and pixels ----------------------------------------------------------------
#define FF6_SCREEN_WIDTH 256
#define FF6_SCREEN_HEIGHT 224
#define FF6_FRAMES_PER_SECOND 60
#define FF6_BYTES_PER_PIXEL 4  // RGBA, one byte per channel
#define FF6_ALPHA_OFFSET 3  // position of the alpha byte in a pixel
#define FF6_BITS_PER_PIXEL (FF6_BYTES_PER_PIXEL * 8)

// ---- Map, movement and sprites --------------------------------------------------------
#define FF6_DIRECTION_COUNT 4
#define FF6_NO_DIRECTION (-1)
#define FF6_TILE_SIZE 16  // map tile edge, in pixels
#define FF6_MAP_TILE_BYTES (FF6_TILE_SIZE * FF6_TILE_SIZE * FF6_BYTES_PER_PIXEL)
#define FF6_STEP_PIXELS_PER_FRAME 1
#define FF6_STEP_FRAMES (FF6_TILE_SIZE / FF6_STEP_PIXELS_PER_FRAME)
#define FF6_WALK_POSE_COUNT 4
#define FF6_WALK_POSE_MASK (FF6_WALK_POSE_COUNT - 1)
#define FF6_WALK_POSE_SHIFT 3  // the pose changes every 8 pixels walked
#define FF6_Z_LEVEL_1 1
#define FF6_Z_LEVEL_2 2

#define FF6_FRAME_WIDTH 16
#define FF6_FRAME_HEIGHT 24
#define FF6_FRAME_COUNT 12
#define FF6_FRAME_BYTES (FF6_FRAME_WIDTH * FF6_FRAME_HEIGHT * FF6_BYTES_PER_PIXEL)
#define FF6_SPRITE_TOP_HEIGHT 16  // upper body, drawn apart so that tiles can cover only it
#define FF6_SPRITE_BOTTOM_HEIGHT (FF6_FRAME_HEIGHT - FF6_SPRITE_TOP_HEIGHT)
#define FF6_SPRITE_Y_OFFSET 16  // the upper sprite covers the tile above, the lower one the top of its own tile

// ---- Tile properties: one 16-bit word per map cell -------------------------------------
// The high byte holds the passability bits, the low byte holds level and priority flags.
#define FF6_PROP_LOW_MASK 0xFF
#define FF6_PROP_HIGH_SHIFT 8
#define FF6_PROP_PASSABILITY_MASK 0x0F
#define FF6_PASS_UP 0x08
#define FF6_PASS_RIGHT 0x01
#define FF6_PASS_DOWN 0x04
#define FF6_PASS_LEFT 0x02

#define FF6_TILE_LEVEL_1 0x01
#define FF6_TILE_LEVEL_2 0x02
#define FF6_TILE_LEVEL_MASK 0x03  // both bits set: transition tile, usable from either level
#define FF6_TILE_BRIDGE 0x04
#define FF6_TILE_PRIORITY_TOP 0x08  // the upper body of the sprite is drawn above the tile
#define FF6_TILE_PRIORITY_BOTTOM 0x10
#define FF6_TILE_TYPE_MASK 0x07  // level and bridge bits
#define FF6_TILE_IMPASSABLE 0x07  // counter or wall: every type bit set
#define FF6_TILE_DOOR 0x20  // low byte of a tile property: the tile is a door that opens
#define FF6_TILE_DOOR_SOLID 0xF7  // a door with this property does not open
#define FF6_DOOR_SMALL_1 0x15  // tile numbers of the doors, when they are closed
#define FF6_DOOR_SMALL_2 0x17
#define FF6_DOOR_BIG 0x1C
#define FF6_SFX_DOOR_OPEN 0x2C

// ---- Font ----------------------------------------------------------------------------
#define FF6_FONT_WIDTH_COUNT 256  // one advance width per character code
#define FF6_GLYPH_COUNT 96
#define FF6_FIRST_GLYPH_CODE 0x20  // lower codes are dialogue controls
#define FF6_GLYPH_ROWS 11
#define FF6_GLYPH_ROW_BITS 16  // each glyph row is one 16-bit word
#define FF6_GLYPH_LEFT_BIT 0x8000
#define FF6_CODE_SPACE 0x7F
#define FF6_CODE_WIDE_SPACE 0xFF

// ---- Dialogue controls (names follow tools/text.mjs) -----------------------------------
#define FF6_DLG_END 0x00
#define FF6_DLG_NEWLINE 0x01
#define FF6_DLG_WAIT_FIXED 0x10  // no argument
#define FF6_DLG_WAIT 0x11  // one argument: duration in FF6_DLG_WAIT_UNIT_FRAMES
#define FF6_DLG_KEY 0x12  // waits for the confirm button
#define FF6_DLG_PAGE 0x13  // waits for the confirm button, then clears the window
#define FF6_DLG_TAB 0x14  // one argument: number of spaces
#define FF6_DLG_CHOICE 0x15  // advances by the wide-space width
#define FF6_DLG_KEY_TIMED 0x16  // one argument: waits that long, then for the confirm button
#define FF6_DLG_WAIT_FIXED_FRAMES 60
#define FF6_DLG_WAIT_UNIT_FRAMES 15

// ---- Dialogue window and text (pixels, unless noted) -----------------------------------
#define FF6_WINDOW_X 8
#define FF6_WINDOW_Y 8
#define FF6_WINDOW_COLS 30  // in window tiles
#define FF6_WINDOW_ROWS 10
#define FF6_WINDOW_TILE_SIZE 8
#define FF6_WINDOW_TILE_COUNT 28
#define FF6_WINDOW_TILE_BYTES (FF6_WINDOW_TILE_SIZE * FF6_WINDOW_TILE_SIZE * FF6_BYTES_PER_PIXEL)
#define FF6_WINDOW_FILL_SIZE 4  // the interior repeats a 4x4 block made of tiles 0-15
#define FF6_WINDOW_TILE_TOP_LEFT 16
#define FF6_WINDOW_TILE_TOP_RIGHT 19
#define FF6_WINDOW_TILE_TOP_ODD 17  // top edge, odd columns
#define FF6_WINDOW_TILE_TOP_EVEN 18
#define FF6_WINDOW_TILE_BOTTOM_LEFT 24
#define FF6_WINDOW_TILE_BOTTOM_RIGHT 27
#define FF6_WINDOW_TILE_BOTTOM_ODD 25
#define FF6_WINDOW_TILE_BOTTOM_EVEN 26
#define FF6_WINDOW_TILE_LEFT_ODD 20  // left edge, odd rows
#define FF6_WINDOW_TILE_LEFT_EVEN 22
#define FF6_WINDOW_TILE_RIGHT_ODD 21
#define FF6_WINDOW_TILE_RIGHT_EVEN 23

#define FF6_TEXT_ORIGIN_X 12  // where the text area starts on the screen
#define FF6_TEXT_ORIGIN_Y 12
#define FF6_TEXT_START_X 4  // left margin inside the text area
#define FF6_TEXT_MAX_X 0xE0  // a line may not reach this x
#define FF6_TEXT_LINE_COUNT 4
#define FF6_TEXT_LINE_HEIGHT 16
#define FF6_TEXT_BASELINE_OFFSET 4
#define FF6_TEXT_SHADOW_OFFSET 1
#define FF6_TEXT_SHADOW_RGBA {0, 0, 0, 255}
#define FF6_TEXT_INK_RGBA {248, 248, 248, 255}
#define FF6_DLG_FLAG_BOTTOM 0x8000  // flags of the dialogue command, above the 14-bit dialogue id
#define FF6_DLG_FLAG_TEXT_ONLY 0x4000
#define FF6_DLG_ID_MASK 0x3FFF
#define FF6_WINDOW_BOTTOM_Y (FF6_SCREEN_HEIGHT - FF6_WINDOW_Y - FF6_WINDOW_ROWS * FF6_WINDOW_TILE_SIZE)
#define FF6_CURSOR_SIZE 7  // the choice cursor is a small triangle, in pixels
#define FF6_CURSOR_X_OFFSET 2
#define FF6_CURSOR_Y_OFFSET 4

// ---- Game data pack (written by tools/pack.mjs) -----------------------------------------
#define FF6_PACK_MAGIC_SIZE 8
#define FF6_COMMON_MAGIC "FF6COMMN"
#define FF6_MAP_MAGIC "FF6MAP01"
#define FF6_PACK_VERSION 4
#define FF6_COMMON_FILE "common.ff6data"
#define FF6_MAP_DIR "maps"
#define FF6_MAP_FILE_EXTENSION ".ff6map"
#define FF6_FIRST_FIELD_MAP 3  // maps 0-2 are the world maps
#define FF6_MAX_PACK_ITEMS 100000
#define FF6_SPRITE_SHEET_BYTES (FF6_FRAME_COUNT * FF6_FRAME_WIDTH * FF6_FRAME_HEIGHT)
#define FF6_SPRITE_PAL_COLORS FF6_PALETTE_COLORS
#define FF6_PLAYER_SPRITE_GFX 0  // Terra
#define FF6_PLAYER_SPRITE_PAL 2

// ---- Map data ------------------------------------------------------------------------
#define FF6_MAP_PROP_SIZE 33
#define FF6_MAP_PROP_FLAGS 2  // bit 7: bg3 is drawn in front of everything
#define FF6_BG3_FOREGROUND_FLAG 0x80
#define FF6_MAP_PROP_BG2_OFFSET_X 18  // in map tiles
#define FF6_MAP_PROP_BG2_OFFSET_Y 19
#define FF6_MAP_PROP_BG3_OFFSET_X 20
#define FF6_MAP_PROP_BG3_OFFSET_Y 21
#define FF6_MAP_PROP_SIZES_12 23  // 2 bits each: bg1 width, bg1 height, bg2 width, bg2 height
#define FF6_MAP_PROP_SIZES_3 24  // bits 6-7 bg3 width, bits 4-5 bg3 height
#define FF6_MAP_PROP_ANIMATION 27  // bits 0-4 bg1/bg2 tile animation, bits 5-7 bg3 animation
#define FF6_MAP_PROP_LIMIT_WIDTH 30  // playable size in tiles, 0 for none
#define FF6_MAP_PROP_LIMIT_HEIGHT 31
#define FF6_BG_ANIM_MASK 0x1F
#define FF6_BG3_ANIM_SHIFT 5
#define FF6_LAYER_BASE_TILES 16  // a layer is 16, 32, 64 or 128 tiles wide
#define FF6_LAYER_COUNT 3
#define FF6_BG1 0
#define FF6_BG2 1
#define FF6_BG3 2
#define FF6_TILE_PROPS_BYTES 0x200  // 256 low bytes, then 256 high bytes
#define FF6_TILE_PROPS_HIGH 0x100
#define FF6_TILESET_WORDS 0x400
#define FF6_TILESET_TILES 256  // a tileset word of quadrant q for tile t is word q * 256 + t
#define FF6_VRAM_BYTES 0x10000
#define FF6_TILE4_BYTES 32  // 4bpp tile
#define FF6_TILE2_BYTES 16  // 2bpp tile
#define FF6_SUBTILE_SIZE 8
#define FF6_MAP_PALETTE_COLORS 128
#define FF6_BG3_PALETTE_BYTES 0x40
#define FF6_BG3_GFX_BYTES 0x1000
#define FF6_BG3_COLORS 4  // colors per 2bpp palette
#define FF6_BG3_PAL_MASK 0x1C
#define FF6_BG3_PAL_SHIFT 2
#define FF6_BG3_TILE_MASK 0x3F  // a bg3 cell is 4 consecutive 2bpp tiles
#define FF6_BG3_HFLIP 0x40
#define FF6_BG3_VFLIP 0x80
#define FF6_TILE_WORD_INDEX_MASK 0x3FF  // SNES tilemap word of a 16x16 tile quadrant
#define FF6_TILE_WORD_PAL_SHIFT 10
#define FF6_TILE_WORD_PAL_MASK 7
#define FF6_TILE_WORD_PRIORITY 0x2000
#define FF6_TILE_WORD_HFLIP 0x4000
#define FF6_TILE_WORD_VFLIP 0x8000
#define FF6_COLOR_BITS 5  // BGR555
#define FF6_PALETTE_COLORS 16  // colors of a 4bpp palette

// ---- Color math (the PPU registers $212c, $212d and $2131) ----------------------------------
#define FF6_COLOR_MATH_BYTES 3  // color math designation, main screen layers, sub screen layers
#define FF6_SCREEN_BG1 0x01  // layer bits of those registers
#define FF6_SCREEN_BG2 0x02
#define FF6_SCREEN_BG3 0x04
#define FF6_SCREEN_OBJ 0x10
#define FF6_SCREEN_BACKDROP 0x20
#define FF6_COLOR_MATH_HALF 0x40
#define FF6_COLOR_MATH_SUBTRACT 0x80
#define FF6_COLOR_CHANNELS 3
#define FF6_COLOR_MAX 255

// ---- Tile animation and scrolling ----------------------------------------------------
#define FF6_BG_ANIM_TILES 8  // 16x16 blocks of tile graphics that animate
#define FF6_BG_ANIM_FRAMES 4
#define FF6_BG_ANIM_TILE_BYTES 0x80
#define FF6_BG_ANIM_VRAM 0x5000
#define FF6_BG_ANIM_FRAME_SHIFT 9
#define FF6_BG3_ANIM_FRAMES 8
#define FF6_BG3_ANIM_FRAME_SHIFT 8
#define FF6_BG3_ANIM_FRAME_MASK 0x0E  // selects an even word offset into the frame table
#define FF6_ANIM_PERIOD_FRAMES 2  // animations advance at half the frame rate
#define FF6_PARALLAX_BYTES 8
#define FF6_PARALLAX_SPEED_SHIFT 4  // table speeds are in 1/16 of 1/256 pixel per frame
#define FF6_PARALLAX_ONE 16  // scroll multiplier of 1.0
#define FF6_SCROLL_UNITS_PER_PIXEL 256
#define FF6_CAMERA_PARTY_TILE 7  // the party stands in the 8th tile column and row
#define FF6_CAMERA_MIN_X_TILE 8
#define FF6_CAMERA_MIN_Y_TILE 7
#define FF6_CAMERA_MAX_MARGIN 7

// ---- Objects, events and entrances ----------------------------------------------------
#define FF6_NPC_PROP_SIZE 9
#define FF6_EVENT_TRIGGER_SIZE 5
#define FF6_SHORT_ENTRANCE_SIZE 6
#define FF6_LONG_ENTRANCE_SIZE 7
#define FF6_LONG_ENTRANCE_VERTICAL 0x80
#define FF6_ENTRANCE_MAP_MASK 0x1FF
#define FF6_ENTRANCE_SET_PARENT 0x200
#define FF6_ENTRANCE_PARENT_MAP 0x1FF  // leave the sub-map for the map it was entered from
#define FF6_ENTRANCE_FACING_SHIFT 12
#define FF6_NPC_SWITCH_BYTES 128
#define FF6_NPC_SWITCH_EVENT_BIT 0x300  // npc switch n is event bit 0x300 + n
#define FF6_EVENT_BIT_COUNT 0x700
#define FF6_NPC_PAL_SHIFT 2
#define FF6_NPC_PAL_MASK 7
#define FF6_NPC_SWITCH_SHIFT 6
#define FF6_NPC_SPECIAL_FLAG 0x80
#define FF6_NPC_X_MASK 0x7F
#define FF6_NPC_Y_MASK 0x3F
#define FF6_NPC_DIR_MASK 3
#define FF6_NPC_BANK_MASK 3
#define FF6_NPC_NO_TURN 4
#define FF6_NPC_SCROLL_BG2 0x20
#define FF6_NPC_ANIM_FRAME_SHIFT 5
#define FF6_NPC_LAYER_PRIORITY_SHIFT 3
#define FF6_NPC_LAYER_PRIORITY_MASK 3
#define FF6_PROP_NPC_RANDOM 0x80  // high byte of a tile property: an npc may wander onto the tile

// ---- Object movement and scripts (obj.asm) ----------------------------------------------
#define FF6_OBJ_MOVE_NONE 0  // movement types, the low nibble of the object's flags
#define FF6_OBJ_MOVE_SCRIPT 1
#define FF6_OBJ_MOVE_USER 2
#define FF6_OBJ_MOVE_RANDOM 3
#define FF6_OBJ_MOVE_ACTIVATED 4
#define FF6_FIRST_NPC_OBJECT 0x10  // objects 0x10 to 0x2F are the npcs of the map
#define FF6_LAST_NPC_OBJECT 0x2F
#define FF6_CHAR_COUNT 16  // objects 0x00 to 0x0F are the characters
#define FF6_CHARACTER_NAMES \
    {"terra", "locke", "cyan", "shadow", "edgar", "sabin", "celes", "strago", "relm", "setzer", "mog", "gau", "gogo", "umaro", "leo", "banon"}
// The sprite palette slot that the events give to each character when it joins (the slot of the commands 0x43 that
// come with a command 0x37 for the same character).
#define FF6_CHARACTER_PALETTES {2, 1, 4, 4, 0, 0, 0, 3, 3, 4, 5, 3, 3, 5, 1, 3}
#define FF6_OBJ_CAMERA 0x30
#define FF6_OBJ_PARTY_FIRST 0x31  // objects 0x31 to 0x34 are the characters of the active party
#define FF6_OBJ_PARTY_LAST 0x34
#define FF6_PARTY_SIZE 4
#define FF6_PARTY_COUNT 8  // party numbers 1 to 7, 0 for no party
#define FF6_SPRITE_PALETTE_SLOTS 8
#define FF6_SPRITE_GFX_TOTAL 165  // map sprites and palettes that the game has
#define FF6_SPRITE_PALETTE_TOTAL 60
#define FF6_SAVE_VERSION 1
#define FF6_SAVE_DEFAULT_FILE "ff6-save.json"
#define FF6_MAX_TILE 255
#define FF6_BYTE_VALUES 256
#define FF6_MAX_COLOR 0x7FFF  // BGR555
#define FF6_OBJ_SUBPIXELS 256  // object positions are in 1/256 pixel
#define FF6_OBJ_TILE_SUBPIXELS (FF6_TILE_SIZE * FF6_OBJ_SUBPIXELS)
#define FF6_OBJ_ACTION_PERIOD 4  // every object acts once every 4 frames, 6 objects per frame
#define FF6_OBJ_ACTIONS_PER_FRAME 6
#define FF6_OBJ_UPDATED_MAX 24  // objects past this place in the object list never act
#define FF6_OBJ_LIST_FIRST_NPC 2  // the party and the camera come first in the list
#define FF6_OBJ_SPEED_UNIT 0x40  // base speed per frame, in subpixels, times the speed multiplier
#define FF6_OBJ_SPEED_COUNT 10
#define FF6_OBJ_DEFAULT_SPEED 2  // 1 pixel per frame
#define FF6_OBJ_DIAGONAL_FIRST 5  // movement directions 1 to 4 are straight, then come the diagonals
#define FF6_OBJ_SCRIPT_STEPS_PER_ACTION 256  // guards against a script loop that never waits
#define FF6_OBJ_JUMP_LOW 0x0F
#define FF6_OBJ_JUMP_HIGH 0x5F
#define FF6_OBJ_JUMP_MASK 0x3F
#define FF6_OBJ_PRIORITY_NORMAL 0
#define FF6_OBJ_PRIORITY_TOP_ABOVE 1
#define FF6_OBJ_PRIORITY_ABOVE 2
#define FF6_OBJ_PRIORITY_BELOW 3

#define FF6_OBJ_CMD_FRAME_LAST 0x7F  // 0x00-0x7F: show that frame of the sprite
#define FF6_OBJ_CMD_WALK 0x80  // 0x80-0x9F: walk, bits 0-1 direction, bits 2-4 tiles - 1
#define FF6_OBJ_CMD_WALK_LAST 0x9F
#define FF6_OBJ_CMD_DIAGONAL 0xA0  // 0xA0-0xAF: one diagonal step
#define FF6_OBJ_CMD_DIAGONAL_LAST 0xAF
#define FF6_OBJ_DIAGONAL_BASE 0x9C
#define FF6_OBJ_CMD_SPEED 0xC0  // 0xC0-0xC5
#define FF6_OBJ_CMD_SPEED_LAST 0xC5
#define FF6_OBJ_CMD_WALK_ANIM_ON 0xC6
#define FF6_OBJ_CMD_WALK_ANIM_OFF 0xC7
#define FF6_OBJ_CMD_PRIORITY 0xC8
#define FF6_OBJ_CMD_VEHICLE 0xC9
#define FF6_OBJ_CMD_TURN_UP 0xCC  // 0xCC-0xCF
#define FF6_OBJ_CMD_TURN_LAST 0xCF
#define FF6_OBJ_CMD_SHOW 0xD0
#define FF6_OBJ_CMD_HIDE 0xD1
#define FF6_OBJ_CMD_SET_POSITION 0xD5
#define FF6_OBJ_CMD_SCROLL_TO 0xD7
#define FF6_OBJ_CMD_JUMP_LOW 0xDC
#define FF6_OBJ_CMD_JUMP_HIGH 0xDD
#define FF6_OBJ_CMD_PAUSE 0xE0
#define FF6_OBJ_CMD_SET_BIT_0 0xE1  // 0xE1-0xE3 set, 0xE4-0xE6 clear: a bit of the first, second or third 256
#define FF6_OBJ_CMD_CLEAR_BIT_0 0xE4
#define FF6_OBJ_CMD_CLEAR_BIT_LAST 0xE6
#define FF6_OBJ_CMD_EVENT 0xF9
#define FF6_OBJ_CMD_RANDOM_BACK 0xFA
#define FF6_OBJ_CMD_RANDOM_FORWARD 0xFB
#define FF6_OBJ_CMD_BACK 0xFC
#define FF6_OBJ_CMD_FORWARD 0xFD
#define FF6_OBJ_CMD_END 0xFF

#define FF6_EVENT_MAX_OBJ_OP 0x34  // opcodes up to this are object scripts: id, length | flags, bytes
#define FF6_EVENT_OBJ_LENGTH_MASK 0x7F
#define FF6_EVENT_OBJ_WAIT 0x80  // the event waits until the script has ended
#define FF6_EVENT_WAIT_OBJ 0x35
#define FF6_EVENT_LOCK_SCREEN 0x38
#define FF6_EVENT_UNLOCK_SCREEN 0x39
#define FF6_EVENT_PARTY_USER 0x3A
#define FF6_EVENT_PARTY_SCRIPT 0x3B
#define FF6_EVENT_SET_PARTY 0x3C
#define FF6_EVENT_ADD_CHAR 0x3F
#define FF6_EVENT_ACTIVATE_PARTY 0x46
#define FF6_EVENT_UPDATE_PARTY 0x47
#define FF6_EVENT_SET_PALETTE 0x60
#define FF6_EVENT_PARTY_MAP 0x79
#define FF6_EVENT_MOVE_PARTY 0x7E
#define FF6_EVENT_OBJ_PASS_OFF 0x36
#define FF6_EVENT_OBJ_GFX 0x37
#define FF6_EVENT_OBJ_CREATE 0x3D
#define FF6_EVENT_OBJ_DELETE 0x3E
#define FF6_EVENT_OBJ_SHOW 0x41
#define FF6_EVENT_OBJ_HIDE 0x42
#define FF6_EVENT_OBJ_PAL 0x43
#define FF6_EVENT_OBJ_PASS_ON 0x78
#define FF6_EVENT_OBJ_EVENT 0x7A
#define FF6_EVENT_OBJ_TOUCH_ON 0x7C
#define FF6_EVENT_OBJ_TOUCH_OFF 0x7D
#define FF6_EVENT_MAP_DATA_NOW 0x73
#define FF6_EVENT_MAP_DATA 0x74
#define FF6_MAP_DATA_HEADER_BYTES 4  // x, layer and y, width, height
#define FF6_MAP_DATA_Y_MASK 0x3F
#define FF6_MAP_DATA_LAYER_SHIFT 6
#define FF6_EVENT_PAUSE_15F 0x91  // 0x92 to 0x94: 30, 45 and 60 frames
#define FF6_EVENT_PAUSE_120F 0x95
#define FF6_EVENT_PAUSE 0xB4  // one argument: frames
#define FF6_EVENT_PAUSE_LONG 0xB5  // one argument: 15 frames each
#define FF6_EVENT_LONG_PAUSE_UNIT 15
#define FF6_EVENT_LOOP 0xB0  // one argument: count
#define FF6_EVENT_LOOP_END 0xB1
#define FF6_EVENT_CALL_LOOP 0xB3  // count, then the address
#define FF6_EVENT_BATTLE_JUMP 0xB7  // jumps when the battle flag is clear
#define FF6_EVENT_BATTLE_SET 0xB8
#define FF6_EVENT_BATTLE_CLEAR 0xB9
#define FF6_EVENT_LOOP_UNTIL 0xBC  // loops while an event bit is clear
#define FF6_EVENT_RANDOM_JUMP 0xBD  // jumps half of the time
#define FF6_EVENT_CASE 0xBE
#define FF6_BATTLE_FLAG_BYTES 32
#define FF6_RANDOM_HALF 0x80
#define FF6_EVENT_DLG_ASYNC 0x48
#define FF6_EVENT_DLG 0x4B
#define FF6_EVENT_LOAD_MAP_FIRST 0x6A
#define FF6_EVENT_LOAD_MAP_LAST 0x6C
#define FF6_EVENT_LOAD_MAP_SIZE 6
#define FF6_EVENT_CALL 0xB2
#define FF6_EVENT_CHOICE 0xB6
#define FF6_EVENT_IF_ANY 0xC0  // + n - 1 for n switches
#define FF6_EVENT_IF_ALL 0xC8
#define FF6_EVENT_IF_LAST 0xCF
#define FF6_EVENT_SET_SWITCH 0xD0  // + 2 * (switch >> 8), +1 to clear
#define FF6_EVENT_SWITCH_LAST 0xDD
#define FF6_EVENT_RETURN 0xFE
#define FF6_EVENT_ADDRESS_SIZE 3
#define FF6_EVENT_SWITCH_EXPECT_SET 0x8000
#define FF6_EVENT_STACK_SIZE 16
#define FF6_EVENT_COMMANDS_PER_TICK 64
#define FF6_EVENT_OBJECT_WAIT_LIMIT 1800  // frames: an event gives up waiting for an object after this long
#define FF6_EVENT_SOUND_WAIT_LIMIT 3600  // and for the sound program
#define FF6_EVENT_MAP_STARTUP_FLAG 0x80  // in the last byte of the load map commands
#define FF6_EVENT_MAP_NO_FADE_IN_FLAG 0x40
#define FF6_EVENT_FADE_IN_SPEED 0x59  // speed
#define FF6_EVENT_FADE_OUT_SPEED 0x5A  // speed
#define FF6_EVENT_FADE_STOP 0x5B
#define FF6_EVENT_FADE_WAIT 0x5C
#define FF6_EVENT_FADE_IN 0x96
#define FF6_EVENT_FADE_OUT 0x97
#define FF6_BRIGHTNESS_FULL 0xF0  // the screen brightness is a byte that the PPU gets as its high nibble
#define FF6_BRIGHTNESS_STEP 0x10  // the speed of a fade in or out, and where a fade in starts
#define FF6_FADE_OUT_FLAG 0x80
#define FF6_FADE_SPEED_MASK 0x1F
#define FF6_BRIGHTNESS_SHIFT 4
#define FF6_BRIGHTNESS_LEVELS 16
#define FF6_EVENT_PLAY_SONG_VOLUME 0xEF  // song, volume
#define FF6_EVENT_PLAY_SONG 0xF0
#define FF6_EVENT_PLAY_SFX 0xF4  // sound effect
#define FF6_EVENT_PLAY_SFX_PAN 0xF5  // sound effect, pan, pan speed
#define FF6_EVENT_FADE_IN_SONG 0xF1  // song, speed
#define FF6_EVENT_FADE_OUT_SONG 0xF2  // speed
#define FF6_EVENT_FADE_IN_PREVIOUS 0xF3  // speed
#define FF6_EVENT_SOUND_COMMAND 0xF6  // command and its two arguments
#define FF6_EVENT_CONTINUE_SONG 0xF7
#define FF6_EVENT_WAIT_SOUND 0xF8
#define FF6_EVENT_WAIT_SONG_POSITION 0xF9
#define FF6_EVENT_WAIT_SONG_END 0xFA

#define FF6_AUDIO_MAGIC "FF6AUDIO"
#define FF6_SONG_SAMPLE_SLOTS 16  // instruments of a song
#define FF6_BRR_BLOCK_BYTES 9
#define FF6_BRR_BLOCK_SAMPLES 16
#define FF6_BRR_FLAG_END 0x01
#define FF6_BRR_FLAG_LOOP 0x02
#define FF6_BRR_FILTER_SHIFT 2
#define FF6_BRR_FILTER_MASK 3
#define FF6_BRR_SHIFT_SHIFT 4  // the range is the high nibble of the header
#define FF6_BRR_MAX_RANGE 12  // higher ranges only keep the sign
#define FF6_DSP_SAMPLE_RATE 32000
#define FF6_DSP_PITCH_ONE 0x1000  // one sample of the source per output sample
#define FF6_DSP_PITCH_MASK 0x3FFF
#define FF6_DSP_ENV_MAX 0x7FF
#define FF6_DSP_ENV_RELEASE_STEP 8
#define FF6_DSP_ADSR_AR_MASK 0x0F  // ADSR1: attack, then decay in bits 4-6
#define FF6_DSP_ADSR_DR_SHIFT 4
#define FF6_DSP_ADSR_DR_MASK 7
#define FF6_DSP_ADSR_SR_MASK 0x1F  // ADSR2: sustain rate, then sustain level in bits 5-7
#define FF6_DSP_ADSR_SL_SHIFT 5
#define FF6_DSP_RATE_COUNT 32
#define FF6_DSP_COUNTER_RANGE 30720  // the global counter of the chip counts down from this value
#define FF6_DSP_ATTACK_STEP 32
#define FF6_DSP_ATTACK_FAST_STEP 1024
#define FF6_DSP_VOLUME_SHIFT 7  // channel volumes are signed bytes, 128 is full scale

#define FF6_AKAO_VOICES 8
#define FF6_AKAO_TICK_SAMPLES 156  // the script clock of 4.875 ms, at 32 kHz
#define FF6_AKAO_HEADER_BYTES 0x24  // base address, end pointer, 2 x 8 channel pointers
#define FF6_AKAO_END_POINTER_OFFSET 2
#define FF6_AKAO_CHANNEL_POINTERS_OFFSET 4
#define FF6_AKAO_FIRST_COMMAND 0xC4  // lower codes are notes
#define FF6_AKAO_NOTE_DIVISOR 14  // a note code is 14 * pitch + duration
#define FF6_AKAO_TIE_FIRST 0xA8
#define FF6_AKAO_REST_FIRST 0xB6
#define FF6_AKAO_END 0xEB
#define FF6_AKAO_SONG_INSTRUMENT_BASE 0x20  // the instruments of a song come after those of the sound effects
#define FF6_AKAO_LOOP_DEPTH 4
#define FF6_AKAO_DURATION_KEY_OFF 2  // ticks before the end of a note at which the voice is released
#define FF6_AKAO_MASTER_VOLUME 0x3F  // the DSP master volume that the sound program sets
#define FF6_AKAO_ECHO_DELAY 5  // the delay that the sound program starts with, in steps of 16 ms
#define FF6_AKAO_ECHO_DELAY_MAX 15
#define FF6_AKAO_ECHO_FRAMES_PER_DELAY 512  // 2 KB of the sound memory, 4 bytes a stereo frame
#define FF6_AKAO_FIR_TAPS 8
#define FF6_SPC_MEMORY_BYTES 0x10000
#define FF6_SPC_DIRECTORY 0x1B00  // start and loop address of each instrument
#define FF6_SPC_FREQ_MULT 0x1A00  // pitch multiplier of each instrument
#define FF6_SPC_ADSR 0x1A80  // ADSR1 and ADSR2 of each instrument
#define FF6_SFX_SAMPLE_COUNT 8
#define FF6_SFX_POINTERS 0x2C00  // two script addresses for each game sound effect
#define FF6_SFX_POINTER_BYTES 4
#define FF6_SYSTEM_SFX_TABLE 0x17DF  // script address of each system sound effect
#define FF6_SFX_DEFAULT_INSTRUMENT 4
#define FF6_SFX_DEFAULT_VOLUME 0x41
#define FF6_SFX_DEFAULT_OCTAVE 3
#define FF6_SFX_TEMPO 0x78  // the sound effects keep their own tempo
#define FF6_SFX_FIRST_VOICE 4  // the sound effects use the voices 4 to 7
#define FF6_SFX_PAN_CENTER 0x80
#define FF6_AKAO_INTERRUPT_ENV_TICKS 7  // ticks between two steps of the volume that the game sets

// Commands of the game to the sound program (ExecSound), and the events that send them
#define FF6_SOUND_PLAY_SONG 0x10  // song, volume
#define FF6_SOUND_PAUSE_SONG 0x14  // like the play command, and the song that plays is kept to resume it
#define FF6_SOUND_GAME_SFX 0x18  // sound effect, pan: from here to 0x1F
#define FF6_SOUND_SYSTEM_SFX_FIRST 0x20  // the effect is in the low 4 bits
#define FF6_SOUND_SYSTEM_SFX_LAST 0x2F
#define FF6_SOUND_QUICK_SONG_FIRST 0x30  // the song is the low 4 bits, at full volume
#define FF6_SOUND_QUICK_SONG_LAST 0x3F
#define FF6_SOUND_INTERRUPT_FLAG 0x80
#define FF6_SOUND_SET_VOLUME 0x80  // duration, volume of the song and of the sound effects
#define FF6_SOUND_SET_SONG_VOLUME 0x81  // duration, volume of the song
#define FF6_SOUND_SET_SFX_VOLUME 0x82  // duration, volume of the sound effects
#define FF6_SOUND_SET_SFX_PAN 0x83  // duration, pan of the sound effects
#define FF6_SOUND_SET_TEMPO_RATIO 0x84  // duration, ratio
#define FF6_SOUND_SET_PITCH_SHIFT 0x85  // duration, change
#define FF6_SOUND_CONTINUE_SONG 0x89
#define FF6_SOUND_STOP_ALL 0xF0
#define FF6_SOUND_STOP_SONG 0xF1
#define FF6_SOUND_STOP_SFX 0xF2
#define FF6_SOUND_SET_ECHO_DELAY 0xFC  // delay in steps of 16 ms
#define FF6_SOUND_PAUSING_SONGS {0x24, 0x38, 0x14, 0x33}  // two battle songs, Nighty Night and another: PauseSongTbl
#define FF6_SOUND_PORT_SONG_POSITION 1
#define FF6_SOUND_PORT_CONTINUE 2
#define FF6_SOUND_PORT_PLAYING 3
#define FF6_SOUND_FULL_VOLUME 0xFF
#define FF6_SOUND_FADE_IN_VOLUME 0x20  // volume at which a song starts that fades in
#define FF6_SOUND_SONG_MASK 0x7F  // in the events, bit 7 asks to keep the song that plays

// Menus (src/menu). The text is drawn on a grid of 8x8 tiles, with the small font.
#define FF6_MENU_FILE "menu.ff6data"
#define FF6_MENU_MAGIC "FF6MENU1"
#define FF6_MENU_FONT_TILES 256
#define FF6_MENU_FONT_TILE_BYTES 16  // 2bpp
#define FF6_MENU_FONT_COLORS 80
#define FF6_MENU_TILE_SIZE 8
#define FF6_MENU_COLUMNS (FF6_SCREEN_WIDTH / FF6_MENU_TILE_SIZE)
#define FF6_MENU_CODE_UPPER_A 0x80  // the codes of the text are the tiles of the font
#define FF6_MENU_CODE_LOWER_A 0x9A
#define FF6_MENU_CODE_ZERO 0xB4
#define FF6_MENU_CODE_SPACE 0xFF
#define FF6_MENU_CODE_DIALOGUE_SHIFT 0x60  // a code of the font of the dialogues plus this is the code of the menus
#define FF6_MENU_CODE_2DOT 0xD3  // symbols of the font
#define FF6_MENU_CODE_UP_ARROW 0xD4
#define FF6_MENU_CODE_WHITE 0xE8  // then black and effect: the signs of the kinds of magic
#define FF6_MENU_CODE_GAUGE_EMPTY 0xF0  // then the partly filled cells at every two codes
#define FF6_MENU_CODE_GAUGE_FULL 0xF8
#define FF6_MENU_CODE_GAUGE_LEFT 0xF9
#define FF6_MENU_CODE_GAUGE_RIGHT 0xFA
#define FF6_MENU_BG1_COLOR_BAR 0x30
#define FF6_CONFIG_BAR_CELLS 8
#define FF6_MENU_CURSOR_TILE_BYTES 32  // 4bpp
#define FF6_MENU_CURSOR_SIZE 16
#define FF6_MENU_CURSOR_PALETTE_COLORS 4  // the hand uses the sprite colors 12 to 15
#define FF6_MENU_CURSOR_FIRST_COLOR 12
#define FF6_MENU_PORTRAIT_COLORS 16
#define FF6_MENU_PORTRAIT_COUNT 19  // the 14 playable characters, a soldier, an imp, Leo, Banon and a ghost
#define FF6_MENU_PORTRAIT_TILES 25  // 5x5 tiles of 4bpp
#define FF6_MENU_PORTRAIT_COLUMNS 5
#define FF6_MENU_PORTRAIT_SIZE (FF6_MENU_PORTRAIT_COLUMNS * FF6_MENU_TILE_SIZE)
#define FF6_MENU_PORTRAIT_TILE_ORDER \
    {0, 1, 2, 3, 8, 16, 17, 18, 19, 9, 4, 5, 6, 7, 10, 20, 21, 22, 23, 11, 13, 14, 15, 24, 12}  // tile of each cell
#define FF6_MENU_PORTRAIT_X 14  // of a character in the front row, and the y of the first one
#define FF6_MENU_PORTRAIT_Y 21
#define FF6_MENU_PORTRAIT_STEP 48
#define FF6_MENU_PORTRAITS_OF_CHARACTER {0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 16, 17}  // Leo and Banon come after two others
#define FF6_MENU_REPEAT_DELAY 8  // frames before a held direction repeats, then it repeats every FF6_MENU_REPEAT_RATE
#define FF6_MENU_REPEAT_RATE 3
#define FF6_MENU_BG1_PALETTE_COLORS 16  // CGRAM colors of a palette of the bg1 text, 4 for the bg3 text
#define FF6_MENU_BG3_PALETTE_COLORS 4
#define FF6_MENU_TEXT_PALETTE_SHIFT 2  // the text color byte is a tile attribute: bits 2 to 4 are the palette
#define FF6_MENU_TEXT_PALETTE_MASK 7
#define FF6_MENU_BG1_DEFAULT 0x20
#define FF6_MENU_BG1_TEAL 0x24
#define FF6_MENU_BG1_GRAY 0x28
#define FF6_MENU_BG3_DEFAULT 0x20
#define FF6_MENU_BG3_GRAY 0x24
#define FF6_MENU_BG3_TEAL 0x2C
#define FF6_SFX_MENU_SELECT 0x20  // system sound effects
#define FF6_SFX_MENU_MOVE 0x21
#define FF6_SFX_MENU_INVALID 0x22
#define FF6_MENU_ITEM_COUNT 7  // Item, Skills, Equip, Relic, Status, Config, Save
#define FF6_MENU_ITEM_ITEM 0
#define FF6_MENU_CONFIG_ITEM 5
#define FF6_MENU_OPTIONS_TEXT_X 24
#define FF6_MENU_OPTIONS_TEXT_Y 2  // the disassembly says 3, which draws Save over the frame; this fits the 13 rows of the window
#define FF6_MENU_CURSOR_X 175
#define FF6_MENU_CURSOR_Y 12  // the table of the game is 18 + 15 * n, which drifts from the lines of text, 16 pixels apart
#define FF6_MENU_CURSOR_STEP 16
#define FF6_MENU_BLOCK_STEP 6  // rows between the blocks of the characters
#define FF6_MENU_FRAMES_PER_HOUR (FF6_FRAMES_PER_SECOND * 3600)
#define FF6_ITEM_COUNT 256  // the items of the game, and the places of the inventory
#define FF6_ITEM_NAME_BYTES 13
#define FF6_ITEM_ROWS 10  // the rows of the list of the item menu
#define FF6_ITEM_ROW_HEIGHT 12
#define FF6_ITEM_LIST_TOP 88  // the first row of the list, in pixels
#define FF6_ITEM_NAME_X 3  // in tiles
#define FF6_ITEM_QUANTITY_X 17
#define FF6_ITEM_CURSOR_X 8
#define FF6_ITEM_CURSOR_DY (-4)  // the hand is higher than the text that it shows
#define FF6_ITEM_OPTION_COUNT 3  // use, arrange, rare
#define FF6_ITEM_OPTION_CURSOR_X {64, 104, 176}
#define FF6_ITEM_OPTION_X {10, 15, 24}  // tiles
#define FF6_ITEM_OPTION_ROW 2
#define FF6_ITEM_OPTION_CURSOR_Y 12
#define FF6_ITEM_PLACEHOLDER_FIRST 233  // the potion, and the next items, in the placeholder inventory
#define FF6_ITEM_PLACEHOLDER_COUNT 14
#define FF6_CONFIG_BATTLE_DEFAULT 0x2A  // wait mode, battle speed 3 and message speed 3
#define FF6_CONFIG_BATTLE_SPEED_MASK 0x07
#define FF6_CONFIG_WAIT 0x08
#define FF6_CONFIG_MESSAGE_SPEED_MASK 0x70
#define FF6_CONFIG_MESSAGE_SPEED_SHIFT 4
#define FF6_CONFIG_COMMAND_SHORT 0x80
#define FF6_CONFIG_REEQUIP_EMPTY 0x10
#define FF6_CONFIG_MONO 0x20
#define FF6_CONFIG_CURSOR_MEMORY 0x40
#define FF6_CONFIG_GAUGE_OFF 0x80
#define FF6_CONFIG_MULTIPLE_CONTROLLERS 0x80
#define FF6_CONFIG_SPEED_MAX 5  // speeds go from 0 to 5, shown as 1 to 6
#define FF6_CONFIG_FIRST_ROW 5  // the tile row of the first option, then one every two rows
#define FF6_CONFIG_FIRST_COLUMN 14
#define FF6_CONFIG_SECOND_COLUMN 22
#define FF6_CONFIG_TEXT_OFFSET_Y 5
#define FF6_CONFIG_CURSOR_X 96
#define FF6_CONFIG_CURSOR_Y 41
#define FF6_CONFIG_CURSOR_STEP 16
#define FF6_CONFIG_ORDER_MASK 0x07  // of the third config byte: the order of the magic
#define FF6_CONFIG_COLOR_MASK 0x38  // the color being edited: 0 for the font, 1 to 7 for the colors of the window
#define FF6_CONFIG_COLOR_SHIFT 3
#define FF6_CONFIG_WINDOW_MASK 0x0F  // of the second config byte: the pattern of the windows
#define FF6_CONFIG_ORDER_MAX 5
#define FF6_CONFIG_WINDOW_MAX 7
#define FF6_CONFIG_COLOR_MAX 7
#define FF6_CONFIG_COMPONENT_MAX 31  // a color is red, green and blue from 0 to 31
#define FF6_CONFIG_FONT_COLOR_DEFAULT 0x7FFF
#define FF6_CONFIG_FONT_COLOR_ENTRIES {3, 21, 23}  // the colors of the font palette that the color of the font replaces
#define FF6_MENU_WINDOW_COUNT 8
#define FF6_MENU_WINDOW_COLORS 7
#define FF6_MENU_WINDOW_GFX_BYTES (FF6_WINDOW_TILE_COUNT * FF6_MENU_CURSOR_TILE_BYTES)
#define FF6_CONFIG_PAGE_SCROLL_FRAMES 16  // a page scrolls by 16 pixels a frame
#define FF6_CONFIG_PAGE_SCROLL_STEP 16
#define FF6_CONFIG_PAGE_PIXELS 256  // the second page is the screen below the first
#define FF6_CONFIG_SCROLL_FIRST (-5)  // the scroll of the first page
#define FF6_CONFIG_CLIP_TOP 39  // the first page and the second are only seen between these lines
#define FF6_CONFIG_CLIP_BOTTOM 199
#define FF6_CONFIG_PAGE1_ROWS 9
#define FF6_CONFIG_PAGE2_ROWS 6
#define FF6_CONFIG_PAGE2_CURSOR_Y {41, 105, 121, 153, 169, 185}
#define FF6_MENU_FRAMES_PER_MINUTE (FF6_FRAMES_PER_SECOND * 60)
#define FF6_MENU_MINUTES_PER_HOUR 60
#define FF6_MENU_MAX_HOURS 99
#define FF6_MENU_PLACEHOLDER_LEVEL 1  // until the statistics of the characters are ported
#define FF6_MENU_PLACEHOLDER_HP 100
#define FF6_MENU_PLACEHOLDER_MP 20

