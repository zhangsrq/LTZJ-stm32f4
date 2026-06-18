# 雷霆战机 — 代码说明文档

## 项目概述

| 项目 | 说明 |
|------|------|
| 平台 | 正点原子探索者 STM32F407ZG |
| LCD | NT35510 4.3寸 480×800，FSMC 16位并口 |
| 外部 SRAM | IS62WV51216 1MB，FSMC Bank1 NE3 |
| 按键 | KEY0(左) KEY1(下) KEY2(右) KEY_UP(上) |
| 编译环境 | Keil MDK-ARM，标准外设库，C89 |
| 帧率 | ~120 FPS（无 delay，全速运行） |

---

## 一、文件结构

```
雷霆战机/
├── USER/
│   ├── main.c              → 主入口 + 游戏主循环
│   ├── ltzj.uvprojx        → Keil 工程文件
│   ├── ltzj.uvoptx
│   ├── stm32f4xx_it.c      → 中断服务
│   └── system_stm32f4xx.c  → 系统时钟 168MHz
├── HARDWARE/
│   ├── KEY/   key.h key.c         → 按键驱动
│   ├── GAME/  game.h game.c       → 游戏核心
│   ├── LCD/   lcd.h lcd.c FONT.H  → NT35510 驱动
│   ├── LED/   led.h led.c         → 板载 LED
│   └── SRAM/  sram.h sram.c       → 外部 SRAM 初始化（双缓冲）
├── SYSTEM/  delay/ sys/ usart/    → 系统组件
├── CORE/    startup + CMSIS 头    → ARM Cortex-M4 启动
└── FWLIB/   STM32F4 标准外设库     → GPIO/FSMC/RCC/USART
```

---

## 二、主循环 (main.c)

```
main()
 ├─ NVIC_PriorityGroupConfig(2)
 ├─ delay_init(168)           SysTick 168MHz
 ├─ uart_init(115200)         USART1
 ├─ LED_Init()                PF9/PF10
 ├─ LCD_Init()                FSMC NE4 + NT35510 初始化
 ├─ KEY_Init()                PA0 PE2 PE3 PE4
 ├─ SRAM_Init()               FSMC NE3 → 1MB 外部 SRAM
 ├─ Game_Init()               创建所有对象池
 ├─ Game_DrawStartPage()      标题画面
 └─ while(1)
      ├─ key = KEY_Scan(1)    连续模式扫描按键
      ├─ switch(Game_GetState())
      │    ├─ STATE_START    → KEY_UP → Game_Start()
      │    ├─ STATE_PLAYING  → Game_Update(key) → Game_Render()
      │    ├─ STATE_GAMEOVER → KEY_UP → Game_Start()
      │    └─ STATE_WIN      → KEY_UP → Game_Start()
      └─ (空循环，无 delay — 全速运行)
```

**帧率**: 主循环无 `delay_ms`，跑满 CPU。实际帧率取决于每帧计算 + 绘制 + 双缓冲搬移的总耗时。

---

## 三、双缓冲架构（核心设计）

### 3.1 为什么需要双缓冲

传统的"擦旧→画新"模式在大量对象时会有闪烁：擦除瞬间露出黑色背景，然后重绘——人眼看到的就是黑色闪烁。**双缓冲**在外部 SRAM 中准备完整的一帧画面，一次性搬到 LCD，LCD 始终只看到完整帧。

### 3.2 实现方式

```
外部 SRAM (IS62WV51216 @ 0x68000000):
┌─────────────────────────┐
│ 000000                  │
│  ...  480×768 缓冲区    │  ← buf_Fill / buf_Point 写入这里
│  ...  (368,640 像素)    │
│ 05A000                  │
└─────────────────────────┘

每帧流程:
  Game_Render()
    ├─ buf_Fill(0,32,479,799, BLACK)    清空缓冲区
    ├─ DrawStars()                      星星画到缓冲区
    ├─ DrawItems() × N                  道具画到缓冲区
    ├─ DrawBullets() × N                子弹画到缓冲区
    ├─ DrawEnemies() × N                敌人+Boss画到缓冲区
    ├─ DrawPlane()                      玩家画到缓冲区
    ├─ buf_Flush()                      737KB 从 SRAM → LCD GRAM
    └─ DrawHUD()                        HUD 直接画 LCD（不再经过缓冲区）
```

### 3.3 buf_Fill / buf_Point / buf_Flush

```c
// 写矩形像素到 SRAM（不触发 LCD IO）
static void buf_Fill(sx, sy, ex, ey, color) {
    yoff = sy - 32;          // 去 HUD 偏移
    for (y = yoff..endY)
        for (x = 0..width)
            SRAM[y * 480 + x] = color;
}

// 整帧搬运 SRAM → LCD GRAM
static void buf_Flush(void) {
    LCD_SetCursor(0, 32);
    LCD_WriteRAM_Prepare();
    for (i = 0..368640)
        LCD->LCD_RAM = *p++;    // 16-bit FSMC 直写
}
```

**性能代价**: `buf_Flush` 搬 368640 个像素，每个 ~71ns（FSMC 写时序），约 **26ms**。这是帧率的主要瓶颈。

### 3.4 Game_Update → 纯数据层

```
Game_Update(key):
  ├─ UpdateStars()          移动星星 Y 坐标
  ├─ 按键 → 移动玩家 XY
  ├─ 自动射击冷却 → FirePlayerBullets()
  ├─ 所有子弹 XY 移动 + 边界回收
  ├─ 所有敌人 XY 移动 + Boss 射击
  ├─ 所有道具 XY 移动 + 边界回收
  ├─ 四组碰撞检测 (见第五节)
  ├─ 敌兵定时生成 + Boss 分数触发
  └─ Boss 死亡 → 清场继续
```

**关键**: `Game_Update` 里没有任何 `LCD_Fill` 或 `buf_Fill`——只改坐标数据。所有绘制在 `Game_Render` 中完成。

---

## 四、对象池设计

所有对象用 **固定数组 + active 标志**，不动态分配内存：

| 池 | 容量 | 结构大小 | 总内存 |
|----|------|----------|--------|
| 玩家子弹 `pbullets[]` | 64 | 8 字节 | 512B |
| 敌方子弹 `ebullets[]` | 50 | 8 字节 | 400B |
| 敌人 `enemies[]` | 15 | 22 字节 | 330B |
| 道具 `items[]` | 8 | 10 字节 | 80B |
| 星星 `stars[]` | 80 | 6 字节 | 480B |

分配新对象时遍历数组找到第一个 `active==0` 的槽位。如果池耗尽返回 NULL，调用处检查后静默跳过——不会崩溃也不会内存泄漏。

---

## 五、碰撞检测 (AABB)

```c
u8 CheckHit(x1,y1,w1,h1, x2,y2,w2,h2) {
    hw = (w1+w2)/2 + 2;   // 半宽之和 + 2px 宽容
    hh = (h1+h2)/2 + 2;   // 半高之和 + 2px 宽容
    dx = |x1 - x2|;
    dy = |y1 - y2|;
    return (dx < hw && dy < hh);
}
```

+2 像素的宽容量让手感不那么苛刻——视觉上擦边的子弹也能命中。

### 碰撞处理流程

| 检测对 | 结果 |
|--------|------|
| 玩家子弹 vs 敌人 | 敌人 hp--，hp=0 则死亡 + 掉落道具 + 加分 |
| 敌方子弹 vs 玩家 | 有炸弹→自动消耗炸弹+无敌；无炸弹→扣 1 命+无敌 |
| 敌人机身 vs 玩家 | 同上一行，小兵撞到即消灭 |
| 玩家 vs 道具 | 升级 PWR/SPD/BOMB，生命+1 |

### 无敌帧

被击中后 `inv_frames = 90`（约 1.5 秒），期间所有伤害检测跳过。绘制时闪烁：`(inv_frames/5)%2 == 0` 画飞机，奇数帧不画，产生闪烁效果。

---

## 六、HUD 增量更新

顶部 32 行显示分数、生命、子弹威力、射速、炸弹数。为了不闪烁，**只有数值变化时才重画对应区域**：

```c
void DrawHUD(void) {
    if (hud_score == 0xFFFFFFFF) {  // 首次：画背景+分隔线（只画一次）
        LCD_Fill(0,0,479,31, BLACK);
        LCD_Fill(0,30,479,31, GRAY);
    }
    if (hud_score != player.score) { 重画 "SC:xxxxx" }
    if (hud_lives != player.lives) { 重画 "HP:x" }
    if (hud_pwr   != player.bullet_lv) { 重画 "PWR:x" }
    if (hud_spd   != player.fire_rate) { 重画 "SPD:x" }
    if (hud_bomb  != player.bombs)    { 重画 "BOMB:x" }
}
```

**效果**: 背景和分隔线只在游戏开始时画一次。分数变化最频繁（每杀一个敌人+100），也只重画 5 个字符的极小区域。其他标签基本不动。

---

## 七、按键驱动 (key.c)

| 宏 | GPIO | 极性 | 方向 |
|----|------|------|------|
| `KEY_UP` | PA0 | 按下高 (3.3V) | 上 |
| `KEY0` | PE2 | 按下低 (GND) | 左 |
| `KEY1` | PE3 | 按下低 (GND) | 下 |
| `KEY2` | PE4 | 按下低 (GND) | 右 |

`KEY_Scan(0)`: 单次触发——按下只返回一次，松开才允许再次触发。用于菜单。

`KEY_Scan(1)`: 连续触发——每帧都返回当前按下的键值。用于游戏中移动（按住方向键持续移动）。

消抖：检测到按键后 `delay_ms(10)` 再读一次确认。

---

## 八、外部 SRAM 初始化 (sram.c)

IS62WV51216 挂在 FSMC Bank1 NE3 (`0x68000000`)，1MB，16 位数据总线。

**与 LCD 共享的引脚**: 数据线 D0-D15 也是 LCD 的数据线。FSMC 通过不同的 NE 片选区分：LCD 用 NE4，SRAM 用 NE3。

**GPIO 配置**: 19 根地址线 (A0-A18) 分布在 PF0-PF5、PF13-PF15、PG0-PG5、PD11-PD13。由于很多引脚和 LCD 的 FSMC 配置重叠，实际只需要配置 LCD 没配的那些。

**时序**: ADDSET=2, DATAST=2（SRAM 比 LCD 快得多），AccessMode A。

---

## 九、Boss 机制

### 触发

```c
boss_threshold = 3000;  // 第一个 Boss 在 3000 分出现

if (!boss_spawned && player.score >= boss_threshold)
    SpawnBoss();
```

### Boss 击杀后

```c
if (boss_defeated) {
    boss_spawned = 0;
    boss_defeated = 0;
    boss_threshold += 7000;  // 下一个 Boss: 3000→10000→17000→...
}
```

不结束游戏。只要命没死光，游戏无限继续。Boss 间隔 7000 分递增。

### Boss AI

- 出场位置: Y = GAME_AREA_TOP + 80（血条上方留空给 HUD）
- 左右振荡: `x = base_x + BOSS_SPEED × 三角波`
- 扇形射击: 每 18 帧射出 3 发子弹（正中 + 左偏移 + 右偏移）
- HP = 30，上方显示绿色/灰色血条（宽 BOSS_W，高 6px）
- 炸弹对其造成 15 点伤害

---

## 十、敌兵生成（自适应难度）

```c
if (spawn_timer == 0) {
    SpawnOneEnemy();
    if (score < 500)  spawn_timer = 25;   // 每 25 帧一个
    else if (<1500)   spawn_timer = 18;   // 每 18 帧
    else              spawn_timer = 12;   // 每 12 帧
}
```

Boss 存活期间暂停出兵（`if (!boss_spawned)`）。

三种移动模式随机分配：直线下落 (PAT_STRAIGHT)、蛇形左右摆 (PAT_ZIGZAG)、斜向下落 (PAT_DIAGONAL)。

---

## 十一、道具系统

| 枚举值 | 图标 | 拾取效果 | 上限 |
|--------|------|----------|------|
| `ITEM_LIFE` | 红色十字 (22×22) | lives+1 | 5 |
| `ITEM_BULLET_UP` | 黄底+"B"字 | bullet_lv+1 | 3 |
| `ITEM_FIRE_RATE` | 绿底+"S"字 | fire_rate+1 | 3 |
| `ITEM_BOMB` | 灰底+白点 | bombs+1 | 5 |

掉落率 35%（`Rand()%100 >= 35` 时跳过）。每只被击杀的小兵独立判定。

### 子弹等级

- Lv1: 1 发 (正上方)
- Lv2: 2 发 (左右各偏 6px)
- Lv3: 3 发 (正中 + 左右各偏 8px)

### 射速等级

- SPD=1: 每 15 帧射一次 (FIRE_SLOW)
- SPD=2: 每 8 帧 (FIRE_MEDIUM)
- SPD=3: 每 4 帧 (FIRE_FAST) → 全速约 30 发/秒 × 3 = 90 颗子弹

---

## 十二、炸弹机制

炸弹是**自动保命**道具，不是主动技能。

当玩家被击中且有炸弹时：
1. `player.bombs--`
2. 清除所有小兵 (active=0)
3. Boss 扣 15 HP
4. 清除所有敌方子弹
5. 玩家进入 90 帧无敌

如果没炸弹，直接扣 1 命。

---

## 十三、绘制函数 — 全部用几何图形

没有精灵/图片引擎，所有角色用 `LCD_Fill` 矩形拼接：

| 角色 | 画法 | 颜色 |
|------|------|------|
| 玩家飞机 | 三角形机头 + 矩形机身 + 矩形机翼 + 白色座舱 + 黄色尾焰 | CYAN |
| 小兵 | 菱形身体 + 上下尖刺 + 两侧凸起 + 黄色眼睛 | RED |
| Boss | 双层矩形主体 + 两侧吊舱 + 驾驶舱 + 炮管 | BRED/RED/GRAY |
| 玩家子弹 | 3×12 细长矩形 | YELLOW |
| 敌方子弹 | 6×6 小方块 | RED |
| 流星 | 2×4 短竖线，5 种灰/蓝色调 | GRAY 等 |

---

## 十四、伪随机数

```c
static u32 rng_seed = 12345;
static u32 Rand(void) {
    rng_seed = rng_seed * 1103515245 + 12345;
    return (rng_seed >> 16) & 0x7FFF;
}
```

线性同余生成器 (LCG)。不用 stdlib 的 `rand()`，省库空间且可控。`RandSeed(54321)` 在 `Game_Start()` 重置种子。

---

## 十五、常量调参表

所有可调参数集中在 `game.h`：

| 参数 | 值 | 作用 |
|------|-----|------|
| `PLAYER_SPEED` | 6 | 飞机移动速度 px/f |
| `PLAYER_INIT_LIVES` | 3 | 初始生命 |
| `FIRE_SLOW / MEDIUM / FAST` | 15/8/4 | 射击冷却帧数 |
| `BOSS_MAX_HP` | 30 | Boss 生命 |
| `SCORE_BOSS_THRESHOLD` | 3000 | 首个 Boss 出现分数 |
| `SCORE_BOSS_STEP` | 7000 | Boss 间分数间隔 |
| `BOSS_FIRE_INTERVAL` | 18 | Boss 射击间隔 |
| `PBULLET_SPEED` | 14 | 玩家子弹速度 |
| `EBULLET_SPEED` | 5 | 敌方子弹速度 |
| `ENEMY_SPEED_Y` | 3 | 敌兵下落速度 |
| `ITEM_W / ITEM_H` | 22/22 | 道具碰撞体积 |
| `INVINCIBLE_FRAMES` | 90 | 无敌持续时间 |
| `MAX_PLAYER_BULLETS` | 64 | 玩家子弹池 |
| `MAX_ENEMY_BULLETS` | 50 | 敌方子弹池 |
| `MAX_ENEMIES` | 15 | 敌人池 |
| `MAX_ITEMS` | 8 | 道具池 |
| `MAX_STARS` | 80 | 星星池 |
| `GAME_AREA_TOP` | 32 | 游戏区顶部 Y（HUD 高度） |

---

## 十六、C89 兼容

Keil MDK 的 C 编译器要求 C89 标准：所有变量声明必须在块开头，不能混合声明和语句。代码中所有 `s16 hw, hh;` 都放在函数/块最前面。

---

## 十七、已知限制

- 无音效（STM32F407 DAC 可用，未接入）
- 无高分存档
- 无中文字库
- 道具上的字 "B" "S" 用的是 LCD_ShowChar（直接写 LCD），和缓冲区的绘制逻辑不协调——在某些帧可能被 flush 覆盖
- 帧率瓶颈在 `buf_Flush`（~26ms），如果要回到 60fps，需要改为只搬运脏矩形
