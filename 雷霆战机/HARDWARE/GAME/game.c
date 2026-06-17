#include "game.h"
#include "lcd.h"
#include "key.h"
#include "delay.h"
#include "stdlib.h"

// --- Global Game State ---
GameState game_state;
Player player;
Bullet pbullets[MAX_PLAYER_BULLETS];
Bullet ebullets[MAX_ENEMY_BULLETS];
Enemy enemies[MAX_ENEMIES];
Item items[MAX_ITEMS];
Star stars[MAX_STARS];

u8 boss_spawned = 0;
u8 boss_defeated = 0;
u16 spawn_timer = 0;
u8 alive_enemies = 0;

static u16 scr_w = 480;
static u16 scr_h = 800;
static u32 boss_threshold = SCORE_BOSS_THRESHOLD;  // score needed for next boss

// --- RNG ---
static u32 rng_seed = 12345;
static u32 Rand(void) {
    rng_seed = rng_seed * 1103515245 + 12345;
    return (rng_seed >> 16) & 0x7FFF;
}
static void RandSeed(u32 s) { rng_seed = s; }

static s16 Clamp(s16 v, s16 lo, s16 hi) {
    if (v < lo) return lo;
    if (v > hi) return hi;
    return v;
}

u8 CheckHit(s16 x1, s16 y1, u8 w1, u8 h1, s16 x2, s16 y2, u8 w2, u8 h2) {
    s16 hw, hh, dx, dy;
    hw = (w1 + w2) / 2 + 2;
    hh = (h1 + h2) / 2 + 2;
    dx = x1 - x2; if (dx < 0) dx = -dx;
    dy = y1 - y2; if (dy < 0) dy = -dy;
    return (dx < hw && dy < hh);
}

// --- Starfield ---
void InitStars(void) {
    u8 i;
    const u16 cs[] = {GRAY, LGRAY, DARKBLUE, LIGHTBLUE, GRAYBLUE};
    for (i = 0; i < MAX_STARS; i++) {
        stars[i].x = Rand() % scr_w;
        stars[i].y = Rand() % scr_h;
        stars[i].active = 1;
        stars[i].color = cs[Rand() % 5];
    }
}

void UpdateStars(void) {
    u8 i;
    for (i = 0; i < MAX_STARS; i++) {
        if (!stars[i].active) continue;
        // Erase old meteor
        if (stars[i].y >= GAME_AREA_TOP && stars[i].y < scr_h)
            LCD_Fill(stars[i].x, stars[i].y, stars[i].x + 1, stars[i].y + 3, BLACK);
        stars[i].y += 2;
        if (stars[i].y >= scr_h) {
            stars[i].y = GAME_AREA_TOP;
            stars[i].x = Rand() % scr_w;
        }
    }
}

void DrawStars(void) {
    u8 i;
    for (i = 0; i < MAX_STARS; i++) {
        if (!stars[i].active) continue;
        if (stars[i].y < GAME_AREA_TOP || stars[i].y >= scr_h) continue;
        LCD_Fill(stars[i].x, stars[i].y, stars[i].x + 1, stars[i].y + 3, stars[i].color);
    }
}

// --- Object Pool ---
static Bullet* GetFreePBullet(void) {
    u8 i;
    for (i = 0; i < MAX_PLAYER_BULLETS; i++)
        if (!pbullets[i].active) return &pbullets[i];
    return 0;
}
static Bullet* GetFreeEBullet(void) {
    u8 i;
    for (i = 0; i < MAX_ENEMY_BULLETS; i++)
        if (!ebullets[i].active) return &ebullets[i];
    return 0;
}
static Enemy* GetFreeEnemy(void) {
    u8 i;
    for (i = 0; i < MAX_ENEMIES; i++)
        if (!enemies[i].active) return &enemies[i];
    return 0;
}
static Item* GetFreeItem(void) {
    u8 i;
    for (i = 0; i < MAX_ITEMS; i++)
        if (!items[i].active) return &items[i];
    return 0;
}

// --- Erase helpers ---
static void EraseStar(Star* s) {
    if (!s->active) return;
    if (s->y < GAME_AREA_TOP || s->y >= scr_h) return;
    LCD_Fill(s->x, s->y, s->x + 1, s->y + 3, BLACK);
}
static void EraseBullet(Bullet* b) {
    if (!b->active) return;
    LCD_Fill(b->x - b->width / 2 - 1, b->y - b->height / 2 - 1,
             b->x + b->width / 2 + 1, b->y + b->height / 2 + 1, BLACK);
}
static void EraseItem(Item* it) {
    if (!it->active) return;
    LCD_Fill(it->x - ITEM_W / 2 - 2, it->y - ITEM_H / 2 - 2,
             it->x + ITEM_W / 2 + 2, it->y + ITEM_H / 2 + 2, BLACK);
}
static void ErasePlayer(void) {
    LCD_Fill(player.x - PLAYER_W / 2 - 2, player.y - PLAYER_H / 2 - 2,
             player.x + PLAYER_W / 2 + 2, player.y + PLAYER_H / 2 + 2, BLACK);
}
static void EraseSmallEnemy(Enemy* e) {
    LCD_Fill(e->x - ENEMY_SMALL_W / 2 - 2, e->y - ENEMY_SMALL_H / 2 - 2,
             e->x + ENEMY_SMALL_W / 2 + 2, e->y + ENEMY_SMALL_H / 2 + 2, BLACK);
}
static void EraseBoss(Enemy* e) {
    LCD_Fill(e->x - BOSS_W / 2 - 2, e->y - BOSS_H / 2 - 20,
             e->x + BOSS_W / 2 + 2, e->y + BOSS_H / 2 + 20, BLACK);
}

// --- Drawing ---
void DrawPlane(s16 x, s16 y, u16 color) {
    s16 hw = PLAYER_W / 2, hh = PLAYER_H / 2;
    s16 cx = x, top = y - hh, bot = y + hh, left = x - hw, right = x + hw;
    LCD_Fill(cx - 4, top, cx + 4, top + 10, color);
    LCD_Fill(cx - 6, top + 4, cx + 6, top + 7, color);
    LCD_Fill(cx - 4, top + 8, cx + 4, bot - 4, color);
    LCD_Fill(left, top + 12, cx - 4, bot - 8, color);
    LCD_Fill(cx + 4, top + 12, right, bot - 8, color);
    LCD_Fill(cx - 3, bot - 4, cx + 3, bot, color);
    LCD_Fill(cx - 2, top + 6, cx + 2, top + 12, WHITE);
    LCD_Fill(cx - 2, bot, cx + 2, bot + 2, YELLOW);
}

void DrawSmallEnemy(s16 x, s16 y, u16 color) {
    s16 h = ENEMY_SMALL_H / 2, w = ENEMY_SMALL_W / 2;
    LCD_Fill(x - w + 3, y - 4, x + w - 3, y + 4, color);
    LCD_Fill(x - 3, y - h, x + 3, y - 2, color);
    LCD_Fill(x - 3, y + 2, x + 3, y + h, color);
    LCD_Fill(x - w, y - 3, x - w + 3, y + 3, color);
    LCD_Fill(x + w - 3, y - 3, x + w, y + 3, color);
    LCD_Fill(x - 2, y - 2, x + 2, y + 2, YELLOW);
}

void DrawBoss(s16 x, s16 y) {
    s16 hw = BOSS_W / 2, hh = BOSS_H / 2;
    s16 left = x - hw, right = x + hw, top = y - hh, bot = y + hh;
    LCD_Fill(left + 4, top + 8, right - 4, bot - 4, BRED);
    LCD_Fill(left + 6, top + 10, right - 6, bot - 6, RED);
    LCD_Fill(left, top + 16, left + 8, bot - 12, BRED);
    LCD_Fill(left + 2, top + 18, left + 6, bot - 14, RED);
    LCD_Fill(right - 8, top + 16, right, bot - 12, BRED);
    LCD_Fill(right - 6, top + 18, right - 2, bot - 14, RED);
    LCD_Fill(x - 10, top + 10, x + 10, top + 26, BLACK);
    LCD_Fill(x - 6, top + 14, x + 6, top + 24, YELLOW);
    LCD_Fill(x - 2, top + 16, x + 2, top + 22, RED);
    LCD_Fill(left + 10, bot - 8, left + 18, bot, GRAY);
    LCD_Fill(right - 18, bot - 8, right - 10, bot, GRAY);
}

void DrawBossHPBar(void) {
    u16 bar_w = BOSS_W, bar_h = 8, fill_w;
    s16 bx, by;
    Enemy* boss = 0;
    u8 i;
    for (i = 0; i < MAX_ENEMIES; i++) {
        if (enemies[i].active && enemies[i].etype == 1) { boss = &enemies[i]; break; }
    }
    if (!boss) return;
    bx = boss->x - bar_w / 2;
    by = boss->y - BOSS_H / 2 - 14;
    LCD_Fill(bx - 1, by - 1, bx + bar_w + 1, by + bar_h + 1, BLACK);
    LCD_Fill(bx, by, bx + bar_w, by + bar_h, GRAY);
    if (boss->max_hp > 0) {
        fill_w = (u16)((u32)boss->hp * bar_w / boss->max_hp);
        if (fill_w > 0) LCD_Fill(bx, by, bx + fill_w, by + bar_h, GREEN);
    }
}

void DrawBullet(Bullet* b) {
    u16 c;
    if (!b->active) return;
    c = (b->type == BULLET_PLAYER) ? YELLOW : RED;
    LCD_Fill(b->x - b->width / 2, b->y - b->height / 2,
             b->x + b->width / 2, b->y + b->height / 2, c);
}

void DrawItem(Item* it) {
    s16 hw, hh;
    if (!it->active) return;
    hw = ITEM_W / 2; hh = ITEM_H / 2;
    // White border
    LCD_Fill(it->x - hw - 1, it->y - hh - 1, it->x + hw + 1, it->y + hh + 1, WHITE);
    LCD_Fill(it->x - hw, it->y - hh, it->x + hw, it->y + hh, BLACK);
    switch (it->type) {
        case ITEM_LIFE:
            LCD_Fill(it->x - hw + 2, it->y - 3, it->x + hw - 2, it->y + 3, RED);
            LCD_Fill(it->x - 3, it->y - hh + 2, it->x + 3, it->y + hh - 2, RED);
            break;
        case ITEM_BULLET_UP:
            LCD_Fill(it->x - hw + 2, it->y - hh + 2, it->x + hw - 2, it->y + hh - 2, YELLOW);
            POINT_COLOR = BLACK; BACK_COLOR = YELLOW;
            LCD_ShowChar(it->x - 4, it->y - 8, 'B', 16, 0);
            break;
        case ITEM_FIRE_RATE:
            LCD_Fill(it->x - hw + 2, it->y - hh + 2, it->x + hw - 2, it->y + hh - 2, GREEN);
            POINT_COLOR = BLACK; BACK_COLOR = GREEN;
            LCD_ShowChar(it->x - 4, it->y - 8, 'S', 16, 0);
            break;
        case ITEM_BOMB:
            LCD_Fill(it->x - hw + 1, it->y - hh + 1, it->x + hw - 1, it->y + hh - 1, GRAY);
            LCD_Fill(it->x - 3, it->y - 3, it->x + 3, it->y + 3, BLACK);
            LCD_Fast_DrawPoint(it->x, it->y - hh + 2, WHITE);
            break;
    }
}

void DrawHUD(void) {
    LCD_Fill(0, 0, scr_w - 1, GAME_AREA_TOP - 1, BLACK);
    POINT_COLOR = WHITE; BACK_COLOR = BLACK;
    LCD_ShowString(4, 4, 80, 16, 16, (u8*)"SC:");
    LCD_ShowNum(28, 4, player.score, 5, 16);
    LCD_ShowString(110, 4, 60, 16, 16, (u8*)"HP:");
    LCD_ShowNum(136, 4, player.lives, 1, 16);
    LCD_ShowString(160, 4, 60, 16, 16, (u8*)"PWR:");
    LCD_ShowNum(196, 4, player.bullet_lv, 1, 16);
    LCD_ShowString(220, 4, 60, 16, 16, (u8*)"SPD:");
    LCD_ShowNum(256, 4, player.fire_rate, 1, 16);
    LCD_ShowString(280, 4, 80, 16, 16, (u8*)"BOMB:");
    LCD_ShowNum(328, 4, player.bombs, 1, 16);
    LCD_Fill(0, GAME_AREA_TOP - 2, scr_w - 1, GAME_AREA_TOP - 1, GRAY);
}

// --- Spawning ---
void FirePlayerBullets(void) {
    Bullet* b;
    if (player.bullet_lv == 1) {
        b = GetFreePBullet();
        if (b) { b->x = player.x; b->y = player.y - PLAYER_H / 2 - 4;
                 b->width = PBULLET_W; b->height = PBULLET_H;
                 b->active = 1; b->type = BULLET_PLAYER; }
    } else if (player.bullet_lv == 2) {
        b = GetFreePBullet();
        if (b) { b->x = player.x - 6; b->y = player.y - PLAYER_H / 2 - 4;
                 b->width = PBULLET_W; b->height = PBULLET_H;
                 b->active = 1; b->type = BULLET_PLAYER; }
        b = GetFreePBullet();
        if (b) { b->x = player.x + 6; b->y = player.y - PLAYER_H / 2 - 4;
                 b->width = PBULLET_W; b->height = PBULLET_H;
                 b->active = 1; b->type = BULLET_PLAYER; }
    } else {
        b = GetFreePBullet();
        if (b) { b->x = player.x; b->y = player.y - PLAYER_H / 2 - 6;
                 b->width = PBULLET_W; b->height = PBULLET_H;
                 b->active = 1; b->type = BULLET_PLAYER; }
        b = GetFreePBullet();
        if (b) { b->x = player.x - 8; b->y = player.y - PLAYER_H / 2;
                 b->width = PBULLET_W; b->height = PBULLET_H;
                 b->active = 1; b->type = BULLET_PLAYER; }
        b = GetFreePBullet();
        if (b) { b->x = player.x + 8; b->y = player.y - PLAYER_H / 2;
                 b->width = PBULLET_W; b->height = PBULLET_H;
                 b->active = 1; b->type = BULLET_PLAYER; }
    }
}

void SpawnItem(s16 x, s16 y) {
    Item* it;
    if (Rand() % 100 >= 35) return;
    it = GetFreeItem();
    if (!it) return;
    it->x = x; it->y = y;
    it->width = ITEM_W; it->height = ITEM_H;
    it->active = 1;
    it->type = (ItemType)(Rand() % 4);
}

void DoBomb(void) {
    u8 i;
    if (player.bombs == 0) return;
    player.bombs--;
    for (i = 0; i < MAX_ENEMIES; i++) {
        if (enemies[i].active) {
            if (enemies[i].etype == 0) {
                EraseSmallEnemy(&enemies[i]);
                enemies[i].active = 0;
            } else {
                enemies[i].hp = (enemies[i].hp > 15) ? enemies[i].hp - 15 : 0;
                if (enemies[i].hp == 0) {
                    EraseBoss(&enemies[i]);
                    enemies[i].active = 0;
                    boss_defeated = 1;
                    player.score += SCORE_BOSS;
                }
            }
        }
    }
    for (i = 0; i < MAX_ENEMY_BULLETS; i++) {
        if (ebullets[i].active) { EraseBullet(&ebullets[i]); ebullets[i].active = 0; }
    }
}

static void SpawnOneEnemy(void) {
    Enemy* e = GetFreeEnemy();
    if (!e) return;
    e->x = 40 + (Rand() % (scr_w - 80));
    e->y = GAME_AREA_TOP + ENEMY_SMALL_H / 2;
    e->width = ENEMY_SMALL_W;
    e->height = ENEMY_SMALL_H;
    e->hp = ENEMY_HP;
    e->max_hp = ENEMY_HP;
    e->etype = 0;
    e->pattern = Rand() % 3;
    e->move_cnt = 0;
    e->fire_cd = 0;
    e->base_x = e->x;
    e->speed_y = ENEMY_SPEED_Y + (Rand() % 2);
    e->active = 1;
    alive_enemies++;
}

void SpawnBoss(void) {
    Enemy* e = GetFreeEnemy();
    if (!e) return;
    e->x = scr_w / 2;
    e->y = GAME_AREA_TOP + BOSS_H / 2 + 10;
    e->width = BOSS_W; e->height = BOSS_H;
    e->hp = BOSS_MAX_HP; e->max_hp = BOSS_MAX_HP;
    e->etype = 1; e->pattern = 0;
    e->move_cnt = 0; e->fire_cd = BOSS_FIRE_INTERVAL;
    e->base_x = e->x; e->speed_y = 0;
    e->active = 1;
    boss_spawned = 1;
}

// --- Game Init / Start ---
void Game_Init(void) {
    u8 i;
    scr_w = lcddev.width;
    scr_h = lcddev.height;
    for (i = 0; i < MAX_PLAYER_BULLETS; i++) pbullets[i].active = 0;
    for (i = 0; i < MAX_ENEMY_BULLETS; i++) ebullets[i].active = 0;
    for (i = 0; i < MAX_ENEMIES; i++) enemies[i].active = 0;
    for (i = 0; i < MAX_ITEMS; i++) items[i].active = 0;
    for (i = 0; i < MAX_STARS; i++) stars[i].active = 0;
    InitStars();
    game_state = STATE_START;
}

void Game_Start(void) {
    u8 i;
    for (i = 0; i < MAX_PLAYER_BULLETS; i++) pbullets[i].active = 0;
    for (i = 0; i < MAX_ENEMY_BULLETS; i++) ebullets[i].active = 0;
    for (i = 0; i < MAX_ENEMIES; i++) enemies[i].active = 0;
    for (i = 0; i < MAX_ITEMS; i++) items[i].active = 0;

    player.x = scr_w / 2;
    player.y = scr_h - 60;
    player.lives = PLAYER_INIT_LIVES;
    player.bullet_lv = 1;
    player.fire_rate = 1;
    player.bombs = 0;
    player.fire_cd = 0;
    player.inv_frames = 0;
    player.score = 0;

    boss_spawned = 0;
    boss_defeated = 0;
    spawn_timer = 20;
    alive_enemies = 0;
    boss_threshold = SCORE_BOSS_THRESHOLD;

    RandSeed(54321);
    LCD_Fill(GAME_AREA_LEFT, GAME_AREA_TOP, scr_w - 1, scr_h - 1, BLACK);
    game_state = STATE_PLAYING;
}

// --- Game Update ---
void Game_Update(u8 key) {
    u8 i, j;
    Bullet* b;
    Enemy* e;
    Item* it;
    u16 interval;

    if (game_state != STATE_PLAYING) return;

    // ===== Stars: erase old → move (drawn by Game_Render) =====
    for (i = 0; i < MAX_STARS; i++) {
        if (!stars[i].active) continue;
        EraseStar(&stars[i]);
        stars[i].y += 2;
        if (stars[i].y >= scr_h) {
            stars[i].y = GAME_AREA_TOP;
            stars[i].x = Rand() % scr_w;
        }
    }

    // ===== Player: erase old → move =====
    ErasePlayer();
    switch (key) {
        case KEY_UP_PRES: player.y -= PLAYER_SPEED; break;
        case KEY1_PRES:   player.y += PLAYER_SPEED; break;
        case KEY2_PRES:   player.x += PLAYER_SPEED; break;
        case KEY0_PRES:   player.x -= PLAYER_SPEED; break;
        default: break;
    }
    player.x = Clamp(player.x, PLAYER_W / 2, scr_w - 1 - PLAYER_W / 2);
    player.y = Clamp(player.y, GAME_AREA_TOP + PLAYER_H / 2 + 4, scr_h - 1 - PLAYER_H / 2);
    if (player.inv_frames > 0) player.inv_frames--;

    // ===== Auto-fire =====
    if (player.fire_rate == 3) interval = FIRE_FAST;
    else if (player.fire_rate == 2) interval = FIRE_MEDIUM;
    else interval = FIRE_SLOW;
    if (player.fire_cd == 0) { FirePlayerBullets(); player.fire_cd = interval; }
    if (player.fire_cd > 0) player.fire_cd--;

    // ===== Bullets: erase old → move (per object, no bulk) =====
    for (i = 0; i < MAX_PLAYER_BULLETS; i++) {
        b = &pbullets[i];
        if (!b->active) continue;
        EraseBullet(b);
        b->y -= PBULLET_SPEED;
        if (b->y < GAME_AREA_TOP) b->active = 0;
    }
    for (i = 0; i < MAX_ENEMY_BULLETS; i++) {
        b = &ebullets[i];
        if (!b->active) continue;
        EraseBullet(b);
        b->y += EBULLET_SPEED;
        if (b->y > scr_h) b->active = 0;
    }

    // ===== Enemies: erase old → move (per object) =====
    for (i = 0; i < MAX_ENEMIES; i++) {
        e = &enemies[i];
        if (!e->active) continue;
        if (e->etype == 0) {
            EraseSmallEnemy(e);
            e->move_cnt++;
            if (e->pattern == PAT_STRAIGHT) {
                e->y += e->speed_y;
            } else if (e->pattern == PAT_ZIGZAG) {
                e->y += e->speed_y;
                if ((e->move_cnt / 4) % 2 == 0) e->x += 2; else e->x -= 2;
                e->x = Clamp(e->x, ENEMY_SMALL_W, scr_w - 1 - ENEMY_SMALL_W);
            } else {
                e->y += e->speed_y;
                if (e->x < scr_w / 2) e->x += 1; else e->x -= 1;
            }
            if (e->y > scr_h + ENEMY_SMALL_H) {
                e->active = 0;
                if (alive_enemies > 0) alive_enemies--;
            }
        } else {
            EraseBoss(e);
            e->move_cnt++;
            e->x = e->base_x + (s16)(BOSS_SPEED * 8 *
                ((e->move_cnt % 60 < 30) ? e->move_cnt % 30 : 60 - e->move_cnt % 30) / 60);
            if (e->fire_cd == 0) {
                Bullet* eb;
                eb = GetFreeEBullet();
                if (eb) { eb->x = e->x; eb->y = e->y + BOSS_H / 2;
                          eb->width = EBULLET_R * 2; eb->height = EBULLET_R * 2;
                          eb->active = 1; eb->type = BULLET_ENEMY; }
                eb = GetFreeEBullet();
                if (eb) { eb->x = e->x - BOSS_W / 3; eb->y = e->y + BOSS_H / 2 - 4;
                          eb->width = EBULLET_R * 2; eb->height = EBULLET_R * 2;
                          eb->active = 1; eb->type = BULLET_ENEMY; }
                eb = GetFreeEBullet();
                if (eb) { eb->x = e->x + BOSS_W / 3; eb->y = e->y + BOSS_H / 2 - 4;
                          eb->width = EBULLET_R * 2; eb->height = EBULLET_R * 2;
                          eb->active = 1; eb->type = BULLET_ENEMY; }
                e->fire_cd = BOSS_FIRE_INTERVAL;
            }
            if (e->fire_cd > 0) e->fire_cd--;
        }
    }

    // ===== Items: erase old → move (per object) =====
    for (i = 0; i < MAX_ITEMS; i++) {
        it = &items[i];
        if (!it->active) continue;
        EraseItem(it);
        it->y += ITEM_SPEED;
        if (it->y > scr_h) it->active = 0;
    }

    // ===== Collision: player bullets vs enemies =====
    for (i = 0; i < MAX_PLAYER_BULLETS; i++) {
        b = &pbullets[i];
        if (!b->active) continue;
        for (j = 0; j < MAX_ENEMIES; j++) {
            e = &enemies[j];
            if (!e->active) continue;
            if (CheckHit(b->x, b->y, b->width, b->height,
                         e->x, e->y, e->width, e->height)) {
                b->active = 0;
                e->hp--;
                if (e->hp == 0) {
                    if (e->etype == 0) {
                        e->active = 0;
                        player.score += SCORE_KILL;
                        SpawnItem(e->x, e->y);
                        if (alive_enemies > 0) alive_enemies--;
                    } else {
                        e->active = 0;
                        boss_defeated = 1;
                        player.score += SCORE_BOSS;
                    }
                }
                break;
            }
        }
    }

    // ===== Collision: enemy bullets vs player =====
    if (player.inv_frames == 0) {
        for (i = 0; i < MAX_ENEMY_BULLETS; i++) {
            b = &ebullets[i];
            if (!b->active) continue;
            if (CheckHit(b->x, b->y, b->width, b->height,
                         player.x, player.y, PLAYER_W, PLAYER_H)) {
                b->active = 0;
                if (player.bombs > 0) {
                    DoBomb();
                    player.inv_frames = INVINCIBLE_FRAMES;
                } else {
                    player.lives--;
                    player.inv_frames = INVINCIBLE_FRAMES;
                    if (player.lives == 0) {
                        game_state = STATE_GAMEOVER;
                        Game_DrawGameOver();
                        return;
                    }
                }
                break;
            }
        }
    }

    // ===== Collision: enemies vs player =====
    if (player.inv_frames == 0) {
        for (i = 0; i < MAX_ENEMIES; i++) {
            e = &enemies[i];
            if (!e->active) continue;
            if (CheckHit(player.x, player.y, PLAYER_W, PLAYER_H,
                         e->x, e->y, e->width, e->height)) {
                if (player.bombs > 0) {
                    DoBomb();
                    player.inv_frames = INVINCIBLE_FRAMES;
                } else {
                    if (e->etype == 0) {
                        e->active = 0;
                        if (alive_enemies > 0) alive_enemies--;
                    }
                    player.lives--;
                    player.inv_frames = INVINCIBLE_FRAMES;
                    if (player.lives == 0) {
                        game_state = STATE_GAMEOVER;
                        Game_DrawGameOver();
                        return;
                    }
                }
                break;
            }
        }
    }

    // ===== Collision: player vs items =====
    for (i = 0; i < MAX_ITEMS; i++) {
        it = &items[i];
        if (!it->active) continue;
        if (CheckHit(player.x, player.y, PLAYER_W, PLAYER_H,
                     it->x, it->y, ITEM_W, ITEM_H)) {
            EraseItem(it);
            switch (it->type) {
                case ITEM_LIFE:      if (player.lives < PLAYER_MAX_LIVES) player.lives++; break;
                case ITEM_BULLET_UP: if (player.bullet_lv < 3) player.bullet_lv++; break;
                case ITEM_FIRE_RATE: if (player.fire_rate < 3) player.fire_rate++; break;
                case ITEM_BOMB:      if (player.bombs < 5) player.bombs++; break;
            }
            player.score += SCORE_ITEM;
            it->active = 0;
        }
    }

    // ===== Infinite enemy spawning =====
    if (!boss_spawned && player.score >= boss_threshold) {
        // Score reached: spawn boss
        SpawnBoss();
    }

    if (!boss_spawned) {
        // Spawn regular enemies continuously
        if (spawn_timer == 0) {
            SpawnOneEnemy();
            // Spawn interval decreases as player score increases
            if (player.score < 500)      spawn_timer = 25;
            else if (player.score < 1500) spawn_timer = 18;
            else                          spawn_timer = 12;
        }
        if (spawn_timer > 0) spawn_timer--;
    }

    // ===== Check boss dead -> continue play =====
    if (boss_defeated) {
        boss_spawned = 0;
        boss_defeated = 0;
        boss_threshold += SCORE_BOSS_STEP;  // next boss at higher score
        LCD_Fill(GAME_AREA_LEFT, GAME_AREA_TOP, scr_w - 1, scr_h - 1, BLACK);
    }

    DrawHUD();
}

// --- Game Render ---
void Game_Render(void) {
    u8 i;
    if (game_state != STATE_PLAYING) return;

    // Draw stars (background layer)
    DrawStars();

    // Draw items
    for (i = 0; i < MAX_ITEMS; i++)
        if (items[i].active) DrawItem(&items[i]);

    // Draw player bullets
    for (i = 0; i < MAX_PLAYER_BULLETS; i++)
        if (pbullets[i].active) DrawBullet(&pbullets[i]);

    // Draw enemy bullets
    for (i = 0; i < MAX_ENEMY_BULLETS; i++)
        if (ebullets[i].active) DrawBullet(&ebullets[i]);

    // Draw enemies
    for (i = 0; i < MAX_ENEMIES; i++) {
        if (!enemies[i].active) continue;
        if (enemies[i].etype == 1) {
            DrawBoss(enemies[i].x, enemies[i].y);
            DrawBossHPBar();
        } else {
            DrawSmallEnemy(enemies[i].x, enemies[i].y, RED);
        }
    }

    // Draw player (flash when invincible)
    if (player.inv_frames == 0 || (player.inv_frames / 5) % 2 == 0)
        DrawPlane(player.x, player.y, CYAN);

    DrawHUD();
}

// --- Screens ---
void Game_DrawStartPage(void) {
    LCD_Clear(BLACK);
    POINT_COLOR = WHITE; BACK_COLOR = BLACK;
    LCD_ShowString(100, 140, 280, 36, 24, (u8*)"THUNDER");
    LCD_ShowString(135, 190, 210, 36, 24, (u8*)"FIGHTER");
    POINT_COLOR = CYAN;
    LCD_ShowString(110, 260, 260, 20, 16, (u8*)"Lei Ting Zhan Ji");
    POINT_COLOR = WHITE;
    LCD_ShowString(40, 380, 400, 20, 16, (u8*)"KEY2:Right  KEY1:Down");
    LCD_ShowString(40, 410, 400, 20, 16, (u8*)"KEY0:Left   KEY_UP:Up");
    LCD_ShowString(40, 450, 400, 20, 16, (u8*)"Score 3000 -> Boss");
    POINT_COLOR = YELLOW;
    LCD_ShowString(90, 530, 300, 24, 16, (u8*)"Press KEY_UP to Start");
    DrawPlane(scr_w / 2, 630, CYAN);
}

void Game_DrawGameOver(void) {
    LCD_Fill(GAME_AREA_LEFT, GAME_AREA_TOP, scr_w - 1, scr_h - 1, BLACK);
    POINT_COLOR = RED; BACK_COLOR = BLACK;
    LCD_ShowString(100, 280, 280, 40, 32, (u8*)"GAME OVER");
    POINT_COLOR = WHITE;
    LCD_ShowString(90, 370, 300, 20, 16, (u8*)"Score:");
    LCD_ShowNum(160, 370, player.score, 6, 16);
    POINT_COLOR = YELLOW;
    LCD_ShowString(90, 500, 300, 24, 16, (u8*)"KEY_UP to Restart");
}

void Game_DrawWin(void) {
    LCD_Fill(GAME_AREA_LEFT, GAME_AREA_TOP, scr_w - 1, scr_h - 1, BLACK);
    POINT_COLOR = GREEN; BACK_COLOR = BLACK;
    LCD_ShowString(130, 260, 220, 40, 32, (u8*)"YOU WIN!");
    POINT_COLOR = WHITE;
    LCD_ShowString(90, 370, 300, 20, 16, (u8*)"Score:");
    LCD_ShowNum(160, 370, player.score, 6, 16);
    POINT_COLOR = YELLOW;
    LCD_ShowString(90, 500, 300, 24, 16, (u8*)"KEY_UP to Restart");
}

u8 Game_GetState(void) {
    return (u8)game_state;
}
