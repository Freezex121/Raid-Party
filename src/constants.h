#ifndef CONSTANTS_H
#define CONSTANTS_H

// Pixel-perfect render target and window
#define VIRT_W 427
#define VIRT_H 240
#define SCALE 3
#define SCREEN_W (VIRT_W * SCALE)
#define SCREEN_H (VIRT_H * SCALE)

// Party frames
#define FRAME_W 100
#define FRAME_H 24
#define FRAME_GAP 4
#define FRAME_Y 3

// Hand cards
#define HAND_CARD_W 60
#define HAND_CARD_H 75
#define HAND_GAP 3
#define HAND_Y (VIRT_H - HAND_CARD_H - 5)

// Enemy
#define ENEMY_SIZE 40
#define ENEMY_Y 80

// Cast bar
#define CAST_BAR_W 100
#define CAST_BAR_HEIGHT 10

// Reward cards
#define REWARD_CARD_W 66
#define REWARD_CARD_H 90
#define REWARD_GAP 7
#define REWARD_Y 100

// Deck browser (upgrade selection)
#define DECK_CARD_W 46
#define DECK_CARD_H 65
#define DECK_GAP 3
#define DECK_PER_ROW 7

// Card art source dimensions (for icon/art textures)
#define CARD_SOURCE_W 60
#define CARD_SOURCE_H 75

// Class portrait icon size
#define CLASS_PORTRAIT_SIZE 16
#define PORTRAIT_SIZE CLASS_PORTRAIT_SIZE

// Buttons
#define BTN_H 16
#define BTN_RADIUS 0.0f

#endif
