#ifndef CONSTANTS_H
#define CONSTANTS_H

// Pixel-perfect render target and window
#define VIRT_W 640
#define VIRT_H 360
#define SCALE 2
#define SCREEN_W (VIRT_W * SCALE)
#define SCREEN_H (VIRT_H * SCALE)

// Party frames
#define FRAME_W 116
#define FRAME_H 42
#define FRAME_GAP 6
#define FRAME_Y 8

// Hand cards
#define HAND_CARD_W 96
#define HAND_CARD_H 120
#define HAND_GAP 3
#define HAND_BOTTOM_MARGIN -8
#define HAND_Y (VIRT_H - HAND_CARD_H - HAND_BOTTOM_MARGIN)

// Enemy
#define ENEMY_SIZE 56
#define ENEMY_Y 145

// Cast bar
#define CAST_BAR_W 132
#define CAST_BAR_HEIGHT 28

// Reward cards
#define REWARD_CARD_W 96
#define REWARD_CARD_H 120
#define REWARD_GAP 12
#define REWARD_Y 74

// Relic reward screen
#define RELIC_REWARD_CARD_W 172
#define RELIC_REWARD_CARD_H 112
#define RELIC_REWARD_CARD_GAP 14
#define RELIC_REWARD_CARD_Y 112
#define RELIC_REWARD_CARD_HOVER_LIFT 4

#define RELIC_REWARD_TITLE_BAND_X 108
#define RELIC_REWARD_TITLE_BAND_Y 42
#define RELIC_REWARD_TITLE_BAND_W 424
#define RELIC_REWARD_TITLE_BAND_H 46

#define RELIC_REWARD_EMPTY_PANEL_X 126
#define RELIC_REWARD_EMPTY_PANEL_Y 134
#define RELIC_REWARD_EMPTY_PANEL_W 388
#define RELIC_REWARD_EMPTY_PANEL_H 82

#define RELIC_REWARD_TRAY_FRAME_X 144
#define RELIC_REWARD_TRAY_FRAME_Y 266
#define RELIC_REWARD_TRAY_FRAME_W 352
#define RELIC_REWARD_TRAY_FRAME_H 52
#define RELIC_REWARD_TRAY_INSET_X 8
#define RELIC_REWARD_TRAY_INSET_Y 4

#define RELIC_REWARD_ICON_BOX_X 14
#define RELIC_REWARD_ICON_BOX_Y 14
#define RELIC_REWARD_ICON_BOX_SIZE 36
#define RELIC_REWARD_NAME_X 58
#define RELIC_REWARD_NAME_Y 16
#define RELIC_REWARD_DESC_X 14
#define RELIC_REWARD_DESC_Y 58
#define RELIC_REWARD_DESC_INSET_W 28
#define RELIC_REWARD_CLAIM_Y 96

#define RELIC_REWARD_PULSE_SPEED 120
#define RELIC_REWARD_PULSE_MOD 75
#define RELIC_REWARD_PULSE_BASE 110

// Discard cards
#define DISCARD_CARD_W 96
#define DISCARD_CARD_H 120
#define DISCARD_GAP 12
#define DISCARD_Y 74

// Deck browser (upgrade selection)
#define DECK_CARD_W 96
#define DECK_CARD_H 120
#define DECK_GAP 3
#define DECK_PER_ROW 7

// Card art source dimensions (for icon/art textures)
#define CARD_SOURCE_W 96
#define CARD_SOURCE_H 120

// Class portrait icon size
#define CLASS_PORTRAIT_SIZE 32
#define PORTRAIT_SIZE CLASS_PORTRAIT_SIZE

// Buttons
#define BTN_H 22
#define BTN_RADIUS 0.0f

#endif
