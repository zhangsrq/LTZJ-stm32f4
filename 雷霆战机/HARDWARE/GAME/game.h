#ifndef __GAME_H
#define __GAME_H
#include "sys.h"

//////////////////////////////////////////////////////////////////////////////////
// Thunder Fighter Game - STM32F407 Explorer Board
// NT35510 LCD 480x800, KEY0/KEY1/KEY2/KEY_UP controls
//////////////////////////////////////////////////////////////////////////////////

// --- Game States ---
typedef enum {
    STATE_START = 0,
    STATE_PLAYING,
    STATE_GAMEOVER,
    STATE_WIN
} GameState;

// --- Object Limits ---
#define MAX_PLAYER_BULLETS  64
#define MAX_ENEMY_BULLETS   50
#define MAX_ENEMIES         15
#define MAX_ITEMS           8
#define MAX_STARS           80

// --- Player ---
#define PLAYER_W            26
#define PLAYER_H            34
#define PLAYER_SPEED        6
#define PLAYER_INIT_LIVES   3
#define PLAYER_MAX_LIVES    5

// --- Bullets ---
#define PBULLET_W           3
#define PBULLET_H           12
#define PBULLET_SPEED       14
#define EBULLET_R           3
#define EBULLET_SPEED       5

// --- Fire rates (frames between shots) ---
#define FIRE_SLOW           15
#define FIRE_MEDIUM         8
#define FIRE_FAST           4

// --- Enemies ---
#define ENEMY_SMALL_W       22
#define ENEMY_SMALL_H       22
#define ENEMY_HP            1
#define ENEMY_SPEED_Y       3
#define BOSS_MAX_HP         30
#define BOSS_W              70
#define BOSS_H              60
#define BOSS_FIRE_INTERVAL  18
#define BOSS_SPEED          4

// --- Items ---
#define ITEM_W              22
#define ITEM_H              22
#define ITEM_SPEED          3

// --- Waves ---
#define WAVE1_COUNT         12
#define WAVE2_COUNT         18
#define WAVE_SPAWN_INTERVAL 12
#define WAVE_COOLDOWN       90

// --- Screen ---
#define GAME_AREA_LEFT      0
#define GAME_AREA_TOP       32
#define INVINCIBLE_FRAMES   90   // 3 seconds at 30fps

// --- Score ---
#define SCORE_KILL          100
#define SCORE_BOSS          2000
#define SCORE_ITEM          50
#define SCORE_BOSS_THRESHOLD 3000
#define SCORE_BOSS_STEP      7000  // score gap between bosses

// --- Item types ---
typedef enum {
    ITEM_LIFE = 0,
    ITEM_BULLET_UP,
    ITEM_FIRE_RATE,
    ITEM_BOMB
} ItemType;

// --- Bullet type ---
typedef enum {
    BULLET_PLAYER = 0,
    BULLET_ENEMY
} BulletType;

// --- Enemy move patterns ---
#define PAT_STRAIGHT  0
#define PAT_ZIGZAG    1
#define PAT_DIAGONAL  2

// --- Data Structures ---

typedef struct {
    s16 x, y;
    u8  active;
    u16 color;
} Star;

typedef struct {
    s16 x, y;
    u8  active;
    u8  width, height;
    BulletType type;
} Bullet;

typedef struct {
    s16 x, y;
    u8  active;
    u8  width, height;
    u16 hp;
    u16 max_hp;
    u8  etype;          // 0=small, 1=boss
    u8  pattern;
    u16 move_cnt;
    u16 fire_cd;
    s16 base_x;
    s16 speed_y;
} Enemy;

typedef struct {
    s16 x, y;
    u8  active;
    u8  width, height;
    ItemType type;
} Item;

typedef struct {
    s16 x, y;
    u8  lives;
    u8  bullet_lv;
    u8  fire_rate;
    u8  bombs;
    u16 fire_cd;
    u16 inv_frames;
    u32 score;
} Player;

// --- Globals ---
extern GameState game_state;
extern Player player;
extern Bullet pbullets[MAX_PLAYER_BULLETS];
extern Bullet ebullets[MAX_ENEMY_BULLETS];
extern Enemy enemies[MAX_ENEMIES];
extern Item items[MAX_ITEMS];
extern Star stars[MAX_STARS];

extern u8 boss_spawned;
extern u8 boss_defeated;
extern u16 spawn_timer;
extern u8 alive_enemies;

// --- API ---
void Game_Init(void);
void Game_Start(void);
void Game_Update(u8 key);
void Game_Render(void);
void Game_DrawStartPage(void);
void Game_DrawGameOver(void);
void Game_DrawWin(void);
u8   Game_GetState(void);

// --- Helpers ---
u8   CheckHit(s16 x1, s16 y1, u8 w1, u8 h1, s16 x2, s16 y2, u8 w2, u8 h2);
void SpawnItem(s16 x, s16 y);
void FirePlayerBullets(void);
void DoBomb(void);
void DrawPlane(s16 x, s16 y, u16 color);
void DrawSmallEnemy(s16 x, s16 y, u16 color);
void DrawBoss(s16 x, s16 y);
void DrawHUD(void);
void InitStars(void);
void UpdateStars(void);
void DrawStars(void);

#endif
