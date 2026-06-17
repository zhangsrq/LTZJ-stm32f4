# 雷霆战机 (LeiTingZhanJi) — STM32F407 游戏项目代码说明

## 项目概述

- **平台**: 正点原子探索者 STM32F407ZG 开发板
- **显示**: NT35510 4.3寸 TFT LCD，480×800 分辨率，FSMC 16位并口
- **输入**: KEY0(PE2,左)、KEY1(PE3,下)、KEY2(PE4,右)、KEY_UP(PA0,上)
- **开发环境**: Keil MDK-ARM (uVision 5)，标准外设库
- **帧率**: ~60 FPS

## 文件结构

```
雷霆战机/
├── USER/
│   ├── main.c              ← 主程序入口 + 游戏主循环
│   ├── ltzj.uvprojx        ← Keil 工程文件
│   ├── stm32f4xx_it.c      ← 中断服务函数
│   └── system_stm32f4xx.c  ← 系统时钟初始化 (168MHz)
├── HARDWARE/
│   ├── KEY/
│   │   ├── key.h           ← 按键宏定义 (KEY_UP_PRES 等)
│   │   └── key.c           ← 按键消抖扫描
│   ├── GAME/
│   │   ├── game.h          ← 所有常量/数据结构/API 声明
│   │   └── game.c          ← 游戏核心逻辑 (~700行)
│   ├── LCD/                ← NT35510 驱动 (源自正点原子)
│   └── LED/                ← 板载 LED (用作心跳指示)
├── SYSTEM/                 ← 系统组件 (delay, sys, usart)
├── CORE/                   ← CMSIS 核心 + 启动文件
└── FWLIB/                  ← STM32F4 标准外设库
```

---

## 一、主循环架构 (main.c)

```c
int main(void) {
    初始化系统 → NVIC优先级 → delay_init → uart → LED → LCD → KEY
    Game_Init();              // 创建对象池、初始化星星
    Game_DrawStartPage();     // 显示开始画面

    while(1) {
        key = KEY_Scan(1);   // 连续模式扫描按键

        switch(Game_GetState()) {
            case STATE_START:    // 开始页面 → 按 KEY_UP → Game_Start()
            case STATE_PLAYING:  // 游戏中 → Game_Update(key) → Game_Render()
            case STATE_GAMEOVER: // 死亡 → 按 KEY_UP → 重新开始
            case STATE_WIN:      // 胜利 → 按 KEY_UP → 重新开始
        }

        delay_ms(16);  // 约 60 FPS
    }
}
```

**关键设计**: 整个游戏由 `Game_Update()` 处理逻辑（移动+碰撞），`Game_Render()` 处理绘制，两个函数完全分离。状态机负责页面切换。

---

## 二、按键驱动 (key.c)

### 引脚定义 (key.h)
```c
#define KEY_UP    PAin(0)   // 读取 PA0，按下为高（接 3.3V）
#define KEY0      PEin(2)   // 读取 PE2，按下为低（接 GND）
#define KEY1      PEin(3)   // PE3
#define KEY2      PEin(4)   // PE4
```

### 扫描策略
- `KEY_Scan(mode)` 做了 10ms 消抖
- `mode=0`: 只响应一次按下（用于菜单选择）
- `mode=1`: 按住连续触发（用于游戏中移动，**游戏中必须用 mode=1**）
- 返回值: `KEY_UP_PRES=1`, `KEY0_PRES=2`, `KEY1_PRES=3`, `KEY2_PRES=4`, 无按键=0

### KEY_UP 的特殊性
PA0 在 ALIENTEK 板子上通过硬件上拉到 3.3V，所以配置为**下拉输入**，按下时读到高电平。而 PE2/PE3/PE4 是按键下拉到 GND，配置为**上拉输入**，按下时读到低电平。这两种极性在 `KEY_Scan` 里用 `== 1` / `== 0` 区分。

---

## 三、游戏核心 (game.c/h) — 重点

### 3.1 对象池设计

传统做法（malloc/free）在嵌入式上不可靠。采用**固定大小数组 + active 标志**的对象池模式：

```c
Bullet pbullets[MAX_PLAYER_BULLETS];  // 64 个槽位
Bullet ebullets[MAX_ENEMY_BULLETS];   // 50 个槽位
Enemy  enemies[MAX_ENEMIES];          // 15 个槽位
Item   items[MAX_ITEMS];              // 8 个槽位
Star   stars[MAX_STARS];              // 80 个槽位
```

分配新对象: `GetFreePBullet()` 遍历数组找到第一个 `active == 0` 的槽位并返回指针。
释放对象: 直接设 `active = 0`。

**为什么不会越界？** 每个 `GetFree*()` 返回 0（NULL）时，调用处必须 `if (!b) return;` 检查——这就是子弹池耗尽时丢弹但不死机的关键。

### 3.2 数据结构

```c
typedef struct {
    s16 x, y;           // 坐标
    u8  lives;          // 生命 0~5
    u8  bullet_lv;      // 子弹等级 1/2/3 → 单发/双发/三发
    u8  fire_rate;      // 射速等级 1/2/3 → 慢/中/快
    u8  bombs;          // 炸弹数量 0~5
    u16 fire_cd;        // 射击冷却计数器（帧数）
    u16 inv_frames;     // 剩余无敌帧数（0=不无敌）
    u32 score;          // 总分
} Player;
```

子弹等级升级逻辑:
- Lv1: 正上方 1 发
- Lv2: 左右各 1 发 (x-6, x+6)
- Lv3: 正中 1 发 + 左右 2 发 (x, x-8, x+8)

射速等级映射:
- SPD=1: 每 15 帧射一次 (`FIRE_SLOW`)
- SPD=2: 每 8 帧 (`FIRE_MEDIUM`)
- SPD=3: 每 4 帧 (`FIRE_FAST`) — 60fps 下每秒 15 发 × 3 = 45 颗子弹

### 3.3 渲染策略 — 擦旧画新

解决屏幕闪烁的关键设计: **每帧先擦旧再画新，而非整屏重填**。

```
Game_Update():
  1. 星星: EraseStar() → 移动
  2. 玩家: ErasePlayer() → 移动
  3. 子弹: EraseBullet() → 移动 (逐个处理)
  4. 敌人: EraseSmallEnemy()/EraseBoss() → 移动 (逐个处理)
  5. 道具: EraseItem() → 移动 (逐个处理)
  6. 四组碰撞检测 (见 3.5)
  7. 敌人生成 + Boss 生成 (见 3.6)

Game_Render():
  1. DrawStars()     ← 背景层（先画）
  2. DrawItem() × N  ← 道具层
  3. DrawBullet() × N ← 子弹层
  4. DrawEnemy() × N  ← 敌人层
  5. DrawPlane()      ← 玩家层（最上面）
  6. DrawHUD()        ← UI 层
```

**为什么这样不闪？** 全程没有清屏操作。移动 → 擦旧 → 画新，画面始终是"实心的"。只有背景星星靠 `UpdateStars` 里的 erase 维护（星星密度低，擦不产生视觉闪烁）。

*注: 如果将来仍然有闪烁，可以改为双缓冲——但 STM32F407 显存不够 480×800×2 = 768KB。*

### 3.4 碰撞检测 (AABB)

```c
u8 CheckHit(x1,y1,w1,h1, x2,y2,w2,h2) {
    hw = (w1 + w2)/2 + 2;   // 半宽之和 + 2像素宽容
    hh = (h1 + h2)/2 + 2;   // 半高之和 + 2像素宽容
    dx = |x1 - x2|;          // 中心点 x 差
    dy = |y1 - y2|;          // 中心点 y 差
    return (dx < hw && dy < hh);
}
```

+2 的宽容量让碰撞手感"宽松"一点，避免视觉上擦边不判定的问题。

### 3.5 碰撞处理流程

一共四组检测，顺序不影响（每帧独立）：

| 检测 | 动作 |
|------|------|
| 玩家子弹 vs 敌人 | 敌人 hp--，hp=0 则死亡 + 掉落道具 + 加分 |
| 敌方子弹 vs 玩家 | 有炸弹→自动炸+无敌，无炸弹→扣命+无敌 |
| 敌人 vs 玩家（碰机身） | 同上，小兵同时被消灭 |
| 玩家 vs 道具 | PWR/SPD/BOMB 升级（上限3/3/5），生命+1（上限5） |

**无敌帧设计**: 碰撞后 `inv_frames = 90`（~1.5秒），期间所有伤害判定的代码都被 `if (inv_frames == 0)` 跳过。绘制时通过 `inv_frames/5 % 2` 闪烁实现视觉反馈。

### 3.6 Boss 生成机制

```c
boss_threshold = 3000  // 第一个 Boss 门槛
if (player.score >= boss_threshold && !boss_spawned)
    SpawnBoss();

// Boss 被击杀后:
boss_defeated → 标记触发
boss_threshold += 7000  // 下一个在 10000 分出
boss_spawned = 0        // 允许下次再生成
```

消灭 Boss 不结束游戏。无限模式，命用完才 Game Over。

### 3.7 敌人生成 — 自适应难度

```c
if (spawn_timer == 0) {
    SpawnOneEnemy();
    // 分数越高，出兵越快
    if (player.score < 500)      spawn_timer = 25帧;
    else if (player.score < 1500) spawn_timer = 18帧;
    else                          spawn_timer = 12帧;  // 0.2秒一个
}
```

出兵在 Boss 存活期间暂停 (`if (!boss_spawned)`)，保证 Boss 战中不刷杂兵。

### 3.8 道具系统

| 道具 | 图标 | 效果 | 最大值 |
|------|------|------|--------|
| 红十字 | 红色十字 (ITEM_LIFE) | lives+1 | 5 |
| B | 黄色方块+B字 (ITEM_BULLET_UP) | bullet_lv+1 | 3 |
| S | 绿色方块+S字 (ITEM_FIRE_RATE) | fire_rate+1 | 3 |
| 炸弹 | 灰色方块+白点 (ITEM_BOMB) | bombs+1 | 5 |

掉落率 35%（`Rand()%100 >= 35` 时跳过），每只被击杀的小兵独立随机判定。

### 3.9 炸弹机制

- 获得炸弹时：`bombs` 加 1（上限 5）
- 碰撞触发时：如果 `bombs > 0`，自动消耗 1 颗炸弹：
  1. 清除全部小兵（设为 inactive）
  2. Boss 扣 15 HP
  3. 清除全部敌方子弹
  4. 玩家进入无敌帧

这设计让炸弹是"保命道具"而非主动技能——撞到时自动用掉。

### 3.10 伪随机数 (LCG)

```c
static u32 rng_seed = 12345;
static u32 Rand(void) {
    rng_seed = rng_seed * 1103515245 + 12345;
    return (rng_seed >> 16) & 0x7FFF;
}
```

线性同余生成器，不使用 `stdlib.h` 的 `rand()`（太慢，且占库空间）。种子在 `Game_Start()` 中设为 54321，确保每次游戏敌人位置/道具掉落有变化。

### 3.11 C89 兼容性

代码遵循 C89 标准——所有变量声明在块开头，禁用在 `if`/`return` 之后声明。这是 Keil MDK 的 C 编译器限制。

---

## 四、常量调参指南

所有可调参数集中在 `game.h`，修改后无需动逻辑代码：

| 参数 | 当前值 | 作用 |
|------|--------|------|
| `PLAYER_SPEED` | 6 | 飞机移动速度（像素/帧） |
| `PLAYER_INIT_LIVES` | 3 | 初始生命 |
| `FIRE_SLOW/MEDIUM/FAST` | 15/8/4 | 射击冷却（帧数） |
| `BOSS_MAX_HP` | 30 | Boss 生命值 |
| `SCORE_BOSS_THRESHOLD` | 3000 | 第一个 Boss 出现分数 |
| `SCORE_BOSS_STEP` | 7000 | 后续 Boss 间隔分数 |
| `BOSS_FIRE_INTERVAL` | 18 | Boss 射击间隔（帧） |
| `PBULLET_SPEED` | 14 | 玩家子弹速度 |
| `ENEMY_SPEED_Y` | 3 | 敌兵下落速度 |
| `ITEM_W/ITEM_H` | 22/22 | 道具碰撞体积 |
| `INVINCIBLE_FRAMES` | 90 | 无敌持续时间 |
| `MAX_PLAYER_BULLETS` | 64 | 子弹池大小 |

**帧率调参**: 修改 `main.c` 中 `delay_ms(16)` 的值。加大=慢帧率，减小=快帧率。调整帧率后需要同步调整射击冷却值（`FIRE_*`）来保持手感一致。

---

## 五、已知限制与改进方向

1. **无音效**: STM32F407 有 DAC，可以加 PWM 驱动的蜂鸣器。扩展点在 `main.c` 主循环中插入蜂鸣器触发。
2. **无 EEPROM 存档**: 高分不保存。可以用 STM32 内部 Flash 写一个小的高分存档（注意 Flash 写入寿命）。
3. **无中文字库**: 标题只用了英文。可以加载正点原子的汉字库显示中文标题。
4. **单线程**: 没有用 RTOS，但 `SYSTEM_SUPPORT_OS` 宏已经预留了 uC/OS 接口。
5. **内存使用**: 全局数组 ≈ 11KB（主要是子弹池 64×8 + 50×8），对小容量的 STM32F4 完全安全。
