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
#define FF6_PACK_VERSION 2
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
#define FF6_OBJ_CAMERA 0x30
#define FF6_OBJ_PARTY_FIRST 0x31  // objects 0x31 to 0x34 are the characters of the active party
#define FF6_OBJ_PARTY_LAST 0x34
#define FF6_PARTY_SIZE 4
#define FF6_PARTY_COUNT 8  // party numbers 1 to 7, 0 for no party
#define FF6_SPRITE_PALETTE_SLOTS 8
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

