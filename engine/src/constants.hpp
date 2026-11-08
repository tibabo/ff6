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
#define FF6_FRAME_COUNT 16  // down, up, left, right, then the poses of a rider facing left and right
#define FF6_FRAME_RIDING_LEFT 12  // two frames
#define FF6_FRAME_RIDING_RIGHT 14  // two frames
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
#define FF6_PACK_VERSION 13
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
#define FF6_PLAYABLE_COUNT 14  // the characters that join the party; the other two are guests
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
#define FF6_EVENT_LOAD_MAP_LAST 0x6B
#define FF6_EVENT_SET_PARENT_MAP 0x6C  // the map that the exits of a house lead back to: map and facing, x, y
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
#define FF6_MENU_BG3_YELLOW 0x28
#define FF6_MENU_BG3_TEAL 0x2C
#define FF6_SFX_MENU_SELECT 0x20  // system sound effects
#define FF6_SFX_MENU_MOVE 0x21
#define FF6_SFX_MENU_INVALID 0x22
#define FF6_MENU_ITEM_COUNT 7  // Item, Skills, Equip, Relic, Status, Config, Save
#define FF6_MENU_ITEM_ITEM 0
#define FF6_MENU_FIRST_CHARACTER_ITEM 1  // Skills, Equip, Relic and Status ask for a character
#define FF6_MENU_LAST_CHARACTER_ITEM 4
#define FF6_MENU_SAVE_ITEM 6
#define FF6_MENU_CHARACTER_CURSOR_X 8
#define FF6_MENU_CHARACTER_CURSOR_Y 40
#define FF6_MENU_SAVE_CURSOR_X 192
#define FF6_MENU_SAVE_CURSOR_Y 68
#define FF6_SFX_MENU_SUCCESS 0x23
#define FF6_MENU_CONFIG_ITEM 5
#define FF6_MENU_OPTIONS_TEXT_X 24
#define FF6_MENU_OPTIONS_TEXT_Y 2  // the disassembly says 3, which draws Save over the frame; this fits the 13 rows of the window
#define FF6_MENU_CURSOR_X 175
#define FF6_MENU_CURSOR_Y 12  // the table of the game is 18 + 15 * n, which drifts from the lines of text, 16 pixels apart
#define FF6_MENU_CURSOR_STEP 16
#define FF6_MENU_BLOCK_STEP 6  // rows between the blocks of the characters
#define FF6_MENU_FRAMES_PER_HOUR (FF6_FRAMES_PER_SECOND * 3600)
#define FF6_ITEM_COUNT 256  // the items of the game, and the places of the inventory
#define FF6_ITEM_EMPTY 0xFF
#define FF6_MENU_EQUIP_SLOTS 6  // right hand, left hand, head, body and two relics
#define FF6_MENU_SKILL_LISTS 7  // espers, magic, sword techniques, blitzes, lores, rages and dances
#define FF6_MENU_EQUIP_CURSOR_X 8
#define FF6_EQUIP_SLOT_ROW 6  // the first slot, then one every row
#define FF6_EQUIP_STAT_ROW 14  // the first statistic, then one every row
#define FF6_EQUIP_STAT_LABEL_X 19
#define FF6_EQUIP_STAT_VALUE_X 26
#define FF6_EQUIP_LIST_ROWS 9  // items that the list shows, from FF6_EQUIP_STAT_ROW
#define FF6_EQUIP_LIST_X 3
#define FF6_EQUIP_LIST_WIDTH 15  // inside the frame, which takes a tile on each side: the list ends before the statistics
#define FF6_MENU_SKILL_CURSOR_X 0
#define FF6_SKILL_LIST_ROWS 18  // names that the list of an ability category shows
#define FF6_SKILL_LIST_WINDOW_X 9
#define FF6_SKILL_LIST_WINDOW_WIDTH 18
#define FF6_SKILL_LIST_X 11
#define FF6_ITEM_NAME_BYTES 13
#define FF6_ITEM_DESC_BYTES 0x13A0
#define FF6_ITEM_DESC_NEWLINE 0x01  // in a description
#define FF6_ITEM_DESC_X 2  // in tiles; the two lines of the window are 12 pixels apart
#define FF6_ITEM_DESC_TOP 48
#define FF6_ITEM_TARGET_PORTRAIT_X 88  // the characters that can get the item are in a window on the right
#define FF6_ITEM_TARGET_CURSOR_X 72
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

// ---- Battles (battle.ff6data) --------------------------------------------------------------------
#define FF6_BATTLE_FILE "battle.ff6data"
#define FF6_BATTLE_MAGIC "FF6BTL01"
#define FF6_MONSTER_COUNT 384
#define FF6_MONSTER_PROP_BYTES 32
#define FF6_MONSTER_ITEM_BYTES 4  // rare steal, common steal, rare drop, common drop
#define FF6_MONSTER_NAME_BYTES 10
#define FF6_BATTLE_COUNT 576
#define FF6_BATTLE_PROP_BYTES 4
#define FF6_BATTLE_MONSTER_BYTES 15  // flags (a word), six monster ids, six positions, the ninth bits of the ids
#define FF6_BATTLE_SLOTS 6
#define FF6_BATTLE_GROUP_COUNT 256
#define FF6_RAND_GROUP_BATTLES 4  // a word each
#define FF6_EVENT_GROUP_BATTLES 2
#define FF6_BATTLE_MAP_COUNT 512
#define FF6_SUB_RATE_BYTES 128
#define FF6_RNG_BYTES 256
#define FF6_AI_SCRIPT_BYTES 0x3950
#define FF6_AI_NO_LOOP 0xFF  // no wait to go back to
#define FF6_AI_VARIABLES 0x24  // the variables that the battle shares; the next numbers are the monster's own
#define FF6_AI_ATTACK_BATTLE 0xEE  // the normal blow of a monster
#define FF6_AI_ATTACK_SPECIAL 0xEF  // its special attack
#define FF6_AI_ATTACK_NOTHING 0xFE
#define FF6_AI_FIRST_COMMAND 0xF0  // below it a byte is an attack
#define FF6_AI_COMMAND_SIZES {4, 2, 4, 3, 4, 4, 4, 2, 3, 4, 4, 3, 4, 1, 1, 1}
#define FF6_AI_LOOP_GUARD 512  // commands that a script may run in a turn
#define FF6_AI_TARGET_MONSTER_1 0x30
#define FF6_AI_TARGET_SELF 0x36
#define FF6_AI_VALUE_HIGH 0x80  // the byte of a variable command: add or subtract
#define FF6_AI_VALUE_SUBTRACT 0x40
#define FF6_AI_VALUE_MASK 0x3F
#define FF6_AI_TIMER_PHASE 11  // the pace counter turns the timers each time it comes to this phase
#define FF6_SPECIAL_NO_DAMAGE 0x40  // of the special attack of a monster
#define FF6_SPECIAL_NO_DODGE 0x80
#define FF6_SPECIAL_KIND_MASK 0x3F
#define FF6_SPECIAL_MULTIPLIER 0x20
#define FF6_SPECIAL_DRAIN_HP 0x30
#define FF6_SPECIAL_DRAIN_MP 0x31
#define FF6_SPECIAL_REMOVE_REFLECT 0x32
#define FF6_MONSTER_PROP_SPECIAL 31
#define FF6_ATTACK_NAME_BYTES 10
#define FF6_GENJU_ATTACK_COUNT 27  // the attacks 54 to 80 have their names apart
#define FF6_ATTACK_NAME_COUNT 175  // the attacks 81 to 255
#define FF6_SKETCH_BYTES 2  // the attacks of Sketch and of Control of each monster
#define FF6_CONTROL_BYTES 4
#define FF6_FIRST_GENJU_ATTACK FF6_SPELL_COUNT
#define FF6_FIRST_NAMED_ATTACK (FF6_SPELL_COUNT + FF6_GENJU_ATTACK_COUNT)
#define FF6_SUB_RATES {0x70, 0x40, 0x160, 0x200}  // added to the counter of the steps: normal, low, high, very high
#define FF6_ENCOUNTER_COUNTER_MAX 0xFF00
#define FF6_RNG_STEP_ENCOUNTER 17  // added to the second number each time the pointer wraps
#define FF6_RNG_STEP_GROUP 0x17
#define FF6_ENCOUNTER_PICKS {0x50, 0xA0, 0xF0}  // a random byte under each picks that battle of the group; else the fourth
#define FF6_MAP_PROP_ENCOUNTERS 5  // bit 7: the map has random battles
#define FF6_MAP_ENCOUNTERS_FLAG 0x80
#define FF6_FIRST_SUB_MAP 3  // the maps 0 to 2 are the world
#define FF6_EVENT_BIT_NO_ENCOUNTERS 461  // set by the events that stop random battles
#define FF6_EVENT_SCRIPT_RAND_BATTLE 0x28
#define FF6_EVENT_SCRIPT_TREASURE_MONSTER 0x40
#define FF6_EVENT_RANDOM_BATTLE 0x4E
#define FF6_EVENT_BOX_BATTLE 0x8E  // the battle of a monster in a box
#define FF6_MONSTER_NONE 0x1FF  // the id of an empty place of a battle
#define FF6_MONSTER_PROP_SPEED 0  // offsets in the properties of a monster
#define FF6_MONSTER_PROP_ATTACK 1
#define FF6_MONSTER_PROP_HIT_RATE 2
#define FF6_MONSTER_PROP_EVADE 3
#define FF6_MONSTER_PROP_MAGIC_BLOCK 4
#define FF6_MONSTER_PROP_DEFENSE 5
#define FF6_MONSTER_PROP_MAGIC_DEFENSE 6
#define FF6_MONSTER_PROP_MAGIC_POWER 7
#define FF6_MONSTER_PROP_HP 8  // a word, then mp, experience and gil
#define FF6_MONSTER_PROP_MP 10
#define FF6_MONSTER_PROP_EXP 12
#define FF6_MONSTER_PROP_GIL 14
#define FF6_MONSTER_PROP_LEVEL 16
#define FF6_MONSTER_ITEM_RARE_DROP 2  // in the items of a monster
#define FF6_MONSTER_ITEM_COMMON_DROP 3

// ---- The battle (ATB, damage and rewards) -------------------------------------------------------
#define FF6_BATTLE_PARTY_SLOTS 4  // combatants 0 to 3 are the party, 4 to 9 the monsters
#define FF6_BATTLE_COMBATANTS 10
#define FF6_BATTLE_TICK_FRAMES 2  // the gauges advance once for each two frames
#define FF6_BATTLE_SPEED_DEFAULT 2  // the raw value of the config, 0 (fast) to 5: the menu shows it plus one
#define FF6_BATTLE_SPEED_STEP 24  // what each step of the battle speed takes from the pace of the monsters
#define FF6_ATB_SPEED_BONUS 20  // added to the speed
#define FF6_ATB_NORMAL 0x40  // the constants of slow, normal and hasted
#define FF6_ATB_SLOW 0x20
#define FF6_ATB_HASTE 0x54
#define FF6_ATB_SHIFT 4
#define FF6_ATB_FULL 0x10000
#define FF6_DAMAGE_VARIANCE_MIN 0xE0  // the random number of the damage gets these bits: 224 to 255
#define FF6_MONSTER_STRENGTH_BASE 0x38  // the vigor of a monster that attacks: 56 to 63
#define FF6_MONSTER_STRENGTH_RANGE 8
#define FF6_MONSTER_POWER_FACTOR 4
#define FF6_MAX_DAMAGE 9999
#define FF6_HIT_ALWAYS 0xFF  // a hit rate that never misses
#define FF6_DROP_RARE_LIMIT 0x20  // a random byte under it drops the rare item: 1 in 8
#define FF6_TWO_HAND_NUM 7  // a weapon with the two-hand effect hits for 7/4 of its power
#define FF6_TWO_HAND_DEN 4
#define FF6_CRITICAL_LIMIT 8  // a random byte under it is a critical hit, which doubles the damage: 1 in 32
#define FF6_BATTLE_TYPE_NORMAL 0
#define FF6_BATTLE_TYPE_BACK 1
#define FF6_BATTLE_TYPE_PINCER 2
#define FF6_BATTLE_TYPE_SIDE 3
#define FF6_BATTLE_TYPE_COUNT 4
#define FF6_BATTLE_TYPE_FLAGS_SHIFT 4  // bit 4 + type of the flags of a battle forbids the type
#define FF6_BATTLE_TYPE_RATES {0xCF, 0x07, 0x07, 0x1E}  // normal, back, pincer, side
#define FF6_BATTLE_SIDE_MIN_ALLIES 3  // fewer allies alive forbid the side attack
#define FF6_BATTLE_AUX_NO_PREEMPTIVE 0x0400  // of the second word of the battle properties
#define FF6_PREEMPTIVE_BASE 0x20  // a random byte under it, plus 4 for each battle type, is a preemptive strike
#define FF6_PREEMPTIVE_STEP 4
#define FF6_GAUGE_START 0xFF  // the high byte of a gauge that no rule has set
#define FF6_GAUGE_RANK_STEP 8
#define FF6_GAUGE_EMPTY_SLOT 0x10
#define FF6_GAUGE_MONSTER_SURPRISE 1  // the monsters start nearly empty when the party strikes first
#define FF6_COUNTER_PERIOD 16  // the counters of status and running go round the combatants in this many ticks

// ---- Attacks, items and status in the battle ----------------------------------------------------
// The status of a combatant: status 1 and 2, then 3 and 4 above them, a bit for each (STATUS_ID of the game).
#define FF6_ST_BLIND 0x00000001u
#define FF6_ST_ZOMBIE 0x00000002u
#define FF6_ST_POISON 0x00000004u
#define FF6_ST_MAGITEK 0x00000008u
#define FF6_ST_VANISH 0x00000010u
#define FF6_ST_IMP 0x00000020u
#define FF6_ST_PETRIFY 0x00000040u
#define FF6_ST_DEAD 0x00000080u
#define FF6_ST_CONDEMNED 0x00000100u
#define FF6_ST_NEAR_FATAL 0x00000200u
#define FF6_ST_IMAGE 0x00000400u
#define FF6_ST_SILENCE 0x00000800u
#define FF6_ST_BERSERK 0x00001000u
#define FF6_ST_CONFUSE 0x00002000u
#define FF6_ST_SAP 0x00004000u
#define FF6_ST_SLEEP 0x00008000u
#define FF6_ST_DANCE 0x00010000u
#define FF6_ST_REGEN 0x00020000u
#define FF6_ST_SLOW 0x00040000u
#define FF6_ST_HASTE 0x00080000u
#define FF6_ST_STOP 0x00100000u
#define FF6_ST_SHELL 0x00200000u
#define FF6_ST_SAFE 0x00400000u
#define FF6_ST_REFLECT 0x00800000u
#define FF6_ST_RAGE 0x01000000u
#define FF6_ST_FROZEN 0x02000000u
#define FF6_ST_RERAISE 0x04000000u
#define FF6_ST_MORPH 0x08000000u
#define FF6_ST_CONTROL 0x10000000u
#define FF6_ST_HIDE 0x20000000u
#define FF6_ST_INTERCEPTOR 0x40000000u
#define FF6_ST_FLOAT 0x80000000u
#define FF6_ST_COUNT 32
#define FF6_ST_GONE (FF6_ST_DEAD | FF6_ST_PETRIFY)  // out of the fight
#define FF6_ST_NO_GAUGE (FF6_ST_DEAD | FF6_ST_PETRIFY | FF6_ST_SLEEP | FF6_ST_STOP | FF6_ST_FROZEN)  // the gauge stands still
#define FF6_ST_NO_COUNTER_HIT (FF6_ST_PETRIFY | FF6_ST_SLEEP | FF6_ST_STOP | FF6_ST_FROZEN)  // an attack never misses them
#define FF6_ST_ZOMBIE_CURES (FF6_ST_BLIND | FF6_ST_POISON | FF6_ST_NEAR_FATAL | FF6_ST_SLEEP | FF6_ST_CONFUSE | FF6_ST_BERSERK)
#define FF6_ST_PETRIFY_CURES (FF6_ST_BLIND | FF6_ST_POISON | FF6_ST_VANISH | FF6_ST_NEAR_FATAL | FF6_ST_IMAGE | FF6_ST_SILENCE | FF6_ST_BERSERK | FF6_ST_CONFUSE | FF6_ST_SAP | FF6_ST_SLEEP)
#define FF6_ST_DEAD_CURES (FF6_ST_PETRIFY | FF6_ST_CONDEMNED)
#define FF6_ST_NEAR_FATAL_DIV 8  // below a eighth of the maximum hp
#define FF6_SLEEP_COUNT 18  // how many times its pace overflows before a status wears off
#define FF6_STOP_COUNT 18
#define FF6_REFLECT_COUNT 26
#define FF6_FREEZE_COUNT 34
#define FF6_CONDEMNED_BASE 60  // the counter of condemned: 60 less the level and a random number under it, plus 20
#define FF6_CONDEMNED_MIN 20
#define FF6_DOT_PERIOD 8  // poison on the third pace of eight, regen and seizure on the first and the fifth
#define FF6_DOT_POISON_PHASE 2
#define FF6_DOT_REGEN_PHASES 0x11  // bits of the phases 0 and 4
#define FF6_DOT_DIVISOR 1024  // the power of poison and regen: max hp * stamina / 1024 + 2
#define FF6_DOT_ADD 2
#define FF6_DOT_CAP 252
#define FF6_STAMINA_MONSTER_BASE 16  // the stamina of a monster: its hp / 512 + 16, 40 for a monster of 12800 hp or more
#define FF6_STAMINA_MONSTER_STEPS 25
#define FF6_STAMINA_MONSTER_MAX 40
#define FF6_ST_POSITIVE 0xFFFF0000u  // the status 3 and 4
#define FF6_NEAR_FATAL_SHIFT 3
#define FF6_ITEM_SPECIAL_BASE 72  // the special effects of the items come after the ones of the attacks
#define FF6_FRACTION_SHIFT 4
#define FF6_MONSTER_FLAG_DIE_AT_ZERO_MP 0x01
#define FF6_MONSTER_STATUS3_FLYING 0x01  // the dance bit: the monster floats
#define FF6_MONSTER_STATUS3_MASK 0xFE  // what a monster keeps of its status 3

// A row of the table of attacks (MagicProp): the target, the element, 24 bits of flags, the cost, the power, the
// flags of the message, the hit rate, the special effect, then the status 1 and 2 and the status 3 and 4.
#define FF6_ATTACK_TARGET 0
#define FF6_ATTACK_ELEMENT 1
#define FF6_ATTACK_FLAGS 2
#define FF6_ATTACK_MP 5
#define FF6_ATTACK_POWER 6
#define FF6_ATTACK_MISC 7
#define FF6_ATTACK_HIT 8
#define FF6_ATTACK_SPECIAL 9
#define FF6_ATTACK_STATUS12 10
#define FF6_ATTACK_STATUS34 12
#define FF6_TARGET_MANUAL 0x01  // the cursor can be moved
#define FF6_TARGET_ONE_SIDE 0x02  // without MANUAL: the user alone
#define FF6_TARGET_INIT_MASK 0x0C
#define FF6_TARGET_INIT_ALL 0x04  // everybody
#define FF6_TARGET_INIT_GROUP 0x08  // one side
#define FF6_TARGET_INIT_HALF 0x0C  // all the monsters or all the characters
#define FF6_TARGET_MULTI 0x20  // a button turns the cursor into all the targets of its side
#define FF6_TARGET_ENEMY 0x40  // the cursor starts on the monsters
#define FF6_AF_PHYSICAL 0x000001u
#define FF6_AF_INSTANT_DEATH 0x000002u
#define FF6_AF_RESURRECT 0x000004u
#define FF6_AF_INVERT_UNDEAD 0x000008u
#define FF6_AF_RANDOM_TARGET 0x000010u
#define FF6_AF_IGNORE_DEF 0x000020u
#define FF6_AF_NO_SPLIT 0x000040u  // the damage is not shared by the targets
#define FF6_AF_MONSTERS_ONLY 0x000080u
#define FF6_AF_NO_REFLECT 0x000200u
#define FF6_AF_LORE 0x000400u  // a monster attack that Strago learns when it hits him
#define FF6_AF_RUNIC 0x000800u  // absorbed by the sword of a Runic stance
#define FF6_AF_AFFECT_MP 0x008000u
#define FF6_AF_RESTORATIVE 0x010000u
#define FF6_AF_DRAIN 0x020000u
#define FF6_AF_REMOVE_STATUS 0x040000u
#define FF6_AF_TOGGLE_STATUS 0x080000u
#define FF6_AF_STAMINA_DEF 0x100000u
#define FF6_AF_NO_DODGE 0x200000u
#define FF6_AF_LEVEL_DIV 0x400000u
#define FF6_AF_HP_FRACTION 0x800000u  // the power is in sixteenths of the hp
#define FF6_ATTACK_MISC_STATUS_BLOCK 1  // misses when the target is immune to the status
#define FF6_ELEMENT_FIRE 0x01
#define FF6_DAMAGE_RESIST_NUM 0xAA  // shell and safe: damage * 170 / 256
#define FF6_DAMAGE_SPLIT_TARGETS 2  // from this many targets the damage is halved
#define FF6_REFLECT_HALF 1  // a reflected attack has half the damage
#define FF6_STAMINA_ROLL 0x7F  // the roll against the stamina of the target of a spell that kills
#define FF6_ITEM_PROP_USAGE 0  // the byte of the type of an item also says where it can be used
#define FF6_ITEM_PROP_TARGET 14
#define FF6_ITEM_PROP_ELEMENT 15
#define FF6_ITEM_PROP_ITEM_POWER 20
#define FF6_ITEM_PROP_ITEM_STATUS12 21
#define FF6_ITEM_PROP_ITEM_STATUS34 23
#define FF6_ITEM_PROP_ITEM_SPECIAL 27
#define FF6_ITEM_PROP_IMMUNE 6  // of equipment: a word of the status that it protects from
#define FF6_ITEM_PROP_EQUIP_STATUS_HIGH 8  // the status that it sets at the start: the third byte of the status here, the second below
#define FF6_ITEM_PROP_EQUIP_STATUS_LOW 25
#define FF6_ITEM_PROP_ELEM_ABSORB 22  // the elements that it absorbs, nullifies and is weak to
#define FF6_ITEM_PROP_ELEM_NULL 23
#define FF6_ITEM_PROP_ELEM_WEAK 24
#define FF6_ITEM_USAGE_BATTLE 0x20
#define FF6_ITEM_USAGE_THROW 0x10
#define FF6_ITEM_FLAG_INVERT_UNDEAD 0x02
#define FF6_ITEM_SPECIAL_NONE 0xFF
#define FF6_SPECIAL_ELIXIR 76  // restores all the hp and mp
#define FF6_MONSTER_PROP_FLAGS 18  // the properties of a monster that the spells read
#define FF6_MONSTER_PROP_IMMUNE 20  // the status 1, 2 and 3 that it blocks
#define FF6_MONSTER_PROP_ABSORB 23
#define FF6_MONSTER_PROP_NULL 24
#define FF6_MONSTER_PROP_WEAK 25
#define FF6_MONSTER_PROP_STATUS 27  // the status that it starts with, status 1 to 4
#define FF6_MONSTER_FLAG_HUMAN 0x10
#define FF6_MONSTER_FLAG_UNDEAD 0x80
#define FF6_CHAR_PROP_COMMANDS 2  // the four commands of an actor
#define FF6_CHAR_COMMANDS 4
#define FF6_CMD_FIGHT 0
#define FF6_CMD_ITEM 1
#define FF6_CMD_MAGIC 2
#define FF6_CMD_TOOLS 9
#define FF6_CMD_ROW 20
#define FF6_CMD_DEFEND 21
#define FF6_CMD_NONE 0xFF
#define FF6_CMD_COUNT 32
#define FF6_BATTLE_DEFEND_HALF 2  // a defending character takes half the damage of a physical attack

// ---- The battle screen: text windows, no monster graphics yet; the positions are not the game's ----
#define FF6_BATTLE_MESSAGE_FRAMES 40  // how long a message stays when nobody presses the button
#define FF6_BATTLE_MESSAGE_X 1  // the window on top: its text is a tile inside it
#define FF6_BATTLE_MESSAGE_Y 1
#define FF6_BATTLE_MESSAGE_W 28
#define FF6_BATTLE_MONSTERS_Y 19  // the windows at the bottom (eight tiles with their frame, from the line where the field ends): the monsters on the left, the party on the right
#define FF6_BATTLE_MONSTERS_W 11  // the width inside the frame, as for the other windows
#define FF6_BATTLE_WINDOW_ROWS 6  // the height inside the frame
#define FF6_BATTLE_ROW_TILES 2
#define FF6_BATTLE_PARTY_X 13
#define FF6_BATTLE_PARTY_W 17
#define FF6_BATTLE_ROW_PX 12  // a line of a character, of a command or of a spell
#define FF6_BATTLE_ROWS_TOP_PX 164  // the first line of the windows at the bottom
#define FF6_BATTLE_MONSTER_ROW_PX 8  // the lines of the monsters when there are more than four
#define FF6_BATTLE_MONSTER_ROWS 4
#define FF6_BATTLE_NAME_X 2  // of a monster in its window
#define FF6_BATTLE_CHAR_NAME_X 14
#define FF6_BATTLE_HP_X 20
#define FF6_BATTLE_GAUGE_X 24
#define FF6_BATTLE_GAUGE_CELLS 5
#define FF6_BATTLE_GAUGE_STEPS 4  // each cell shows four steps
#define FF6_BATTLE_COMMAND_X 1
#define FF6_BATTLE_COMMAND_Y 19  // the windows of the commands and of the lists take the place of the ones at the bottom
#define FF6_BATTLE_COMMAND_COUNT 2  // Fight and Run
#define FF6_BATTLE_LIST_X 0  // the window of the commands, of the spells and of the items
#define FF6_BATTLE_LIST_Y 19
#define FF6_BATTLE_LIST_W 14
#define FF6_BATTLE_ITEM_W 20
#define FF6_BATTLE_COMMAND_W 9  // over the window of the monsters, which shows at both sides of it
#define FF6_BATTLE_LIST_ROWS 4  // how many entries it shows at once
#define FF6_BATTLE_LIST_TEXT_X 2  // the names, and the hand left of them
#define FF6_BATTLE_LIST_AMOUNT_X 19  // the cost of a spell, the quantity of an item
#define FF6_BATTLE_PARTY_HAND_X 98
#define FF6_BATTLE_COMMAND_RUN 0x100  // the entry that runs away, after the commands of the game
#define FF6_BATTLE_RESULT_X 4
#define FF6_BATTLE_GAME_OVER_X 11  // the text of the screen of a lost battle, in tiles
#define FF6_BATTLE_GAME_OVER_Y 12
#define FF6_BATTLE_RESULT_Y 6
#define FF6_BATTLE_RESULT_W 24
#define FF6_BATTLE_RESULT_H 12
#define FF6_BATTLE_HAND_X 0
#define FF6_BATTLE_COMMAND_HAND_X 8
#define FF6_BATTLE_NAME_PAD_CODE 0xFF  // ends the name of a monster

// ---- Battle graphics (battlegfx.ff6data) ---------------------------------------------------------
#define FF6_BATTLE_GFX_FILE "battlegfx.ff6data"
#define FF6_BATTLE_GFX_MAGIC "FF6BGX01"
#define FF6_MONSTER_GFX_COUNT 416  // the monsters, then the espers
#define FF6_MONSTER_GFX_PROP_BYTES 5
#define FF6_MONSTER_PAL_BYTES 0x3000
#define FF6_MONSTER_PAL_STEP 16  // the palettes start every 16 bytes
#define FF6_STENCIL_SMALL_COUNT 128
#define FF6_STENCIL_SMALL_BYTES 8
#define FF6_STENCIL_LARGE_COUNT 48
#define FF6_STENCIL_LARGE_BYTES 32
#define FF6_MONSTER_GFX_BYTES 0x37200
#define FF6_STENCIL_SMALL_CELLS 8  // a small stencil is 8 x 8 tiles, a large one 16 x 16
#define FF6_STENCIL_LARGE_CELLS 16
#define FF6_GFX_3BPP_FLAG 0x8000  // of the first word of the properties
#define FF6_GFX_LARGE_FLAG 0x8000  // of the second one
#define FF6_GFX_ADDRESS_MASK 0x7FFF
#define FF6_GFX_ADDRESS_UNIT 8
#define FF6_GFX_PALETTE_MASK 0x3FF
#define FF6_TILE_4BPP_BYTES 32
#define FF6_TILE_3BPP_BYTES 24
#define FF6_TILE_PLANE_STRIDE 16  // the third and fourth planes of a tile start here
#define FF6_BATTLE_MONSTER_X_UNIT 8  // the position of a monster in a battle: the nibbles times this, in pixels
#define FF6_BATTLE_MONSTER_Y_UNIT 8
#define FF6_BG_COUNT 56  // battle backgrounds, the last three blank
#define FF6_BG_PROP_BYTES 6  // three blocks of graphics, two tilemaps, a palette
#define FF6_BG_PROP_PALETTE 5
#define FF6_BG_WAVY_FLAG 0x80  // of the palette byte: the lines of the field sway
#define FF6_BG_WAVE_AMPLITUDE 3  // in pixels, and the lines and frames of a period
#define FF6_BG_WAVE_AMPLITUDE_DREAM 4
#define FF6_BG_CYANS_DREAM 46
#define FF6_BG_TENTACLES 55
#define FF6_BG_WAVE_LINES 48
#define FF6_BG_WAVE_FRAMES 90
#define FF6_BG_PAL_BYTES 96  // three palettes of 16 colors
#define FF6_BG_GFX_COUNT 75
#define FF6_BG_GFX_BYTES 0x2000
#define FF6_BG_TILES_COUNT 49
#define FF6_BG_TILES_BYTES 0x800  // 32 x 32 words
#define FF6_BG_COLUMNS 32
#define FF6_BG_ROWS 19  // what the battlefield shows of the tilemap
#define FF6_BG_NONE 0xFF  // a block of graphics that is not loaded
#define FF6_BG_LARGE_FLAG 0x80  // of a block: it holds twice the tiles
#define FF6_BG_INDEX_MASK 0x7F
#define FF6_BG_BLOCK_BYTES 0x1000
#define FF6_BG_SECOND_TILE 128  // the tile numbers where the blocks start
#define FF6_BG_THIRD_TILE 896
#define FF6_BG_THIRD_TRIM 0x20  // the third block loses its last 32 bytes
#define FF6_BG_TILE_COUNT 1024
#define FF6_BG_FIRST_PALETTE 5  // the palettes of the tilemap that the background fills
#define FF6_BG_PALETTE_COUNT 3
#define FF6_BG_PALETTE_COLORS 16
#define FF6_BG_TILE_MASK 0x3FF  // of a word of the tilemap, then the palette, and the flips
#define FF6_BG_PALETTE_SHIFT 10
#define FF6_BG_HFLIP 0x4000
#define FF6_BG_VFLIP 0x8000
#define FF6_MAP_BG_MASK 0x7F  // of the property of a map: its battle background
#define FF6_BATTLE_BG_DEFAULT 0x3F  // the flags of a battle that use the background of the map

// ---- Attack animations (attackanim.ff6data) ------------------------------------------------------
#define FF6_ATTACK_ANIM_FILE "attackanim.ff6data"
#define FF6_ATTACK_ANIM_MAGIC "FF6ATK01"
#define FF6_ATTACK_PROP_COUNT 406  // one per attack
#define FF6_ATTACK_PROP_BYTES 14
#define FF6_ATTACK_SCRIPT_COUNT 650  // the scripts, each with the properties of its sheet of frames
#define FF6_ATTACK_GFX_PROP_BYTES 6
#define FF6_ATTACK_SCRIPT_BYTES 0x7FB2
#define FF6_ATTACK_FRAME_PTR_COUNT 2949
#define FF6_ATTACK_FRAME_FIRST 0x141  // the frame pointers are addresses, the data starts here
#define FF6_ATTACK_FRAME_END 59785
#define FF6_ATTACK_TILES_3BPP_BYTES 0x6000
#define FF6_ATTACK_PAL_BYTES 0x6000
#define FF6_ATTACK_TILES_2BPP_BYTES 0x4000
#define FF6_ATTACK_GFX_3BPP_BYTES 0x1CA00
#define FF6_ATTACK_GFX_2BPP_BYTES 0x6000
#define FF6_ATTACK_WEAPON_PROP_COUNT 128  // the weapons, then the attacks of the monsters
#define FF6_ATTACK_WEAPON_PROP_BYTES 8
#define FF6_ATTACK_MODE7_GFX_BYTES 0x1800  // three sets of tiles of 4 bits
#define FF6_ATTACK_MODE7_MAP_BYTES 0xC00  // three maps of tile numbers
#define FF6_MODE7_SIDE 256  // the pixels of the plane that hold a picture: 32 x 32 tiles of the 128 x 128 of the plane
#define FF6_MODE7_TILE_COLUMNS 32
#define FF6_MODE7_SET_GFX_BYTES 0x800
#define FF6_MODE7_SET_MAP_BYTES 0x400
#define FF6_MODE7_SET_TILES 63  // the tile after the last of a set is empty, as the map says by its number
#define FF6_MODE7_PLANE 1024  // the pixels of a side of the whole plane, which the transformation wraps round
#define FF6_ESPER_ODIN 11  // the order of an esper among the pictures of the espers
#define FF6_ESPER_RAIDEN 12
#define FF6_ESPER_CRUSADER 15
#define FF6_ESPER_CRUSADER_SHEET 28  // the picture of the figure in gold of the purifier
#define FF6_ESPER_CRUSADER_DARK 29  // the same graphics with the palette that follows, for the dark figure
#define FF6_ESPER_SHEET_PALETTE 1000  // stands for the palette of a frame that is cut from the picture of an esper, plus its order
#define FF6_ATTACK_PROBE_FRAMES 900  // how far a copy of an animation is run to find what it shows first
#define FF6_ATTACK_WEAPON_FISTS 0x5D  // the entry of a character that fights with bare hands; the monsters start here too
#define FF6_ATTACK_WEAPON_HIT_SPRITE 0x80  // of the init function: the hit is a sprite, not a bg1 thread
#define FF6_ATTACK_MONSTER_WEAPON_SCRIPT 0x62  // the script of the weapon of every monster
#define FF6_ATTACK_MONSTER_HIT_FIRST 0x60  // the hit scripts from here are those of the monsters, +0x200
#define FF6_ATTACK_NO_SCRIPT 0xFFFF
#define FF6_ATTACK_GFX_ONLY_FLAG 0x8000  // of a script of the properties: only the graphics are loaded
#define FF6_ATTACK_INIT_PURIFIER 0x74  // the init function that makes three threads of the esper
#define FF6_ATTACK_INIT_JUSTICE 0x3B  // the same, and it puts bg3 behind the field
#define FF6_ATTACK_INIT_POSSESS 0x77  // this one puts bg3 behind the field too
#define FF6_PURIFIER_REACH 8  // how far the side figures of the purifier go out to touch the edges
#define FF6_JUSTICE_BEHIND_Y 0x81  // the combatants whose bottom is above this line stand behind bg1 for Justice
#define FF6_JUSTICE_WINDOW_LINES 128  // the lines where the window of Justice shows bg1 (and the sprites of the animation)
#define FF6_BATTLE_CHAR_ROW_ANGLE 12  // what the back row adds to the angle of the line of the characters
#define FF6_BATTLE_VEIL_MIN_WIDTH 240  // the frames of bg3 this wide cover the screen, with the edges of the picture stretched
#define FF6_BATTLE_VEIL_FIRST_FRAME 3  // the frames of the veil of Justice from this one are the wide ones
#define FF6_BATTLE_VEIL_STRETCH 17  // sixteenths of the width of the flames in the game; measured on a video of it
#define FF6_BATTLE_BG1_PIVOT 121  // the column that the wider picture of Alexander grows from, fitted on the video
#define FF6_BATTLE_POCKET_LUMA 48  // the picture pixels darker than this let the sprites under bg1 show through
#define FF6_ATTACK_PAL_BYTES_EACH 16  // a palette of the animations holds 8 colors
#define FF6_ATTACK_SHEET_TILES 128  // the tiles of a sheet of sprites: 16 across, 8 down
#define FF6_ATTACK_SHEET_COLUMNS 16
#define FF6_ATTACK_TILE_UNITS 64  // bytes of the tile table for one step of the tile offset
#define FF6_ATTACK_TILE_SET_BYTES 0x4000
#define FF6_ATTACK_2BPP_FLAG 0x80  // of the first byte of the properties of a sheet
#define FF6_ATTACK_SET_FLAG 0x40
#define FF6_ATTACK_FRAMES_MASK 0x3F
#define FF6_ATTACK_TILE_MASK 0x3FFF  // of an entry of the tile table, then the flips
#define FF6_ATTACK_TILE_HFLIP 0x4000
#define FF6_ATTACK_TILE_VFLIP 0x8000
#define FF6_ATTACK_CELL 16  // pixels of a sprite of a frame
#define FF6_ATTACK_FRAME_END_MARK 0xFF
#define FF6_ATTACK_SPRITE_HFLIP 0x40
#define FF6_ATTACK_SPRITE_VFLIP 0x80
#define FF6_CHAR_GFX_COUNT 23
#define FF6_CHAR_GFX_BYTES 0x1500
#define FF6_CHAR_ACTION_COUNT 32
#define FF6_CHAR_ACTION_TILES 8  // two unused, then six in reading order
#define FF6_CHAR_ACTION_FIRST_TILE 2
#define FF6_CHAR_PAL_TABLE_BYTES 24
#define FF6_CHAR_PAL_COUNT 8
#define FF6_CHAR_PAL_BYTES 32
#define FF6_CHAR_WIDTH 16  // a character of a battle is 2 x 3 tiles
#define FF6_CHAR_HEIGHT 24
#define FF6_CHAR_TILES 6
#define FF6_CHAR_TILE_NONE 0xFFFF
#define FF6_CHAR_POSE_FIGHT 17  // the poses of the table: the default one, ready, near fatal and dead
#define FF6_CHAR_POSE_READY 21
#define FF6_CHAR_POSE_NEAR_FATAL 20
#define FF6_CHAR_POSE_DEAD 10
#define FF6_CHAR_POSE_ATTACK 6  // the poses of the table that the actions use; the table has no names, they are read off the sheet
#define FF6_CHAR_POSE_CAST 7
#define FF6_CHAR_POSE_HURT 27
#define FF6_CHAR_POSE_VICTORY 23
#define FF6_CHAR_POSE_DEFEND 8
#define FF6_CHAR_POSE_RUN 28
#define FF6_BATTLE_ESCAPE_FRAMES 40  // a character that runs away leaves the screen in this long, so many pixels a frame
#define FF6_BATTLE_ESCAPE_SPEED 3
#define FF6_BATTLE_POSE_FRAMES 40  // how long an action keeps its pose
#define FF6_BATTLE_STEP_PIXELS 12  // the step of a character at a monster, to the left
#define FF6_BATTLE_FLASH_FRAMES 10
#define FF6_BATTLE_FLASH_PERCENT 7  // what each frame of the flash is worth, in percent of its color
#define FF6_BATTLE_BLINK_FRAMES 24  // a monster that is hit blinks, and one that falls fades the same way
#define FF6_BATTLE_BLINK_PERIOD 3
#define FF6_BATTLE_MODE7_LINES 151  // the scanlines of the field that mode 7 takes; below, the HDMA of the game goes back to mode 1 for the windows
#define FF6_BATTLE_FIELD_HEIGHT 160  // the scanlines of the battlefield, above the windows
#define FF6_BATTLE_SLASH_SIZE 14  // the half of the diagonal of a slash, and how long it lasts
#define FF6_BATTLE_SLASH_FRAMES 12
#define FF6_BATTLE_RING_RADIUS 22  // a ring that spreads over the target
#define FF6_BATTLE_RING_POINTS 40
#define FF6_BATTLE_RING_FRAMES 20
#define FF6_BATTLE_SPARKLES 8  // sparkles that rise from a healed target
#define FF6_BATTLE_SPARKLE_RISE 30
#define FF6_BATTLE_SPARKLE_FRAMES 28
#define FF6_BATTLE_ELEMENT_COLORS {{255, 80, 32}, {96, 200, 255}, {255, 255, 64}, {96, 255, 96}, {176, 255, 176}, {255, 255, 255}, {200, 144, 64}, {64, 96, 255}}
#define FF6_NEAR_FATAL_DIVISOR 8  // a character with less than this fraction of its hp kneels
#define FF6_BATTLE_CHAR_Y {68, 86, 104, 122}  // of the characters of a battle, from the top
#define FF6_BATTLE_CHAR_ANGLE 0x70  // how far right of the middle the line of the characters leans
#define FF6_BATTLE_CHAR_APEX 0x68  // the point where the line starts is this far above the screen
#define FF6_BATTLE_CHAR_CENTER_X 0x78

// ---- Characters, items and levels (game.ff6data) ------------------------------------------
#define FF6_RULES_FILE "game.ff6data"
#define FF6_RULES_MAGIC "FF6GAME1"
#define FF6_CHAR_PROP_COUNT 24  // actors
#define FF6_CHAR_PROP_BYTES 22
#define FF6_CHAR_PROP_HP 0  // offsets in a character's properties
#define FF6_CHAR_PROP_MP 1
#define FF6_CHAR_PROP_VIGOR 6  // then speed, stamina and magic power
#define FF6_CHAR_PROP_ATTACK 10
#define FF6_CHAR_PROP_DEFENSE 11
#define FF6_CHAR_PROP_MAGIC_DEFENSE 12
#define FF6_CHAR_PROP_EVADE 13
#define FF6_CHAR_PROP_MAGIC_BLOCK 14
#define FF6_CHAR_PROP_EQUIPMENT 15  // weapon, shield, helmet, armor and two relics
#define FF6_CHAR_PROP_FLAGS 21
#define FF6_CHAR_LEVEL_MOD_MASK 0x0C
#define FF6_CHAR_LEVEL_MOD_SHIFT 2
#define FF6_CHAR_LEVEL_MODS {0, 2, 5, -3}
#define FF6_ITEM_PROP_BYTES 30
#define FF6_ITEM_PROP_TYPE 0  // offsets in the properties of an item
#define FF6_ITEM_PROP_TYPE_MASK 7
#define FF6_ITEM_PROP_EQUIP_FLAGS 2
#define FF6_ITEM_PROP_RELIC1 9  // hp and mp boosts
#define FF6_ITEM_PROP_RELIC3 11
#define FF6_ITEM_PROP_RELIC4 12
#define FF6_ITEM_PROP_STATS 16  // vigor and speed, then stamina and magic power: a nibble each
#define FF6_ITEM_PROP_WEAPON_FLAGS 19
#define FF6_ITEM_PROP_POWER 20  // battle power of a weapon, defense of an armor
#define FF6_ITEM_PROP_HIT_OR_MAGIC_DEFENSE 21
#define FF6_ITEM_PROP_EVADE 26  // evade in the low nibble, magic block in the high one
#define FF6_ITEM_TYPE_TOOL 0  // used by the command Tools, and never used up
#define FF6_ITEM_TYPE_WEAPON 1
#define FF6_ITEM_TYPE_ARMOR 2
#define FF6_ITEM_TYPE_SHIELD 3
#define FF6_ITEM_TYPE_HELMET 4
#define FF6_ITEM_TYPE_RELIC 5
#define FF6_ITEM_TYPE_CONSUMABLE 6
#define FF6_ITEM_PROP_EQUIP_CHARS 1  // a word: a bit for each character that can equip the item
#define FF6_EQUIP_RIGHT_HAND 0
#define FF6_EQUIP_LEFT_HAND 1
#define FF6_EQUIP_HEAD 2
#define FF6_EQUIP_BODY 3
#define FF6_EQUIP_FIRST_RELIC 4
#define FF6_EQUIP_ANYONE_BIT 0x8000  // the equip mask of a character that wears the relic that lets it equip anything
#define FF6_RELIC4_EQUIP_ANY 0x20

// ---- Using items from the menu (CheckCanUseItem, lpitem_exec) -------------------------------
#define FF6_ITEM_USAGE_MENU 0x40  // in the first byte of the properties of an item
#define FF6_ITEM_PROP_FLAGS 19  // of a consumable: what it does
#define FF6_ITEM_PROP_STATUS 21  // and the status that it removes, a word
#define FF6_ITEM_FLAG_HP 0x08
#define FF6_ITEM_FLAG_MP 0x10
#define FF6_ITEM_FLAG_REMOVE_STATUS 0x20
#define FF6_ITEM_FLAG_DAMAGE 0x40
#define FF6_ITEM_FLAG_FRACTION 0x80  // the power is in sixteenths of the maximum
#define FF6_STATUS_DEAD 0x80
#define FF6_ITEM_FRACTION_SHIFT 4
#define FF6_ITEM_MEGALIXIR 239
#define FF6_ITEM_FENIX_DOWN 240
#define FF6_ITEM_SLEEPING_BAG 246  // these need a save point, a place to warp from, and the status effects of the game
#define FF6_ITEM_TENT 247
#define FF6_ITEM_WARP_STONE 253
#define FF6_ITEM_IMP_BIT 0x40  // of the equip flags: an item that only an imp can use well
#define FF6_ITEM_TWO_HANDED 0x40  // of the flags of a weapon
#define FF6_RELIC1_HP_25 0x04
#define FF6_RELIC1_HP_50 0x08
#define FF6_RELIC1_HP_12 0x10
#define FF6_RELIC1_MP_25 0x20
#define FF6_RELIC1_MP_50 0x40
#define FF6_RELIC1_MP_12 0x80
#define FF6_RELIC3_STRENGTH_50 0x80  // the hyper wrist
#define FF6_RELIC4_GAUNTLET 0x08
#define FF6_WEAPON_BACK_ROW 0x20  // of the weapon flags: it reaches from the back row
#define FF6_RELIC1_ATLAS 0x01  // +25% of the physical damage
#define FF6_RELIC1_EARRING 0x02  // +50% of the magic damage
#define FF6_RELIC3_SINGLE_EARRING 0x02  // +25%
#define FF6_RELIC3_MAX_HIT 0x10
#define FF6_RELIC3_HALF_MP 0x20
#define FF6_RELIC3_ONE_MP 0x40
#define FF6_RELIC3_STEAL 0x01  // the sneak ring doubles the chance to steal
#define FF6_RELIC_DAMAGE_SHIFT 2  // a quarter more
#define FF6_STEAL_BASE 50  // the chance to steal: the level of the thief plus this, less the level of the monster
#define FF6_STEAL_RARE_LIMIT 0x20  // a random byte under it takes the rare item: 1 in 8
#define FF6_CMD_STEAL 5
#define FF6_CMD_BUSHIDO 7
#define FF6_CMD_BLITZ 10
#define FF6_ATTACK_FIRST_BUSHIDO 85  // Dispatch to Cleave
#define FF6_ATTACK_FIRST_BLITZ 93  // Pummel to Bum Rush
#define FF6_TECH_ATTACKS 8
#define FF6_RELIC4_GENJI_GLOVE 0x10
#define FF6_EQUIP_EVADE_TABLE {0, 10, 20, 30, 40, 50, -10, -20, -30, -40, -50}  // by the nibble of the item
#define FF6_LEVEL_STEPS 98  // from level 1 to 99
#define FF6_SHOP_COUNT 128
#define FF6_SHOP_PROP_BYTES 9  // the kind and the price modifier, then eight items
#define FF6_SHOP_ITEMS 8
#define FF6_SHOP_KIND_MASK 7  // weapons, armor, items, relics or a vendor
#define FF6_SHOP_PRICE_SHIFT 3
#define FF6_SHOP_PRICE_MASK 7
#define FF6_ITEM_PROP_PRICE 28  // a word
#define FF6_PRICE_PLUS_50 1
#define FF6_PRICE_PLUS_100 2
#define FF6_PRICE_HALF 3
#define FF6_PRICE_HALF_FOR_FEMALE 4
#define FF6_PRICE_HALF_FOR_MALE 5
#define FF6_PRICE_HALF_FOR_EDGAR 6
#define FF6_SHOPPERS_FEMALE {0, 6, 8}  // the map sprites of Terra, Celes and Relm
#define FF6_SHOPPER_EDGAR 4

// ---- Abilities: spells, espers, swordtechs and blitzes ------------------------------------------
#define FF6_SPELL_COUNT 54
#define FF6_SPELL_NAME_BYTES 7  // a symbol, then six letters
#define FF6_ESPER_COUNT 27
#define FF6_ESPER_NAME_BYTES 8
#define FF6_TECH_COUNT 8  // swordtechs, and blitzes
#define FF6_TECH_NAME_BYTES 10
#define FF6_NATURAL_MAGIC_PAIRS 16  // for Terra, then as many for Celes: a spell and the level that teaches it
#define FF6_NATURAL_MAGIC_BYTES (2 * FF6_NATURAL_MAGIC_PAIRS * 2)
#define FF6_NATURAL_MAGIC_TERRA 0  // the actors that learn them, and the swordtech and blitz users
#define FF6_NATURAL_MAGIC_CELES 6
#define FF6_ACTOR_CYAN 2
#define FF6_MAGIC_PROP_COUNT 256  // the attacks, of which the first FF6_SPELL_COUNT are spells
#define FF6_MAGIC_PROP_BYTES 14
#define FF6_MAGIC_PROP_MP 5  // the cost of a spell, in the properties of the attack
#define FF6_ACTOR_SABIN 5
#define FF6_ACTOR_CELES 6
#define FF6_ACTOR_STRAGO 7
#define FF6_LORE_COUNT 24  // the attacks 139 to 162, which Strago learns
#define FF6_ATTACK_FIRST_LORE 139
#define FF6_CMD_MORPH 3
#define FF6_CMD_REVERT 4
#define FF6_CMD_RUNIC 11
#define FF6_CMD_LORE 12
#define FF6_CMD_THROW 8
#define FF6_CMD_MIMIC 18
#define FF6_CMD_JUMP 22
#define FF6_CMD_SKETCH 13
#define FF6_CMD_CONTROL 14
#define FF6_CMD_CAPTURE 6
#define FF6_STOCK_FIRST_ITEM 0  // the items of --stock: eight weapons from the Dirk, then the eight tools
#define FF6_ITEM_BIO_BLASTER 164  // the tools that cast an attack of their own
#define FF6_ITEM_FLASH 165
#define FF6_ITEM_CHAIN_SAW 166
#define FF6_ITEM_NOISEBLASTER 163
#define FF6_ITEM_DRILL 168
#define FF6_ITEM_AUTOCROSSBOW 170
#define FF6_ATTACK_BIO_BLAST_TOOL 125
#define FF6_ATTACK_FLASH_TOOL 126
#define FF6_ATTACK_ANIM_TOOLS 328  // the animation of the tool number n is this plus n; the chain saw that kills is the ninth
#define FF6_TOOL_FIRST 163
#define FF6_TOOL_CHAIN_SAW_KILL 8
#define FF6_CHAIN_SAW_KILL_MASK 3  // it kills when two random bits are all zero
#define FF6_ITEM_PROP_TOOL_HIT 21
#define FF6_STOCK_ITEMS 8
#define FF6_STOCK_FIRST_TOOL 163
#define FF6_STOCK_TOOLS 8
#define FF6_CMD_GP_RAIN 24
#define FF6_CMD_POSSESS 28
#define FF6_MONSTER_PROP_MONSTER_STATUS 19  // what a monster refuses: Sketch and Control among them
#define FF6_MONSTER_CANT_SKETCH 0x20
#define FF6_MONSTER_CANT_CONTROL 0x80
#define FF6_RELIC3_SKETCH_RATE 0x04  // the beret
#define FF6_RELIC3_CONTROL_RATE 0x08  // the coronet
#define FF6_SKETCH_RATE_SCALE 170  // the level of the target, times this over 256, with the beret or the coronet
#define FF6_SKETCH_SECOND_LIMIT 0x40  // the second attack of Sketch comes when a byte is at least this
#define FF6_GP_RAIN_PER_LEVEL 30  // the gil that GP Rain throws for each level; the damage is twice that
#define FF6_GP_RAIN_MULTIPLIER 2
#define FF6_POSSESS_LIMIT 150  // Possess works when a byte is under this
#define FF6_CMD_X_MAGIC 23
#define FF6_CMD_HEALTH 26  // casts Cura
#define FF6_CMD_SHOCK 27
#define FF6_ATTACK_SHOCK 130
#define FF6_ATTACK_CURA 46
#define FF6_CMD_SUMMON 25
#define FF6_CMD_MAGITEK 29
#define FF6_ATTACK_FIRST_MAGITEK 131  // Fire Beam to Tek Missile
#define FF6_ESPER_GFX_FIRST 384  // the picture of the first esper among those of the monsters
#define FF6_ESPER_FRAMES 90  // how long an esper stays at least
#define FF6_ESPER_FADE_PERCENT 2  // how much of the picture comes or goes in a frame
#define FF6_MAGITEK_ATTACKS 8
#define FF6_MORPH_TICKS 600  // how long Terra stays morphed
#define FF6_BUM_RUSH_BIT 0x80
#define FF6_ESPER_FIRST_ID 0x36  // the events give espers 0x36 to 0x50
#define FF6_EVENT_GIVE_ESPER 0x86
#define FF6_EVENT_TAKE_ESPER 0x87
#define FF6_EVENT_GIVE_SWDTECHS 0x8F
#define FF6_EVENT_GIVE_BUM_RUSH 0x90

// The shop menu: the positions are close to the game's
#define FF6_SHOP_OPTION_COLUMNS {3, 8, 14}
#define FF6_SHOP_OPTION_ROW 6
#define FF6_SHOP_BUY_TOP 56  // pixels, then the rows are FF6_ITEM_ROW_HEIGHT apart
#define FF6_SHOP_SELL_TOP 80
#define FF6_SHOP_SELL_ROWS 10
#define FF6_SHOP_DESC_TOP 160
#define FF6_SHOP_NAME_X 3
#define FF6_SHOP_PRICE_X 20
#define FF6_SHOP_CURSOR_X 8
#define FF6_MAX_LEVEL 99
#define FF6_DEFAULT_LEVEL 3  // the average level when no character is available
#define FF6_MAX_HP 9999
#define FF6_MAX_MP 999
#define FF6_MAX_ITEM_QUANTITY 99
#define FF6_MAX_GIL 9999999
#define FF6_MAX_STEPS 9999999
#define FF6_MAX_EXPERIENCE 0xFFFFFF
#define FF6_EXP_SHIFT 3  // experience is the sum of the steps of the levels, times 8
#define FF6_STAT_MAX 255
#define FF6_EVENT_BIT_CHAR_AVAILABLE 0x2F0  // one bit for each character that has joined
#define FF6_EVENT_BIT_NOT_ENOUGH_GIL 0x1BE  // set by the command that takes gil when there is not enough
#define FF6_HP_BOOST_MASK 0xC0
#define FF6_HP_CHANGES {1, 2, 4, 8, 16, 32, 64, 128}  // what the commands that add or take hp use
#define FF6_EVENT_SET_CHAR_PROP 0x40
#define FF6_EVENT_NORMALIZE_LEVEL 0x77
#define FF6_EVENT_GIVE_ITEM 0x80
#define FF6_EVENT_TAKE_ITEM 0x81
#define FF6_EVENT_GIVE_GIL 0x84
#define FF6_EVENT_TAKE_GIL 0x85
#define FF6_EVENT_CHANGE_HP 0x8B
#define FF6_EVENT_CHANGE_MP 0x8C
#define FF6_EVENT_REMOVE_EQUIPMENT 0x8D
#define FF6_EVENT_SHOP 0x9B
#define FF6_EVENT_BATTLE 0x4D  // a group of the event battles, then the flags of the background
#define FF6_EVENT_BATTLE_COLLISION 0x4C
#define FF6_EVENT_BATTLE_SECOND_LIMIT 0xC0  // a random byte from it picks the second battle of the group: 1 in 4
#define FF6_BATTLE_RANDOM_FLAG 0x8000  // of a battle number: one of the next four battles
#define FF6_BATTLE_RANDOM_SPAN 4
#define FF6_BATTLE_SEED_STEP 0x9E3779B9u
#define FF6_HP_CHANGE_SET_MAX 0x7F  // of the second byte of the commands that change hp: set to the maximum
#define FF6_HP_CHANGE_SUBTRACT 0x80

// ---- Treasure chests (CheckTreasure) ------------------------------------------------------
#define FF6_TREASURE_PROP_SIZE 5
#define FF6_TREASURE_BIT_MASK 0x1FF  // of the word of a chest: its bit among the 512 treasure bits
#define FF6_TREASURE_BIT_COUNT 512
#define FF6_TREASURE_KIND_GIL 0x8000
#define FF6_TREASURE_KIND_ITEM 0x4000
#define FF6_TREASURE_KIND_MONSTER 0x2000
#define FF6_TREASURE_GIL_UNIT 100  // a chest of gil holds its contents times this
#define FF6_TILE_TREASURE 0x80  // low byte of a tile property: a chest or a pot that can be opened
#define FF6_TILE_CHEST_CLOSED 0x13
#define FF6_TILE_CHEST_OPEN 0x12
#define FF6_SFX_TREASURE_CHEST 0xA6
#define FF6_SFX_TREASURE_POT 0x1B
#define FF6_EVENT_SCRIPT_TREASURE_ITEM 0x08  // offsets of the events that tell what a chest held
#define FF6_EVENT_SCRIPT_TREASURE_GIL 0x10
#define FF6_EVENT_SCRIPT_TREASURE_EMPTY 0x14
#define FF6_DLG_GP 0x19  // in a dialogue: the amount of gil that was found
#define FF6_DLG_ITEM 0x1A  // and the name of the item that was received
#define FF6_DLG_GLYPH_ZERO 0x54
#define FF6_DLG_GLYPH_SPACE 0x7F
#define FF6_DLG_FROM_MENU_CODE 0x60  // a code of the small font of the menus minus this is a glyph of the dialogues
#define FF6_MENU_CODE_LAST_GLYPH 0xDF


// ---- Title screen and opening (cutscene program of the game, title.ff6data) --------------------------------
#define FF6_TITLE_FILE "title.ff6data"
#define FF6_TITLE_MAGIC "FF6TITL1"
#define FF6_TITLE_PROGRAM_BYTES 11363  // the program of the cutscenes, which runs from $7e5000
#define FF6_TITLE_PROGRAM_BASE 0x5000
#define FF6_TITLE_GFX_BYTES 44712  // what the game decompresses to $7f0000
#define FF6_TITLE_MAP_GFX_COUNT 4  // the sealed gate, then three graphics of Narshe
#define FF6_TITLE_MAP_GFX_BYTES 0x2000
#define FF6_TITLE_TABLE_BYTES 256  // the table of random numbers, then the sine table
#define FF6_EVENT_TITLE_SCREEN 0xA9  // event commands: the title screen, the opening, the menu of saved games, the load
#define FF6_EVENT_OPENING 0xAA
#define FF6_EVENT_LOAD_MENU 0xAB
#define FF6_EVENT_RESTORE_GAME 0xAC
#define FF6_EVENT_SCRIPT_GAME_START 0x5E33  // where the game begins: the title, then the story and the opening
#define FF6_EVENT_BIT_SKIPPED 0x2FF  // set when a button ended a cutscene, or a saved game was chosen
#define FF6_GAME_START_MAP 19  // loaded in the dark before the event of the start of the game takes over

// ---- Vehicles (DrawChoco, DrawMagitek, DrawRaft): the graphics that the objects ride on ------------------------
#define FF6_VEHICLE_GFX_BYTES 0x1C00
#define FF6_VEHICLE_GFX_VRAM 0x7200  // where the game puts them in the video memory, in words
#define FF6_VEHICLE_MASK 0x60  // the type of vehicle in the flags of the vehicle commands
#define FF6_VEHICLE_CHOCOBO 0x20
#define FF6_VEHICLE_MAGITEK 0x40
#define FF6_VEHICLE_RAFT 0x60
#define FF6_VEHICLE_RIDER 0x80  // the rider shows
#define FF6_VEHICLE_SHIFT 1  // the vehicle of an npc is in the two top bits of its byte of movement
#define FF6_VEHICLE_NPC_MASK 0xC0
#define FF6_EVENT_OBJ_VEHICLE 0x44
#define FF6_VEHICLE_SPRITE_SIZE 16  // the vehicles are made of sprites of 16 x 16 pixels
#define FF6_VEHICLE_MAGITEK_VARIANTS 16  // four facings, four steps
