#ifndef CONSTANTS_H
#define CONSTANTS_H

// Pixel-perfect render target and window
#define VIRT_W 640
#define VIRT_H 360
#define SCALE 1
#define SCREEN_W (VIRT_W * SCALE)
#define SCREEN_H (VIRT_H * SCALE)

// Party frames
#define FRAME_W 108
#define FRAME_H 34
#define FRAME_GAP 6
#define FRAME_Y 8

// Hand cards
#define HAND_CARD_W 72
#define HAND_CARD_H 90
#define HAND_GAP 5
#define HAND_BOTTOM_MARGIN 10
#define HAND_Y (VIRT_H - HAND_CARD_H - HAND_BOTTOM_MARGIN)

// Enemy
#define ENEMY_SIZE 56
#define ENEMY_Y 145

// Cast bar
#define CAST_BAR_W 132
#define CAST_BAR_HEIGHT 14

// Reward cards
#define REWARD_CARD_W 96
#define REWARD_CARD_H 120
#define REWARD_GAP 12
#define REWARD_Y 74

// Deck browser (upgrade selection)
#define DECK_CARD_W 64
#define DECK_CARD_H 80
#define DECK_GAP 6
#define DECK_PER_ROW 7

// Card art source dimensions (for icon/art textures)
#define CARD_SOURCE_W 60
#define CARD_SOURCE_H 75

// Class portrait icon size
#define CLASS_PORTRAIT_SIZE 32
#define PORTRAIT_SIZE CLASS_PORTRAIT_SIZE

// Buttons
#define BTN_H 22
#define BTN_RADIUS 0.0f

#endif
