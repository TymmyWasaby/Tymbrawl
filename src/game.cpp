/* ================================================================
 * TYMBRAWL — Brawl Stars-like pour NumWorks (app externe EADK .nwa)
 * Moteur graphique : sprites pixel art pre-rendus (buffers RGB565
 * pousses via eadk_display_push_rect), tuiles texturees, sol en
 * damier, ombres, particules, degats flottants, zone animee.
 * 10 brawlers, power 1-11, points de power, trophees, Showdown 1+9
 * bots, zone qui retrecit, caisses, cubes, super charge.
 * ================================================================ */
#include <eadk.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

/* ---------------- Ecran / monde ---------------- */
#define SCREEN_W 320
#define SCREEN_H 240
#define HUD_H    16
#define VIEW_H   (SCREEN_H - HUD_H)

#define TILE      8
#define MAP_W     48
#define MAP_H     40
#define WORLD_W   (MAP_W * TILE)
#define WORLD_H   (MAP_H * TILE)

#define N_ACTORS  10
#define N_BULLETS 96
#define N_CUBES   48
#define N_PART    128
#define N_FLOAT   16

#define BULLET_LIFE 60

/* Zone */
#define ZONE_GRACE    750
#define ZONE_TICK     5
#define ZONE_SHRINK   1
#define ZONE_MIN      56
#define ZONE_DMG_TICK 30
#define ZONE_DMG      2

/* Tuiles */
#define T_GRASS 0
#define T_WALL  1
#define T_BUSH  2
#define T_CRATE 3

/* Etats */
#define S_TITLE  0
#define S_SELECT 1
#define S_GAME   2
#define S_END    3
#define S_MODE   4

/* Modes de jeu */
#define M_SHOWDOWN 0
#define M_GEMS     1
#define M_HOTZONE  2
#define M_SURVIE   3
#define N_MODES    4
#define N_GEMS     24
#define GEM_GOAL   10
#define GEM_TIMER  150
#define HZ_GOAL    100

/* Couleurs RGB565 */
#define RGB(r,g,b) ((uint16_t)((((r)&0xF8)<<8)|(((g)&0xFC)<<3)|(((b)&0xF8)>>3)))
static const uint16_t C_BG     = RGB(16,14,28);
static const uint16_t C_GRASS  = RGB(46,112,46);
static const uint16_t C_GRASS2 = RGB(40,100,40);
static const uint16_t C_GRASS3 = RGB(52,122,52);
static const uint16_t C_WALL   = RGB(96,96,110);
static const uint16_t C_WALLL  = RGB(142,142,160);
static const uint16_t C_WALLD  = RGB(54,54,66);
static const uint16_t C_BUSH   = RGB(22,66,22);
static const uint16_t C_BUSHL  = RGB(38,100,38);
static const uint16_t C_BUSHD  = RGB(14,44,14);
static const uint16_t C_CRATE  = RGB(146,98,38);
static const uint16_t C_CRATEL = RGB(198,152,82);
static const uint16_t C_CRATED = RGB(94,58,18);
static const uint16_t C_BULLET = RGB(255,240,180);
static const uint16_t C_CUBE   = RGB(255,208,40);
static const uint16_t C_ZONE   = RGB(235,70,70);
static const uint16_t C_HUD    = RGB(12,10,20);
static const uint16_t C_WHITE  = RGB(255,255,255);
static const uint16_t C_GREY   = RGB(160,160,170);
static const uint16_t C_HPBG   = RGB(110,18,18);
static const uint16_t C_HPGREEN= RGB(70,225,70);
static const uint16_t C_OUT    = RGB(18,16,24);
static const uint16_t C_SHADOW = RGB(30,58,30);
static const uint16_t C_GOLD   = RGB(255,204,40);
static const uint16_t C_PURPLE = RGB(150,80,240);
static const uint16_t C_SUPER  = RGB(255,230,80);

/* ================================================================ */
/* Brawlers                                                          */
/* ================================================================ */

struct BrawlerDef {
  const char * name;
  uint16_t color;
  int hp, dmg, range, cd, speed, bspd, pellets;
  const char * desc;
};

static const BrawlerDef BRAWLERS[10] = {
  { "TYMBO",   RGB(60,200,255), 100, 12, 110, 20, 2, 6, 1, "EQUILIBRE"     },
  { "RAFALE",  RGB(240,120,50), 110,  6,  62, 30, 2, 6, 4, "FUSIL A POMPE" },
  { "LONGIS",  RGB(120,90,240),  70, 30, 180, 55, 2, 9, 1, "SNIPER"        },
  { "BLINZY",  RGB(255,220,60),  80,  5,  82,  8, 2, 8, 1, "MITRAILLEUSE"  },
  { "MAGNUS",  RGB(200,60,60),  180, 15,  48, 25, 2, 5, 1, "TANK"          },
  { "VOLT",    RGB(90,240,220),  70,  8,  72, 14, 3, 7, 1, "RAPIDE"        },
  { "SPYRO",   RGB(90,220,90),   75, 16, 140, 38, 2, 5, 1, "LANCEUR"       },
  { "NEKO",    RGB(250,130,180), 85, 20,  42, 18, 3, 6, 1, "ASSASSIN"      },
  { "BASTION", RGB(150,150,170),150, 10,  92, 26, 1, 5, 2, "LOURD"         },
  { "PIXEL",   RGB(230,90,240),  75, 14, 130, 24, 2, 6, 1, "MAGE"          },
};

#define MAX_POWER 11
#define PP_COST(l) (10 + 10 * (l))

/* ---------------- Etat meta ---------------- */
static int g_brawler;
static int g_power[10];
static int g_pp;
static int g_troph[10];
static int g_state = S_TITLE;
static int g_sel = 0;
static int g_lastRank = 0, g_lastCubes = 0, g_lastTroph = 0, g_lastPP = 0;

/* ---------------- Entites ---------------- */
struct Actor {
  int x, y;
  int hp, maxhp;
  int cubes, super;
  int cd, flashT;
  int gems;
  bool alive, bot;
  int brawler;
  int fx, fy;
  int wx, wy;
  int anim;
  int pvx, pvy;   /* vitesse reelle du dernier deplacement (anticipation IA) */
};
struct Bullet {
  int x, y;
  float dx, dy;
  int owner, life, dmg;
  bool super, alive;
};
struct Cube { int x, y; bool alive; };
struct Part { int x, y, vx, vy, life; uint16_t c; bool alive; };
struct Floater { int x, y, life, val; bool heal; bool alive; };

static uint8_t  g_map[MAP_W * MAP_H];
static uint8_t  g_deco[MAP_W * MAP_H];
static uint8_t  g_crateHp[MAP_W * MAP_H];
static Actor    g_actors[N_ACTORS];
static Bullet   g_bullets[N_BULLETS];
static Cube     g_cubes[N_CUBES];
static Part     g_parts[N_PART];
static Floater  g_floats[N_FLOAT];
static int      g_cx, g_cy;
static int      g_zx, g_zy, g_zw, g_zh;
static int      g_frame, g_rank;
static bool     g_over, g_victory;
static int      g_shake;
static uint16_t g_flash;

/* Modes */
static int g_mode;
struct Gem { int x, y; bool alive; };
static Gem g_gemArr[N_GEMS];
static int g_gemTimer, g_gemCd;
static int g_hzx, g_hzy, g_hzw, g_hzh, g_hzone;
static int g_waveT, g_waveN;
/* Niveau des bots : 0 = noob ... 10 = pro */
static int g_aiDiff = 1;

/* ================================================================ */
/* Moteur de rendu : buffers pre-rendus + blit                       */
/* ================================================================ */

static uint16_t g_sprites[10][24 * 24];   /* sprites brawlers 24x24 */
static uint16_t g_portraits[10][48 * 48]; /* portraits x2 pour menus */
static uint16_t g_tileWall[64], g_tileBush[64], g_tileCrate[64];
static uint16_t g_ground[64 * 64];        /* motif de sol 64x64 */

static inline void pxb(uint16_t * buf, int w, int h, int x, int y, uint16_t c) {
  if (x < 0 || y < 0 || x >= w || y >= h) return;
  buf[y * w + x] = c;
}
static inline void rectb(uint16_t * buf, int w, int h, int x, int y, int rw, int rh, uint16_t c) {
  for (int j = 0; j < rh; j++)
    for (int i = 0; i < rw; i++)
      pxb(buf, w, h, x + i, y + j, c);
}

/* Blit avec clipping. flip = miroir horizontal. Un seul pushRect si
 * le sprite est entierement visible (cas courant), sinon ligne a ligne. */
static uint16_t g_row[SCREEN_W];
static void blit(const uint16_t * src, int w, int h, int dx, int dy, bool flip) {
  int x0 = dx < 0 ? -dx : 0;
  int y0 = dy < 0 ? -dy : 0;
  int x1 = dx + w > SCREEN_W ? SCREEN_W - dx : w;
  int y1 = dy + h > SCREEN_H ? SCREEN_H - dy : h;
  if (x0 >= x1 || y0 >= y1) return;
  if (x0 == 0 && y0 == 0 && x1 == w && y1 == h && !flip) {
    eadk_rect_t r = { (uint16_t)dx, (uint16_t)dy, (uint16_t)w, (uint16_t)h };
    eadk_display_push_rect(r, src);
    return;
  }
  for (int y = y0; y < y1; y++) {
    const uint16_t * srow = src + y * w;
    for (int x = x0; x < x1; x++)
      g_row[x - x0] = flip ? srow[w - 1 - x] : srow[x];
    eadk_rect_t r = { (uint16_t)(dx + x0), (uint16_t)(dy + y), (uint16_t)(x1 - x0), 1 };
    eadk_display_push_rect(r, g_row);
  }
}

static inline void fillRect(int x, int y, int w, int h, uint16_t c) {
  if (x < 0) { w += x; x = 0; }
  if (y < 0) { h += y; y = 0; }
  if (x + w > SCREEN_W) w = SCREEN_W - x;
  if (y + h > SCREEN_H) h = SCREEN_H - y;
  if (w <= 0 || h <= 0) return;
  eadk_rect_t r = { (uint16_t)x, (uint16_t)y, (uint16_t)w, (uint16_t)h };
  eadk_display_push_rect_uniform(r, (eadk_color_t)c);
}

static inline void drawText(const char * t, int x, int y, uint16_t fg, uint16_t bg, bool large) {
  eadk_point_t p = { (uint16_t)x, (uint16_t)y };
  eadk_display_draw_string(t, p, large, (eadk_color_t)fg, (eadk_color_t)bg);
}
static void drawTextC(const char * t, int cy, uint16_t fg, uint16_t bg, bool large) {
  int w = large ? 15 : 10;
  int len = 0; while (t[len]) len++;
  drawText(t, SCREEN_W / 2 - len * w / 2, cy, fg, bg, large);
}
static inline bool keyDown(uint64_t s, eadk_key_t k) { return eadk_keyboard_key_down((eadk_keyboard_state_t)s, k); }
static inline uint32_t rnd(uint32_t n) { return n ? eadk_random() % n : 0; }

/* ================================================================ */
/* Generation des graphismes (au demarrage)                          */
/* ================================================================ */

/* Sprite 24x24 d'un brawler, style Brawl Stars : peau, cheveux, chapeau
   et arme uniques par personnage. Palette derivee de sa couleur. */
static inline void spx(uint16_t * s, int x, int y, uint16_t c) { s[y * 24 + x] = c; }

/* Look par brawler : peau, cheveux, chapeau, arme */
struct Look { uint16_t skin, hair; int hat, weapon; };
static const Look LOOKS[10] = {
  { RGB(255,220,180), RGB(90,60,40),   1, 0 },
  { RGB(240,200,160), RGB(60,40,30),   2, 1 },
  { RGB(255,230,200), RGB(35,30,45),   3, 2 },
  { RGB(255,225,190), RGB(255,240,150),4, 3 },
  { RGB(230,180,140), RGB(40,30,25),   5, 4 },
  { RGB(255,215,175), RGB(60,240,210), 6, 5 },
  { RGB(250,210,170), RGB(200,60,50),  0, 6 },
  { RGB(255,225,195), RGB(120,70,40),  8, 7 },
  { RGB(220,180,150), RGB(40,30,25),   5, 8 },
  { RGB(255,230,200), RGB(70,35,95),   7, 9 },
};

static void buildSprite(int bi) {
  uint16_t * s = g_sprites[bi];
  const BrawlerDef & d = BRAWLERS[bi];
  const Look & L = LOOKS[bi];
  uint16_t c = d.color;
  int cr = ((c >> 11) & 31), cg = ((c >> 5) & 63), cb = (c & 31);
  int dr = cr / 2, dg = cg / 2, db = cb / 2;
  int lr = cr * 3 / 2 > 31 ? 31 : cr * 3 / 2;
  int lg = cg * 3 / 2 > 63 ? 63 : cg * 3 / 2;
  int lb = cb * 3 / 2 > 31 ? 31 : cb * 3 / 2;
  uint16_t cd = (uint16_t)((dr << 11) | (dg << 5) | db);
  uint16_t cl = (uint16_t)((lr << 11) | (lg << 5) | lb);
  uint16_t skin = L.skin, hair = L.hair;
  int sr = ((skin >> 11) & 31), sg = ((skin >> 5) & 63), sb = (skin & 31);
  uint16_t sd = (uint16_t)(((sr * 3 / 4) << 11) | ((sg * 3 / 4) << 5) | (sb * 3 / 4));
  memset(s, 0, 24 * 24 * 2);

  /* ---- ombre au sol ---- */
  rectb(s, 24, 24, 6, 21, 12, 2, C_SHADOW);
  rectb(s, 24, 24, 8, 23, 8, 1, C_SHADOW);

  /* ---- jambes + bottes ---- */
  rectb(s, 24, 24, 7, 16, 4, 6, C_OUT);
  rectb(s, 24, 24, 13, 16, 4, 6, C_OUT);
  rectb(s, 24, 24, 8, 17, 2, 4, cd);
  rectb(s, 24, 24, 14, 17, 2, 4, cd);
  rectb(s, 24, 24, 7, 20, 4, 2, RGB(70,50,35));
  rectb(s, 24, 24, 13, 20, 4, 2, RGB(70,50,35));
  rectb(s, 24, 24, 7, 20, 4, 1, RGB(110,80,55));
  rectb(s, 24, 24, 7, 22, 4, 1, RGB(40,28,18));   /* semelle */
  rectb(s, 24, 24, 13, 22, 4, 1, RGB(40,28,18));
  spx(s, 8, 21, RGB(180,180,190)); spx(s, 14, 21, RGB(180,180,190)); /* lacets */

  /* ---- torse ---- */
  rectb(s, 24, 24, 6, 9, 12, 8, C_OUT);
  rectb(s, 24, 24, 7, 10, 10, 6, c);
  rectb(s, 24, 24, 7, 10, 10, 1, cl);
  rectb(s, 24, 24, 7, 15, 10, 1, cd);
  /* modelé lateral + plis */
  rectb(s, 24, 24, 7, 11, 1, 4, cl);
  rectb(s, 24, 24, 16, 11, 1, 4, cd);
  rectb(s, 24, 24, 9, 11, 1, 4, c);
  rectb(s, 24, 24, 14, 11, 1, 4, cd);
  /* epaulettes */
  rectb(s, 24, 24, 6, 9, 3, 2, cd);
  rectb(s, 24, 24, 15, 9, 3, 2, cd);
  rectb(s, 24, 24, 6, 9, 3, 1, cl);
  rectb(s, 24, 24, 15, 9, 3, 1, c);
  /* col */
  rectb(s, 24, 24, 10, 10, 4, 1, cl);
  /* ceinture + boucle */
  rectb(s, 24, 24, 7, 14, 10, 1, RGB(70,50,35));
  rectb(s, 24, 24, 11, 14, 2, 1, C_GOLD);
  spx(s, 11, 13, C_GOLD);
  /* embleme losange */
  spx(s, 11, 11, cl); spx(s, 12, 11, cl);
  spx(s, 12, 12, C_WHITE); spx(s, 11, 12, cd);

  /* ---- bras + mains ---- */
  rectb(s, 24, 24, 4, 11, 3, 5, C_OUT);
  rectb(s, 24, 24, 17, 11, 3, 5, C_OUT);
  rectb(s, 24, 24, 5, 12, 1, 3, c);
  rectb(s, 24, 24, 18, 12, 1, 3, c);
  rectb(s, 24, 24, 4, 15, 3, 2, skin);
  rectb(s, 24, 24, 17, 15, 3, 2, skin);

  /* ---- tete (grosse, style BS) ---- */
  rectb(s, 24, 24, 5, 0, 14, 11, C_OUT);
  rectb(s, 24, 24, 6, 1, 12, 9, skin);
  rectb(s, 24, 24, 6, 8, 12, 2, sd);           /* menton ombré */
  rectb(s, 24, 24, 6, 1, 12, 1, RGB(255,240,220)); /* reflet front */
  rectb(s, 24, 24, 6, 2, 1, 6, RGB(255,240,220));  /* modelé joue claire */
  rectb(s, 24, 24, 17, 2, 1, 6, sd);               /* modelé joue sombre */
  /* oreilles */
  rectb(s, 24, 24, 4, 5, 1, 2, skin);
  rectb(s, 24, 24, 19, 5, 1, 2, skin);
  spx(s, 4, 5, sd); spx(s, 19, 6, sd);
  /* nez */
  spx(s, 11, 6, sd); spx(s, 12, 6, sd);

  /* ---- cheveux (si visibles) ---- */
  if (bi != 4 && bi != 8) {
    rectb(s, 24, 24, 6, 1, 12, 2, hair);       /* frange */
    spx(s, 6, 3, hair); spx(s, 9, 3, hair); spx(s, 14, 3, hair); spx(s, 17, 3, hair);
    if (bi == 2 || bi == 7) {                  /* cheveux longs sur les cotes */
      rectb(s, 24, 24, 5, 3, 1, 6, hair);
      rectb(s, 24, 24, 18, 3, 1, 6, hair);
    }
  }

  /* ---- yeux ---- */
  rectb(s, 24, 24, 8, 4, 3, 3, C_WHITE);
  rectb(s, 24, 24, 13, 4, 3, 3, C_WHITE);
  rectb(s, 24, 24, 9, 5, 2, 2, C_OUT);
  rectb(s, 24, 24, 14, 5, 2, 2, C_OUT);
  spx(s, 9, 4, C_WHITE); spx(s, 14, 4, C_WHITE); /* reflet */
  spx(s, 10, 6, sd); spx(s, 15, 6, sd);          /* paupiere */
  /* sourcils */
  rectb(s, 24, 24, 8, 3, 3, 1, hair);
  rectb(s, 24, 24, 13, 3, 3, 1, hair);
  spx(s, 8, 2, cd); spx(s, 14, 2, cd);
  /* bouche souriante + coins */
  rectb(s, 24, 24, 10, 8, 4, 1, RGB(170,80,80));
  spx(s, 9, 7, RGB(170,80,80)); spx(s, 14, 7, RGB(170,80,80));
  spx(s, 10, 9, RGB(230,120,120));
  /* joues */
  spx(s, 7, 6, RGB(255,150,150)); spx(s, 16, 6, RGB(255,150,150));
  spx(s, 7, 7, RGB(255,130,130)); spx(s, 16, 7, RGB(255,130,130));

  /* ---- chapeaux / coiffures ---- */
  switch (L.hat) {
    case 1: /* casquette */
      rectb(s, 24, 24, 6, 0, 12, 3, c);
      rectb(s, 24, 24, 6, 0, 12, 1, cl);
      rectb(s, 24, 24, 4, 2, 8, 1, cd);        /* visiere */
      rectb(s, 24, 24, 11, 0, 2, 1, C_WHITE);
      break;
    case 2: /* chapeau de cowboy */
      rectb(s, 24, 24, 3, 3, 18, 2, RGB(120,80,40));
      rectb(s, 24, 24, 7, 0, 10, 4, RGB(140,95,50));
      rectb(s, 24, 24, 7, 2, 10, 1, c);        /* bande coloree */
      break;
    case 3: /* capuche de sniper */
      rectb(s, 24, 24, 5, 0, 14, 4, c);
      rectb(s, 24, 24, 4, 3, 2, 7, c);
      rectb(s, 24, 24, 18, 3, 2, 7, c);
      rectb(s, 24, 24, 5, 0, 14, 1, cl);
      break;
    case 4: /* casque audio */
      rectb(s, 24, 24, 6, 0, 12, 2, RGB(60,60,70));
      rectb(s, 24, 24, 3, 2, 3, 5, RGB(60,60,70));
      rectb(s, 24, 24, 18, 2, 3, 5, RGB(60,60,70));
      rectb(s, 24, 24, 3, 3, 1, 3, c);
      rectb(s, 24, 24, 20, 3, 1, 3, c);
      break;
    case 5: /* casque de combat */
      rectb(s, 24, 24, 5, 0, 14, 5, RGB(120,125,140));
      rectb(s, 24, 24, 5, 0, 14, 1, RGB(170,175,190));
      rectb(s, 24, 24, 6, 3, 12, 1, RGB(40,45,55));
      spx(s, 7, 1, RGB(170,175,190)); spx(s, 16, 1, RGB(170,175,190));
      break;
    case 6: /* iroquois */
      rectb(s, 24, 24, 10, 0, 4, 4, hair);
      spx(s, 9, 1, hair); spx(s, 14, 1, hair);
      spx(s, 10, 0, cl); spx(s, 11, 0, cl);
      break;
    case 7: /* chapeau de mage */
      rectb(s, 24, 24, 11, 0, 2, 1, c);
      rectb(s, 24, 24, 10, 1, 4, 1, c);
      rectb(s, 24, 24, 9, 2, 6, 1, c);
      rectb(s, 24, 24, 8, 3, 8, 1, c);
      rectb(s, 24, 24, 7, 4, 10, 1, c);
      rectb(s, 24, 24, 5, 5, 14, 1, cd);
      spx(s, 11, 2, C_GOLD); spx(s, 12, 3, C_GOLD);
      break;
    case 8: /* oreilles de chat */
      rectb(s, 24, 24, 6, 0, 3, 3, hair);
      rectb(s, 24, 24, 15, 0, 3, 3, hair);
      spx(s, 7, 1, RGB(255,170,190));
      spx(s, 16, 1, RGB(255,170,190));
      rectb(s, 24, 24, 6, 2, 12, 1, hair);     /* bandeau */
      break;
    default: break;
  }

  /* ---- arme tenue a droite ---- */
  switch (L.weapon) {
    case 0: /* pistolet */
      rectb(s, 24, 24, 20, 12, 3, 2, RGB(90,95,105));
      rectb(s, 24, 24, 20, 12, 3, 1, RGB(150,155,170));
      rectb(s, 24, 24, 20, 14, 2, 3, RGB(70,50,35));
      break;
    case 1: /* fusil a pompe */
      rectb(s, 24, 24, 18, 12, 5, 2, RGB(90,95,105));
      rectb(s, 24, 24, 18, 12, 5, 1, RGB(150,155,170));
      rectb(s, 24, 24, 17, 13, 3, 2, RGB(110,75,40));
      rectb(s, 24, 24, 22, 12, 1, 2, C_OUT);   /* bouche du canon */
      break;
    case 2: /* sniper */
      rectb(s, 24, 24, 18, 12, 5, 1, RGB(70,75,90));
      rectb(s, 24, 24, 19, 10, 3, 1, C_OUT);   /* lunette */
      spx(s, 20, 10, RGB(120,220,255));
      rectb(s, 24, 24, 18, 13, 2, 3, RGB(110,75,40));
      break;
    case 3: /* mitraillette */
      rectb(s, 24, 24, 19, 12, 4, 2, RGB(90,95,105));
      rectb(s, 24, 24, 19, 12, 4, 1, RGB(150,155,170));
      rectb(s, 24, 24, 20, 14, 2, 4, C_OUT);
      break;
    case 4: /* gantelet */
      rectb(s, 24, 24, 18, 12, 5, 5, cd);
      rectb(s, 24, 24, 18, 12, 5, 2, c);
      rectb(s, 24, 24, 18, 12, 5, 1, cl);
      spx(s, 19, 15, cl); spx(s, 21, 15, cl);
      break;
    case 5: /* dague */
      rectb(s, 24, 24, 20, 11, 1, 4, RGB(200,210,230));
      spx(s, 20, 11, C_WHITE);
      rectb(s, 24, 24, 19, 15, 3, 1, RGB(110,75,40));
      break;
    case 6: /* baton a orbe */
      rectb(s, 24, 24, 20, 10, 1, 8, RGB(110,75,40));
      rectb(s, 24, 24, 19, 8, 3, 3, cl);
      rectb(s, 24, 24, 19, 8, 3, 1, C_WHITE);
      spx(s, 20, 9, C_WHITE);
      break;
    case 7: /* griffes */
      rectb(s, 24, 24, 19, 12, 1, 4, skin);
      spx(s, 20, 11, RGB(230,235,245)); spx(s, 21, 12, RGB(230,235,245));
      spx(s, 20, 15, RGB(230,235,245));
      break;
    case 8: /* canon lourd */
      rectb(s, 24, 24, 18, 11, 5, 4, RGB(70,75,90));
      rectb(s, 24, 24, 18, 11, 5, 1, RGB(120,125,140));
      rectb(s, 24, 24, 18, 14, 5, 1, RGB(40,45,55));
      rectb(s, 24, 24, 22, 12, 1, 2, C_OUT);
      break;
    case 9: /* orbe magique */
      rectb(s, 24, 24, 19, 11, 4, 4, cl);
      rectb(s, 24, 24, 19, 11, 4, 1, C_WHITE);
      rectb(s, 24, 24, 20, 12, 2, 2, C_WHITE);
      spx(s, 22, 14, c);
      break;
  }
}

static void buildPortraits() {
  for (int bi = 0; bi < 10; bi++)
    for (int y = 0; y < 48; y++)
      for (int x = 0; x < 48; x++)
        g_portraits[bi][y * 48 + x] = g_sprites[bi][(y / 2) * 24 + (x / 2)];
}

static uint16_t noiseTile(uint16_t base, int amount) {
  int r = ((base >> 11) & 31) + (int)(rnd(2 * amount + 1) - amount);
  int g = ((base >> 5) & 63) + (int)(rnd(2 * amount + 1) - amount);
  int b = (base & 31) + (int)(rnd(2 * amount + 1) - amount);
  if (r < 0) r = 0;
  if (r > 31) r = 31;
  if (g < 0) g = 0;
  if (g > 63) g = 63;
  if (b < 0) b = 0;
  if (b > 31) b = 31;
  return (uint16_t)((r << 11) | (g << 5) | b);
}

static void buildTiles() {
  /* Mur : briques grises + bevel clair/sombre */
  for (int i = 0; i < 64; i++) g_tileWall[i] = noiseTile(C_WALL, 2);
  for (int i = 0; i < 8; i++) {
    g_tileWall[i] = C_WALLL;
    g_tileWall[8 + i] = C_WALLL;
    g_tileWall[56 + i] = C_WALLD;
    g_tileWall[48 + i] = C_WALLD;
    g_tileWall[i * 8] = C_WALLL;
    g_tileWall[i * 8 + 1] = C_WALLL;
    g_tileWall[i * 8 + 7] = C_WALLD;
    g_tileWall[i * 8 + 6] = C_WALLD;
  }
  /* joints de briques : 2 rangées décalées */
  for (int i = 0; i < 8; i++) {
    g_tileWall[3 * 8 + i] = C_WALLD;
    g_tileWall[7 * 8 + i] = C_WALLD;
  }
  g_tileWall[1 * 8 + 4] = C_WALLD; g_tileWall[2 * 8 + 4] = C_WALLD;
  g_tileWall[4 * 8 + 1] = C_WALLD; g_tileWall[5 * 8 + 1] = C_WALLD;
  g_tileWall[4 * 8 + 6] = C_WALLD; g_tileWall[5 * 8 + 6] = C_WALLD;
  /* Buisson : vert sombre bruite + feuilles denses */
  for (int i = 0; i < 64; i++) g_tileBush[i] = noiseTile(C_BUSH, 2);
  for (int k = 0; k < 16; k++) {
    int x = rnd(7), y = rnd(7);
    g_tileBush[y * 8 + x] = C_BUSHL;
    g_tileBush[y * 8 + x + 1] = C_BUSHD;
    g_tileBush[(y + 1) * 8 + x] = C_BUSHD;
  }
  for (int k = 0; k < 5; k++) {
    int x = rnd(6) + 1, y = rnd(6) + 1;
    g_tileBush[y * 8 + x] = C_BUSHL;
    g_tileBush[y * 8 + x + 1] = C_BUSHL;
  }
  /* Caisse : planches en diagonale + clous */
  for (int i = 0; i < 64; i++) g_tileCrate[i] = noiseTile(C_CRATE, 2);
  for (int i = 0; i < 8; i++) {
    g_tileCrate[i] = C_CRATEL;
    g_tileCrate[i * 8] = C_CRATEL;
    g_tileCrate[56 + i] = C_CRATED;
    g_tileCrate[i * 8 + 7] = C_CRATED;
  }
  for (int i = 0; i < 8; i++) {
    int d = i < 4 ? i + 3 : i - 4;
    g_tileCrate[i * 8 + d] = C_CRATED;
    if (d + 1 < 8) g_tileCrate[i * 8 + d + 1] = C_CRATEL;
  }
  g_tileCrate[3 * 8 + 3] = C_CRATED;
  g_tileCrate[3 * 8 + 4] = C_CRATED;
  g_tileCrate[4 * 8 + 3] = C_CRATED;
  g_tileCrate[4 * 8 + 4] = C_CRATED;
  g_tileCrate[1 * 8 + 1] = C_CRATEL;
  g_tileCrate[6 * 8 + 6] = C_CRATEL;
}

static void buildGround() {
  /* Damier + bruit + brins d'herbe, en 64x64 */
  for (int y = 0; y < 64; y++)
    for (int x = 0; x < 64; x++) {
      bool chk = ((x / 32) + (y / 32)) % 2;
      g_ground[y * 64 + x] = noiseTile(chk ? C_GRASS2 : C_GRASS, 1);
    }
  for (int k = 0; k < 90; k++) {
    int x = rnd(62) + 1, y = rnd(61) + 1;
    g_ground[y * 64 + x] = C_GRASS3;
    g_ground[(y + 1) * 64 + x] = RGB(38,96,38);
  }
  /* Fleurs et cailloux */
  static const uint16_t FLOWERS[4] = { RGB(255,240,120), RGB(255,120,150), RGB(255,255,255), RGB(150,150,255) };
  for (int k = 0; k < 14; k++) {
    int x = rnd(60) + 2, y = rnd(60) + 2;
    uint16_t c = FLOWERS[rnd(4)];
    g_ground[y * 64 + x] = c;
    g_ground[y * 64 + x - 1] = c;
    g_ground[(y - 1) * 64 + x] = RGB(255,200,200);
  }
  for (int k = 0; k < 10; k++) {
    int x = rnd(61) + 1, y = rnd(61) + 1;
    g_ground[y * 64 + x] = RGB(120,130,110);
    g_ground[(y + 1) * 64 + x] = RGB(90,100,85);
  }
}

static void buildGraphics() {
  for (int i = 0; i < 10; i++) buildSprite(i);
  buildPortraits();
  buildTiles();
  buildGround();
}

/* ================================================================ */
/* Helpers de jeu                                                    */
/* ================================================================ */

static inline int tileAt(int wx, int wy) {
  if (wx < 0 || wy < 0 || wx >= WORLD_W || wy >= WORLD_H) return T_WALL;
  return g_map[(wy / TILE) * MAP_W + (wx / TILE)];
}
static inline bool solid(int wx, int wy) {
  int t = tileAt(wx, wy);
  return t == T_WALL || t == T_CRATE;
}
static inline bool inBush(int wx, int wy) { return tileAt(wx, wy) == T_BUSH; }
static inline bool inZone(int wx, int wy) {
  return wx >= g_zx && wx < g_zx + g_zw && wy >= g_zy && wy < g_zy + g_zh;
}
static inline int dist2(int ax, int ay, int bx, int by) {
  int dx = ax - bx, dy = ay - by; return dx * dx + dy * dy;
}
static int statMul(int base, int power) { return base * (10 + power - 1) / 10; }

static void spawnPart(int x, int y, uint16_t c, int n, int spd) {
  for (int i = 0; i < N_PART && n > 0; i++) {
    Part & p = g_parts[i];
    if (p.alive) continue;
    p.x = x; p.y = y;
    p.vx = (int)(spd * (int)(rnd(101) - 50) / 50);
    p.vy = (int)(spd * (int)(rnd(101) - 50) / 50);
    p.life = 6 + rnd(10);
    p.c = c; p.alive = true;
    n--;
  }
}
static void updateParts() {
  for (int i = 0; i < N_PART; i++) {
    Part & p = g_parts[i];
    if (!p.alive) continue;
    p.x += p.vx; p.y += p.vy;
    p.vx /= 2; p.vy /= 2;
    if (--p.life <= 0) p.alive = false;
  }
}
static void spawnFloat(int x, int y, int val, bool heal) {
  for (int i = 0; i < N_FLOAT; i++) {
    Floater & f = g_floats[i];
    if (f.alive) continue;
    f.x = x; f.y = y; f.life = 24; f.val = val; f.heal = heal; f.alive = true;
    return;
  }
}

/* ================================================================ */
/* Carte                                                             */
/* ================================================================ */

static void genMap() {
  memset(g_map, T_GRASS, sizeof(g_map));
  memset(g_deco, 0, sizeof(g_deco));
  memset(g_crateHp, 0, sizeof(g_crateHp));
  for (int i = 0; i < MAP_W; i++) {
    g_map[i] = T_WALL;
    g_map[(MAP_H - 1) * MAP_W + i] = T_WALL;
  }
  for (int j = 0; j < MAP_H; j++) {
    g_map[j * MAP_W] = T_WALL;
    g_map[j * MAP_W + MAP_W - 1] = T_WALL;
  }
  for (int k = 0; k < 26; k++) {
    int cx = 2 + rnd(MAP_W - 4), cy = 2 + rnd(MAP_H - 4);
    uint8_t t = (k % 2) ? T_BUSH : T_WALL;
    int n = 2 + rnd(4);
    for (int i = 0; i < n; i++) {
      int x = cx + rnd(3) - 1, y = cy + rnd(3) - 1;
      if (x > 0 && x < MAP_W - 1 && y > 0 && y < MAP_H - 1) g_map[y * MAP_W + x] = t;
    }
  }
  for (int k = 0; k < 14; k++) {
    int x = 2 + rnd(MAP_W - 4), y = 2 + rnd(MAP_H - 4);
    if (g_map[y * MAP_W + x] == T_GRASS) {
      g_map[y * MAP_W + x] = T_CRATE;
      g_crateHp[y * MAP_W + x] = 20;
    }
  }
}

static void spawnActor(Actor * a, int idx) {
  for (int tries = 0; tries < 200; tries++) {
    int x = TILE * (2 + rnd(MAP_W - 4)) + TILE / 2;
    int y = TILE * (2 + rnd(MAP_H - 4)) + TILE / 2;
    if (solid(x, y) || inBush(x, y)) continue;
    bool ok = true;
    for (int i = 0; i < idx; i++)
      if (g_actors[i].alive && dist2(x, y, g_actors[i].x, g_actors[i].y) < 70 * 70) { ok = false; break; }
    if (!ok) continue;
    a->x = x; a->y = y;
    return;
  }
  a->x = TILE * (MAP_W / 2);
  a->y = TILE * (MAP_H / 2);
}

static void initGame() {
  genMap();
  memset(g_bullets, 0, sizeof(g_bullets));
  memset(g_cubes, 0, sizeof(g_cubes));
  memset(g_parts, 0, sizeof(g_parts));
  memset(g_floats, 0, sizeof(g_floats));
  memset(g_gemArr, 0, sizeof(g_gemArr));
  g_gemTimer = 0; g_gemCd = 60; g_hzone = 0; g_waveT = 0; g_waveN = 0;
  for (int i = 0; i < N_ACTORS; i++) {
    Actor & a = g_actors[i];
    a.bot = (i != 0);
    a.brawler = a.bot ? rnd(10) : g_brawler;
    const BrawlerDef & d = BRAWLERS[a.brawler];
    int lvl = a.bot ? 1 + rnd(5) : (g_power[g_brawler] ? g_power[g_brawler] : 1);
    a.maxhp = statMul(d.hp, lvl);
    a.hp = a.maxhp;
    a.cubes = 0; a.super = 0; a.cd = 0; a.alive = true; a.flashT = 0;
    a.fx = 1; a.fy = 0; a.wx = a.wy = 0; a.anim = 0; a.gems = 0;
    a.pvx = 0; a.pvy = 0;
    spawnActor(&a, i);
  }
  g_zx = 0; g_zy = 0; g_zw = WORLD_W; g_zh = WORLD_H;
  /* Zone brulante : carré central */
  g_hzw = 6 * TILE; g_hzh = 6 * TILE;
  g_hzx = WORLD_W / 2 - g_hzw / 2; g_hzy = WORLD_H / 2 - g_hzh / 2;
  /* Dégage les murs au centre pour les modes a objectif */
  if (g_mode == M_GEMS || g_mode == M_HOTZONE) {
    for (int j = 0; j < MAP_H; j++)
      for (int i = 0; i < MAP_W; i++) {
        int wx = i * TILE + TILE / 2, wy = j * TILE + TILE / 2;
        if (wx >= g_hzx && wx < g_hzx + g_hzw && wy >= g_hzy && wy < g_hzy + g_hzh)
          g_map[j * MAP_W + i] = T_GRASS;
      }
  }
  /* Survie : les bots emmergent des bords, plus de zone */
  if (g_mode == M_SURVIE) {
    g_actors[0].x = WORLD_W / 2; g_actors[0].y = WORLD_H / 2;
    for (int i = 1; i < N_ACTORS; i++) g_actors[i].alive = false;
    g_waveT = 90;
  }
  g_frame = 0; g_over = false; g_victory = false; g_rank = 0;
  g_shake = 0; g_flash = 0;
  /* difficulté progressive : paliers de trophées 1 a 10 */
  static const int AI_STEP[11] = { 100, 250, 400, 500, 650, 750, 1000, 1400, 1750, 2000, 999999 };
  g_aiDiff = 0;
  while (g_aiDiff < 10 && g_troph[g_brawler] >= AI_STEP[g_aiDiff]) g_aiDiff++;
  g_cx = g_actors[0].x - SCREEN_W / 2;
  g_cy = g_actors[0].y - VIEW_H / 2;
}

/* ================================================================ */
/* Deplacement                                                       */
/* ================================================================ */

static bool canStand(int wx, int wy) {
  return !solid(wx - 3, wy - 3) && !solid(wx + 3, wy - 3) &&
         !solid(wx - 3, wy + 3) && !solid(wx + 3, wy + 3);
}
static void moveActor(Actor * a, int dx, int dy) {
  if (dx && canStand(a->x + dx, a->y)) { a->x += dx; a->pvx = dx; }
  if (dy && canStand(a->x, a->y + dy)) { a->y += dy; a->pvy = dy; }
  if (dx || dy) {
    a->fx = dx; a->fy = dy;
    if (++a->anim >= 8) a->anim = 0;
  }
}
static void moveActorSmart(Actor * a, float dx, float dy) {
  float len = sqrtf(dx * dx + dy * dy);
  if (len < 0.01f) return;
  dx /= len; dy /= len;
  for (int attempt = 0; attempt < 8; attempt++) {
    int sp = BRAWLERS[a->brawler].speed;
    int ix = (int)(dx * sp + (dx > 0 ? 0.5f : -0.5f));
    int iy = (int)(dy * sp + (dy > 0 ? 0.5f : -0.5f));
    if (ix == 0 && iy == 0) {
      ix = dx > 0 ? 1 : (dx < 0 ? -1 : 0);
      iy = dy > 0 ? 1 : (dy < 0 ? -1 : 0);
    }
    if ((ix && canStand(a->x + ix, a->y)) || (iy && canStand(a->x, a->y + iy))) {
      moveActor(a, ix, iy);
      return;
    }
    float ang = (attempt % 2 ? 1 : -1) * (0.7854f * (1 + attempt / 2));
    float c = cosf(ang), s = sinf(ang);
    float nx = dx * c - dy * s, ny = dx * s + dy * c;
    dx = nx; dy = ny;
  }
}

/* ================================================================ */
/* Tirs et degats                                                    */
/* ================================================================ */

static void shoot(int owner, float dx, float dy, bool isSuper) {
  Actor & a = g_actors[owner];
  const BrawlerDef & d = BRAWLERS[a.brawler];
  int lvl = a.bot ? 1 : (g_power[a.brawler] ? g_power[a.brawler] : 1);
  int dmg = statMul(d.dmg, lvl) + 2 * a.cubes;
  if (isSuper) dmg *= 2;
  int pellets = isSuper ? (d.pellets + 2) : d.pellets;
  float base = atan2f(dy, dx);
  float spread = d.pellets > 1 ? 0.38f : 0.0f;
  if (isSuper) spread += 0.25f;
  for (int p = 0; p < pellets; p++) {
    float ang = base + (pellets > 1 ? ((float)p / (pellets - 1) - 0.5f) * 2 * spread : 0);
    for (int i = 0; i < N_BULLETS; i++) {
      Bullet & b = g_bullets[i];
      if (b.alive) continue;
      b.x = a.x; b.y = a.y;
      b.dx = cosf(ang) * d.bspd;
      b.dy = sinf(ang) * d.bspd;
      b.owner = owner; b.life = BULLET_LIFE; b.dmg = dmg;
      b.super = isSuper; b.alive = true;
      break;
    }
  }
  spawnPart(a.x + (int)(cosf(base) * 7), a.y + (int)(sinf(base) * 7),
            isSuper ? C_SUPER : RGB(255,200,120), isSuper ? 8 : 3, 3);
  a.flashT = 2;
  if (isSuper) { a.super = 0; g_shake = 4; }
}

static void dropCubes(int x, int y, int n) {
  for (int k = 0; k < N_CUBES && n > 0; k++) {
    if (!g_cubes[k].alive) {
      g_cubes[k].alive = true;
      g_cubes[k].x = x + rnd(9) - 4;
      g_cubes[k].y = y + rnd(9) - 4;
      n--;
    }
  }
}

static void damageActor(int idx, int dmg, int from) {
  Actor & a = g_actors[idx];
  if (!a.alive) return;
  a.hp -= dmg;
  a.flashT = 3;
  spawnPart(a.x, a.y, RGB(255,80,80), 4, 3);
  spawnFloat(a.x, a.y - 8, dmg, false);
  if (from >= 0 && from != idx && g_actors[from].alive) {
    Actor & f = g_actors[from];
    f.super += dmg;
    if (f.super > 100) f.super = 100;
  }
  if (idx == 0) g_shake = 3;
  if (a.hp > 0) return;
  a.alive = false;
  spawnPart(a.x, a.y, BRAWLERS[a.brawler].color, 14, 5);
  spawnPart(a.x, a.y, RGB(255,160,60), 8, 4);
  spawnPart(a.x, a.y, C_WHITE, 4, 6);
  dropCubes(a.x, a.y, a.cubes + 1);
  if (a.gems) dropGem(a.x, a.y, a.gems);
  int aliveCount = 0;
  for (int i = 0; i < N_ACTORS; i++) if (g_actors[i].alive) aliveCount++;
  if (g_mode == M_SHOWDOWN) {
    if (idx == 0) { g_over = true; g_rank = aliveCount + 1; g_flash = RGB(255,60,60); }
    else if (aliveCount == 1 && g_actors[0].alive) { g_over = true; g_victory = true; g_rank = 1; g_flash = C_GOLD; }
  } else {
    if (idx == 0) { g_over = true; g_victory = false; g_flash = RGB(255,60,60); if (g_mode != M_SURVIE) g_rank = 2; }
  }
}

/* ================================================================ */
/* IA des bots                                                       */
/* ================================================================ */

/* Niveau des bots : 0 = noob ... 10 = pro. Monte avec les trophées. */
static void botThink(Actor * a, int idx) {
  const BrawlerDef & d = BRAWLERS[a->brawler];
  int D = g_aiDiff;

  /* ---- 1. Perception : cible la plus proche ET la plus blessee ---- */
  int seeR2 = d.range * d.range * 2;
  int bushR = 28 + D * 4;              /* détection dans les buissons selon niveau */
  int tx = -1;
  long tsc = 1L << 40;
  for (int i = 0; i < N_ACTORS; i++) {
    if (i == idx || !g_actors[i].alive) continue;
    int dd = dist2(a->x, a->y, g_actors[i].x, g_actors[i].y);
    if (dd > seeR2) continue;
    if (inBush(g_actors[i].x, g_actors[i].y) && dd > bushR * bushR) continue;
    long sc = (long)dd * 2 + (long)g_actors[i].hp * 300L / (g_actors[i].maxhp ? g_actors[i].maxhp : 1);
    if (sc < tsc) { tsc = sc; tx = i; }
  }

  /* ---- 2. Esquive des balles entrantes (niveau 3+) ---- */
  bool dodging = false;
  if (D >= 3) {
    for (int i = 0; i < N_BULLETS; i++) {
      Bullet & b = g_bullets[i];
      if (!b.alive || b.owner == idx) continue;
      if (dist2(a->x, a->y, b.x, b.y) > 26 * 26) continue;
      int rx = a->x - b.x, ry = a->y - b.y;
      if (rx * (int)b.dx + ry * (int)b.dy > 0) {   /* elle vient sur moi */
        int px2 = -(int)b.dy, py2 = (int)b.dx;     /* strafe perpendiculaire */
        if (rnd(2)) { px2 = -px2; py2 = -py2; }
        moveActorSmart(a, (float)px2, (float)py2);
        dodging = true;
        break;
      }
    }
  }

  /* ---- 3. Fuite intelligente (niveau 2+) : vers le buisson le plus proche ---- */
  bool outside = (g_mode == M_HOTZONE) ? false : !inZone(a->x, a->y);
  int zcx = g_zx + g_zw / 2, zcy = g_zy + g_zh / 2;
  bool fleeing = (a->hp < a->maxhp / 3) && (tx >= 0) && (D >= 2) && !dodging;

  if (fleeing) {
    if (D >= 5) {
      int bx = -1, by = 0, bd = 1 << 30;
      for (int j = a->y / TILE - 3; j <= a->y / TILE + 3; j++) {
        for (int i = a->x / TILE - 3; i <= a->x / TILE + 3; i++) {
          if (i < 0 || j < 0 || i >= MAP_W || j >= MAP_H) continue;
          if (g_map[j * MAP_W + i] != T_BUSH) continue;
          int dd = dist2(a->x, a->y, i * TILE + 4, j * TILE + 4);
          if (dd < bd) { bd = dd; bx = i * TILE + 4; by = j * TILE + 4; }
        }
      }
      if (bx >= 0) { moveActorSmart(a, (float)(bx - a->x), (float)(by - a->y)); return; }
    }
    if (tx >= 0) { moveActorSmart(a, (float)(a->x - g_actors[tx].x), (float)(a->y - g_actors[tx].y)); return; }
  }

  /* ---- 4. Retour objectif ---- */
  if (outside && !dodging) {
    moveActorSmart(a, (float)(zcx - a->x), (float)(zcy - a->y));
  } else if (!dodging) {
    if (tx >= 0) {
      int dd = dist2(a->x, a->y, g_actors[tx].x, g_actors[tx].y);
      int ideal = d.range * 3 / 4;
      /* aggro agressif aux bas niveaux, kiting aux hauts niveaux */
      if (D < 4) ideal = d.range / 2;
      if (dd > ideal * ideal) {
        float jitter = (float)(rnd(41) - 20) / 20.0f / (D >= 6 ? 2 : 1);
        moveActorSmart(a, (float)(g_actors[tx].x - a->x) + jitter,
                          (float)(g_actors[tx].y - a->y) + jitter);
      } else if (rnd(3) == 0 || D >= 6) {
        /* strafe autour de la cible (orbit), plus marqué aux hauts niveaux */
        int sx = g_actors[tx].y - a->y, sy = -(g_actors[tx].x - a->x);
        if (rnd(2)) { sx = -sx; sy = -sy; }
        moveActorSmart(a, (float)sx * 2, (float)sy * 2);
      }
    } else {
      if (g_mode == M_GEMS) {
        int gx = -1, gy = 0;
        long gd = 1L << 40;
        for (int k = 0; k < N_GEMS; k++) {
          if (!g_gemArr[k].alive) continue;
          long dd = dist2(a->x, a->y, g_gemArr[k].x, g_gemArr[k].y);
          if (dd < gd) { gd = dd; gx = g_gemArr[k].x; gy = g_gemArr[k].y; }
        }
        if (gx >= 0) moveActorSmart(a, (float)(gx - a->x), (float)(gy - a->y));
        else moveActorSmart(a, (float)(WORLD_W / 2 - a->x), (float)(WORLD_H / 2 - a->y));
      } else if (g_mode == M_HOTZONE) {
        if (dist2(a->x, a->y, g_hzx + g_hzw / 2, g_hzy + g_hzh / 2) > 20 * 20)
          moveActorSmart(a, (float)(g_hzx + g_hzw / 2 - a->x + rnd(41) - 20),
                            (float)(g_hzy + g_hzh / 2 - a->y + rnd(41) - 20));
      } else {
        if (a->wx == 0 || dist2(a->x, a->y, a->wx, a->wy) < 12 * 12) {
          a->wx = g_zx + 8 + rnd(g_zw > 16 ? g_zw - 16 : 1);
          a->wy = g_zy + 8 + rnd(g_zh > 16 ? g_zh - 16 : 1);
        }
        moveActorSmart(a, (float)(a->wx - a->x), (float)(a->wy - a->y));
      }
    }
  }

  /* ---- 5. Tir : anticipation de trajectoire + précision progressive ---- */
  if (tx >= 0 && a->cd <= 0) {
    int dd = dist2(a->x, a->y, g_actors[tx].x, g_actors[tx].y);
    if (dd < d.range * d.range) {
      const Actor & t = g_actors[tx];
      /* anticipation : vise ou la cible SERA (niveau 2+) */
      float lead = (D >= 2) ? 0.35f + D * 0.05f : 0.0f;
      float px = (float)t.x + t.pvx * lead;
      float py = (float)t.y + t.pvy * lead;
      float ang = atan2f(py - (float)a->y, px - (float)a->x);
      /* erreur de visée : enorme a 0, quasi nulle a 10 */
      ang += ((float)(rnd(21) - 10) / 60.0f) * (10 - D) / 6.0f;
      /* super garde pour une cible proche (niveau 4+), gachie avant */
      bool useSuper = a->super >= 100;
      if (D >= 4 && dd > d.range * d.range * 6 / 10) useSuper = false;
      shoot(idx, cosf(ang), sinf(ang), useSuper);
      /* réaction plus rapide aux hauts niveaux */
      a->cd = d.cd + rnd(12 - D > 0 ? 12 - D : 2);
    }
  }

  /* ---- 6. Casser les caisses proches ---- */
  if (a->cd <= 0 && tx < 0 && !dodging) {
    static const int sx8[4] = { 1, -1, 0, 0 }, sy8[4] = { 0, 0, 1, -1 };
    for (int dd = 0; dd < 4; dd++) {
      int px2 = a->x + sx8[dd] * 20, py2 = a->y + sy8[dd] * 20;
      if (tileAt(px2, py2) == T_CRATE) {
        shoot(idx, (float)sx8[dd], (float)sy8[dd], false);
        a->cd = d.cd;
        break;
      }
    }
  }
  if (a->cd > 0) a->cd--;
  if (a->flashT > 0) a->flashT--;
}

/* ================================================================ */
/* Balles / zone                                                     */
/* ================================================================ */

static void updateBullets() {
  for (int i = 0; i < N_BULLETS; i++) {
    Bullet & b = g_bullets[i];
    if (!b.alive) continue;
    b.x += (int)b.dx;
    b.y += (int)b.dy;
    b.life--;
    if (b.life <= 0 || b.x < 0 || b.y < 0 || b.x >= WORLD_W || b.y >= WORLD_H) { b.alive = false; continue; }
    int t = tileAt(b.x, b.y);
    if (t == T_WALL) { spawnPart(b.x, b.y, C_WALLL, 3, 2); b.alive = false; continue; }
    if (t == T_CRATE) {
      int ti = (b.y / TILE) * MAP_W + (b.x / TILE);
      g_crateHp[ti] = (uint8_t)(g_crateHp[ti] > b.dmg ? g_crateHp[ti] - b.dmg : 0);
      if (g_crateHp[ti] == 0) {
        g_map[ti] = T_GRASS;
        spawnPart((ti % MAP_W) * TILE + 4, (ti / MAP_W) * TILE + 4, C_CRATEL, 10, 3);
        dropCubes((ti % MAP_W) * TILE + 4, (ti / MAP_W) * TILE + 4, 1);
      }
      b.alive = false;
      continue;
    }
    for (int a = 0; a < N_ACTORS; a++) {
      if (a == b.owner || !g_actors[a].alive) continue;
      if (dist2(b.x, b.y, g_actors[a].x, g_actors[a].y) < 5 * 5) {
        damageActor(a, b.dmg, b.owner);
        b.alive = false;
        break;
      }
    }
  }
}

static void updateZone() {
  if (g_mode != M_SHOWDOWN) return;
  if (g_frame < ZONE_GRACE) return;
  if (g_frame % ZONE_TICK) return;
  if (g_zw > ZONE_MIN) { g_zx += ZONE_SHRINK / 2; g_zw -= ZONE_SHRINK; }
  if (g_zh > ZONE_MIN) { g_zy += ZONE_SHRINK / 2; g_zh -= ZONE_SHRINK; }
  if (g_zw < ZONE_MIN) g_zw = ZONE_MIN;
  if (g_zh < ZONE_MIN) g_zh = ZONE_MIN;
  if (g_frame % ZONE_DMG_TICK == 0) {
    for (int i = 0; i < N_ACTORS; i++) {
      Actor & a = g_actors[i];
      if (a.alive && !inZone(a.x, a.y)) damageActor(i, ZONE_DMG, -1);
    }
  }
}

/* ================================================================ */
/* Objectifs specifiques aux modes                                   */
/* ================================================================ */

static inline bool inHot(int wx, int wy) {
  return wx >= g_hzx && wx < g_hzx + g_hzw && wy >= g_hzy && wy < g_hzy + g_hzh;
}

static void dropGem(int x, int y, int n) {
  for (int k = 0; k < N_GEMS && n > 0; k++) {
    if (!g_gemArr[k].alive) {
      g_gemArr[k].alive = true;
      g_gemArr[k].x = x + rnd(19) - 9;
      g_gemArr[k].y = y + rnd(19) - 9;
      n--;
    }
  }
}

static void updateMode() {
  if (g_over) return;
  if (g_mode == M_GEMS) {
    /* fontaine a gemmes au centre */
    if (--g_gemCd <= 0) {
      g_gemCd = 110;
      for (int k = 0; k < N_GEMS; k++) {
        if (!g_gemArr[k].alive) {
          g_gemArr[k].alive = true;
          g_gemArr[k].x = WORLD_W / 2 + rnd(40) - 20;
          g_gemArr[k].y = WORLD_H / 2 + rnd(40) - 20;
          spawnPart(g_gemArr[k].x, g_gemArr[k].y, C_PURPLE, 5, 2);
          break;
        }
      }
    }
    /* ramassage par tout le monde */
    for (int i = 0; i < N_ACTORS; i++) {
      Actor & a = g_actors[i];
      if (!a.alive) continue;
      for (int k = 0; k < N_GEMS; k++) {
        if (!g_gemArr[k].alive) continue;
        if (dist2(a.x, a.y, g_gemArr[k].x, g_gemArr[k].y) < 8 * 8) {
          g_gemArr[k].alive = false;
          a.gems++;
          if (i == 0) { spawnPart(a.x, a.y, C_PURPLE, 6, 3); spawnFloat(a.x, a.y - 8, 0, true); }
        }
      }
    }
    /* decompte du joueur */
    if (g_actors[0].gems >= GEM_GOAL) {
      if (g_gemTimer < GEM_TIMER) {
        g_gemTimer++;
        if (g_gemTimer % 25 == 0) { spawnPart(g_actors[0].x, g_actors[0].y, C_PURPLE, 8, 3); g_shake = 2; }
      }
      if (g_gemTimer >= GEM_TIMER) { g_over = true; g_victory = true; g_rank = 1; g_flash = C_GOLD; }
    } else {
      g_gemTimer = 0;
    }
    /* un bot a tout ramasse : il panique et lache ses gemmes */
    for (int i = 1; i < N_ACTORS; i++) {
      if (g_actors[i].alive && g_actors[i].gems >= GEM_GOAL) {
        dropGem(g_actors[i].x, g_actors[i].y, 4);
        g_actors[i].gems = GEM_GOAL - 4;
        g_shake = 4;
      }
    }
  }
  else if (g_mode == M_HOTZONE) {
    bool pin = inHot(g_actors[0].x, g_actors[0].y) && g_actors[0].alive;
    int botsIn = 0;
    for (int i = 1; i < N_ACTORS; i++)
      if (g_actors[i].alive && inHot(g_actors[i].x, g_actors[i].y)) botsIn++;
    if (pin && botsIn == 0) { if (g_frame % 5 == 0) { g_hzone++; if (g_hzone % 20 == 0) spawnPart(g_actors[0].x, g_actors[0].y, RGB(90,220,255), 4, 2); } }
    else if (!pin && botsIn > 0) { if (g_frame % 3 == 0) g_hzone--; }
    if (g_hzone > HZ_GOAL) g_hzone = HZ_GOAL;
    if (g_hzone < -HZ_GOAL) g_hzone = -HZ_GOAL;
    if (g_hzone >= HZ_GOAL) { g_over = true; g_victory = true; g_rank = 1; g_flash = C_GOLD; }
    else if (g_hzone <= -HZ_GOAL) { g_over = true; g_victory = false; g_rank = 3; g_flash = RGB(255,60,60); }
  }
  else if (g_mode == M_SURVIE) {
    /* vagues : des bots emmergent des bords de plus en plus forts */
    if (--g_waveT <= 0) {
      g_waveN++;
      g_waveT = 300 - g_waveN * 12;
      if (g_waveT < 120) g_waveT = 120;
      int spawned = 0;
      for (int i = 1; i < N_ACTORS && spawned < 3; i++) {
        Actor & a = g_actors[i];
        if (a.alive) continue;
        int edge = rnd(4);
        a.x = (edge == 0) ? TILE : (edge == 1) ? WORLD_W - TILE * 2 : TILE * (1 + rnd(MAP_W - 2));
        a.y = (edge < 2) ? TILE * (1 + rnd(MAP_H - 2)) : (edge == 2) ? TILE : WORLD_H - TILE * 2;
        if (solid(a.x, a.y)) continue;
        a.alive = true;
        a.hp = a.maxhp;
        a.cubes = 0;
        a.brawler = rnd(10);
        spawned++;
      }
      g_flash = RGB(200,60,60);
      g_shake = 3;
    }
    g_rank = 1 + g_waveN / 4;  /* "rank" = duree survivee */
  }
}

/* ramassage des cubes commun a tous les modes */
static void pickupCubes() {
  for (int i = 0; i < N_ACTORS; i++) {
    Actor & a = g_actors[i];
    if (!a.alive) continue;
    for (int k = 0; k < N_CUBES; k++) {
      if (!g_cubes[k].alive) continue;
      if (dist2(a.x, a.y, g_cubes[k].x, g_cubes[k].y) < 7 * 7) {
        g_cubes[k].alive = false;
        a.cubes++;
        a.maxhp += 10; a.hp += 10;
        if (i == 0) { spawnPart(a.x, a.y, C_CUBE, 6, 3); spawnFloat(a.x, a.y - 8, 0, true); }
      }
    }
  }
}

/* ================================================================ */
/* Rendu du jeu                                                      */
/* ================================================================ */

static void drawBrawler(int sx, int sy, const Actor & a) {
  bool flip = a.fx < 0;
  int bob = ((a.anim > 0) && (a.fx || a.fy)) ? ((a.anim / 4) % 2) : 0;
  blit(g_sprites[a.brawler], 24, 24, sx - 12, sy - 12 + bob, flip);
  /* arme orientee vers la visee */
  int wx = a.fx, wy = a.fy;
  if (wx == 0 && wy == 0) wx = 1;
  uint16_t wc = BRAWLERS[a.brawler].color;
  for (int s = 12; s <= 15; s++)
    fillRect(sx + wx * s / 6 - 1, sy + wy * s / 6 - 1 + bob, 2, 2, C_OUT);
  fillRect(sx + wx * 15 / 6 - 1, sy + wy * 15 / 6 - 1 + bob, 3, 3, wc);
  /* flash de bouche */
  if (a.flashT > 0) {
    fillRect(sx + wx * 17 / 6 - 2, sy + wy * 17 / 6 - 2 + bob, 5, 5, RGB(255,240,150));
    fillRect(sx + wx * 17 / 6 - 1, sy + wy * 17 / 6 - 1 + bob, 3, 3, C_WHITE);
  }
  /* dans un buisson : translucide simule par pointilles */
  if (inBush(a.x, a.y)) {
    for (int y = 0; y < 24; y += 2)
      fillRect(sx - 12, sy - 12 + y + bob, 24, 1, C_BUSH);
  }
  /* barre de vie */
  int hpw = (10 * (a.hp > 0 ? a.hp : 0)) / a.maxhp;
  fillRect(sx - 6, sy - 16 + bob, 12, 3, C_OUT);
  fillRect(sx - 5, sy - 15 + bob, 10, 1, C_HPBG);
  fillRect(sx - 5, sy - 15 + bob, hpw, 1, a.bot ? C_HPGREEN : RGB(90,230,255));
  /* cubes */
  if (a.cubes) fillRect(sx + 10, sy - 16 + bob, 3, 3, C_CUBE);
  /* super pret : halo pulse */
  if (a.super >= 100 && (g_frame / 4) % 2) {
    fillRect(sx - 13, sy - 4 + bob, 1, 7, C_SUPER);
    fillRect(sx + 13, sy - 4 + bob, 1, 7, C_SUPER);
    fillRect(sx - 1, sy - 17 + bob, 2, 1, C_SUPER);
  }
}

static void draw() {
  Actor & p = g_actors[0];
  g_cx = p.x - SCREEN_W / 2;
  g_cy = p.y - VIEW_H / 2;
  if (g_cx < 0) g_cx = 0;
  if (g_cx > WORLD_W - SCREEN_W) g_cx = WORLD_W - SCREEN_W;
  if (g_cy < 0) g_cy = 0;
  if (g_cy > WORLD_H - VIEW_H) g_cy = WORLD_H - VIEW_H;
  if (g_shake > 0) {
    g_cx += rnd(2 * g_shake + 1) - g_shake;
    g_cy += rnd(2 * g_shake + 1) - g_shake;
    if (g_cx < 0) g_cx = 0;
    if (g_cy < 0) g_cy = 0;
    if (g_cx > WORLD_W - SCREEN_W) g_cx = WORLD_W - SCREEN_W;
    if (g_cy > WORLD_H - VIEW_H) g_cy = WORLD_H - VIEW_H;
    g_shake--;
  }

  /* Sol texture : motifs 64x64 */
  for (int gy = 0; gy < VIEW_H + 64; gy += 64)
    for (int gx = 0; gx < SCREEN_W + 64; gx += 64) {
      int sx = gx - (g_cx % 64), sy = HUD_H + gy - (g_cy % 64);
      blit(g_ground, 64, 64, sx, sy, false);
    }

  /* Ombres portees des murs et caisses (passe 1) */
  int tx0 = g_cx / TILE, ty0 = g_cy / TILE;
  int tx1 = (g_cx + SCREEN_W) / TILE + 1, ty1 = (g_cy + VIEW_H) / TILE + 1;
  for (int ty = ty0; ty <= ty1 && ty < MAP_H; ty++) {
    for (int tx = tx0; tx <= tx1 && tx < MAP_W; tx++) {
      uint8_t t = g_map[ty * MAP_W + tx];
      if (t != T_WALL && t != T_CRATE) continue;
      int sx = tx * TILE - g_cx, sy = HUD_H + ty * TILE - g_cy;
      fillRect(sx + 1, sy + 8, 8, 2, RGB(30,50,30));
      fillRect(sx + 8, sy + 1, 2, 8, RGB(30,50,30));
    }
  }

  /* Tuiles texturees */
  for (int ty = ty0; ty <= ty1 && ty < MAP_H; ty++) {
    for (int tx = tx0; tx <= tx1 && tx < MAP_W; tx++) {
      int idx = ty * MAP_W + tx;
      int sx = tx * TILE - g_cx, sy = HUD_H + ty * TILE - g_cy;
      uint8_t t = g_map[idx];
      if (t == T_GRASS) {
        uint8_t dc = g_deco[idx];
        if (dc) {
          fillRect(sx + 1, sy + 2, 1, 3, C_GRASS2);
          fillRect(sx + 4, sy + 1, 1, 4, C_GRASS3);
          fillRect(sx + 6, sy + 3, 1, 2, C_GRASS2);
        }
        continue;
      }
      const uint16_t * tex = (t == T_WALL) ? g_tileWall : (t == T_BUSH) ? g_tileBush : g_tileCrate;
      blit(tex, 8, 8, sx, sy, false);
      /* Buissons : feuille qui scintille */
      if (t == T_BUSH && ((g_frame / 20 + tx * 3 + ty * 5) % 7) == 0) {
        fillRect(sx + 2, sy + 2, 2, 1, C_BUSHL);
        fillRect(sx + 5, sy + 4, 1, 2, C_BUSHL);
      }
    }
  }

  /* Bord de zone : pointilles defilants (showdown seulement) */
  if (g_mode == M_SHOWDOWN) {
    int zsx = g_zx - g_cx, zsy = HUD_H + g_zy - g_cy;
    int zex = zsx + g_zw, zey = zsy + g_zh;
    int off = (g_frame / 3) % 8;
    for (int x = zsx - off; x < zex; x += 8) {
      fillRect(x, zsy, 4, 1, C_ZONE);
      fillRect(x, zey - 1, 4, 1, C_ZONE);
    }
    for (int y = zsy - off; y < zey; y += 8) {
      fillRect(zsx, y, 1, 4, C_ZONE);
      fillRect(zex - 1, y, 1, 4, C_ZONE);
    }
  }

  /* Zone brulante : carré violet pulse */
  if (g_mode == M_HOTZONE) {
    int hx0 = g_hzx - g_cx, hy0 = HUD_H + g_hzy - g_cy;
    uint16_t zc = (g_frame / 12) % 2 ? RGB(140,60,220) : RGB(110,40,190);
    if (g_hzone > 0) zc = (g_frame / 12) % 2 ? RGB(60,160,240) : RGB(40,120,200);
    for (int x = hx0; x < hx0 + g_hzw; x += 4) {
      for (int y = hy0; y < hy0 + g_hzh; y += 4) {
        if (((x + y) / 4) % 2 == 0) fillRect(x, y, 4, 4, zc);
      }
    }
    for (int x = hx0; x < hx0 + g_hzw; x += 8) {
      fillRect(x, hy0, 4, 2, C_WHITE);
      fillRect(x, hy0 + g_hzh - 2, 4, 2, C_WHITE);
    }
  }

  /* Gemmes : losanges violets scintillants */
  if (g_mode == M_GEMS) {
    for (int k = 0; k < N_GEMS; k++) {
      if (!g_gemArr[k].alive) continue;
      int gx = g_gemArr[k].x - 3 - g_cx, gy = HUD_H + g_gemArr[k].y - 3 - g_cy;
      int bob = (g_frame / 10 + k) % 2;
      fillRect(gx + 1, gy - bob, 4, 6, C_OUT);
      fillRect(gx + 2, gy + 1 - bob, 2, 4, C_PURPLE);
      fillRect(gx + 2, gy + 1 - bob, 1, 1, RGB(220,160,255));
      if ((g_frame / 6 + k) % 8 == 0) fillRect(gx + 3, gy - 2 - bob, 1, 1, C_WHITE);
    }
  }

  /* Cubes */
  for (int k = 0; k < N_CUBES; k++) {
    if (!g_cubes[k].alive) continue;
    int sx = g_cubes[k].x - 2 - g_cx, sy = HUD_H + g_cubes[k].y - 2 - g_cy;
    int bob = (g_frame / 10 + k) % 2;
    fillRect(sx, sy - bob, 5, 5, C_OUT);
    fillRect(sx + 1, sy + 1 - bob, 3, 3, C_CUBE);
    fillRect(sx + 1, sy + 1 - bob, 2, 1, RGB(255,240,140));
    if ((g_frame / 8 + k) % 9 == 0) fillRect(sx + 2, sy - 3 - bob, 1, 2, C_WHITE);
  }

  /* Balles : halo + coeur */
  for (int i = 0; i < N_BULLETS; i++) {
    if (!g_bullets[i].alive) continue;
    Bullet & b = g_bullets[i];
    int sz = b.super ? 5 : 3;
    int sx = b.x - sz / 2 - g_cx, sy = HUD_H + b.y - sz / 2 - g_cy;
    /* trainee */
    int tx = (int)(b.x - b.dx * 4) - g_cx, ty = HUD_H + (int)(b.y - b.dy * 4) - g_cy;
    if (b.super) {
      fillRect(tx - 1, ty - 1, sz + 2, sz + 2, RGB(160,120,20));
      fillRect(tx, ty, sz, sz, RGB(200,170,60));
    } else {
      fillRect(tx, ty, sz - 1, sz - 1, RGB(230,190,120));
    }
    fillRect(sx - 1, sy - 1, sz + 2, sz + 2, b.super ? RGB(180,150,40) : RGB(200,150,60));
    fillRect(sx, sy, sz, sz, b.super ? C_SUPER : C_BULLET);
    /* reflet */
    fillRect(sx, sy, 1, 1, C_WHITE);
  }

  /* Particules */
  for (int i = 0; i < N_PART; i++) {
    if (!g_parts[i].alive) continue;
    fillRect(g_parts[i].x - 1 - g_cx, HUD_H + g_parts[i].y - 1 - g_cy, 2, 2, g_parts[i].c);
  }

  /* Acteurs (joueur au-dessus) */
  for (int i = N_ACTORS - 1; i >= 0; i--) {
    Actor & a = g_actors[i];
    if (!a.alive) continue;
    int sx = a.x - g_cx, sy = HUD_H + a.y - g_cy;
    if (sx < -12 || sy < HUD_H - 14 || sx > SCREEN_W + 12 || sy > SCREEN_H + 14) continue;
    drawBrawler(sx, sy, a);
  }

  /* Degats flottants */
  for (int i = 0; i < N_FLOAT; i++) {
    Floater & f = g_floats[i];
    if (!f.alive) continue;
    f.y--;
    if (--f.life <= 0) { f.alive = false; continue; }
    char b[8];
    b[0] = '-';
    int v = f.val;
    b[1] = (char)('0' + (v / 100) % 10);
    b[2] = (char)('0' + (v / 10) % 10);
    b[3] = (char)('0' + v % 10);
    b[4] = 0;
    drawText(b, f.x - g_cx - 8, HUD_H + f.y - g_cy, f.life > 8 ? C_WHITE : C_GREY, RGB(46,112,46), false);
  }

  /* HUD */
  fillRect(0, 0, SCREEN_W, HUD_H, C_HUD);
  fillRect(0, HUD_H - 1, SCREEN_W, 1, C_OUT);
  blit(g_sprites[p.brawler], 24, 24, 0, 0, false);
  fillRect(24, 0, 1, HUD_H, C_OUT);
  int hpw = (50 * (p.hp > 0 ? p.hp : 0)) / p.maxhp;
  fillRect(28, 3, 50, 5, C_HPBG);
  fillRect(28, 3, hpw, 5, C_HPGREEN);
  fillRect(28, 3, hpw, 2, RGB(120,240,120));
  fillRect(28, 10, 50, 3, RGB(50,42,10));
  for (int s = 0; s < 10; s++) {
    int segStart = 28 + s * 5;
    int segFill = (p.super >= (s + 1) * 10) ? 5 : (p.super > s * 10 ? (p.super % 10) * 5 / 10 : 0);
    if (segFill) fillRect(segStart, 10, segFill, 3, C_SUPER);
  }
  char buf[24];
  buf[0] = 'x'; buf[1] = (char)('0' + (p.cubes / 10) % 10); buf[2] = (char)('0' + p.cubes % 10); buf[3] = 0;
  drawText(buf, 84, 3, C_CUBE, C_HUD, false);
  if (g_mode == M_SHOWDOWN) {
    int aliveCount = 0;
    for (int i = 0; i < N_ACTORS; i++) if (g_actors[i].alive) aliveCount++;
    buf[0] = 'V'; buf[1] = ':'; buf[2] = (char)('0' + aliveCount / 10); buf[3] = (char)('0' + aliveCount % 10); buf[4] = 0;
    drawText(buf, 106, 3, C_WHITE, C_HUD, false);
    if (g_frame < ZONE_GRACE) {
      int s = (ZONE_GRACE - g_frame) / 20;
      buf[0] = 'Z'; buf[1] = ':'; buf[2] = (char)('0' + (s / 60) % 10); buf[3] = (char)('0' + (s / 10) % 6); buf[4] = (char)('0' + s % 10); buf[5] = 0;
      drawText(buf, 136, 3, (ZONE_GRACE - g_frame < 150) ? C_ZONE : C_GREY, C_HUD, false);
    } else {
      if ((g_frame / 8) % 2) drawText("ZONE !", 136, 3, C_ZONE, C_HUD, false);
    }
  }
  else if (g_mode == M_GEMS) {
    /* compteur de gemmes bleu/violet selon qui mene */
    buf[0] = 'G'; buf[1] = ':'; buf[2] = (char)('0' + (p.gems / 10) % 10); buf[3] = (char)('0' + p.gems % 10); buf[4] = 0;
    drawText(buf, 106, 3, p.gems >= GEM_GOAL ? C_SUPER : RGB(200,150,255), C_HUD, true);
    if (g_gemTimer > 0) {
      int s = (GEM_TIMER - g_gemTimer) / 20;
      buf[0] = (char)('0' + (s / 10) % 10); buf[1] = (char)('0' + s % 10); buf[2] = 0;
      drawText(buf, 136, 3, (g_frame / 6) % 2 ? C_WHITE : C_GOLD, C_HUD, true);
      drawText("DECOMPTE", 154, 3, C_GOLD, C_HUD, false);
    } else {
      drawText("GEMMES !", 130, 3, RGB(200,150,255), C_HUD, false);
    }
  }
  else if (g_mode == M_HOTZONE) {
    /* jauge de controle */
    int gw = 60;
    fillRect(110, 3, gw, 5, RGB(60,20,80));
    int f = g_hzone * gw / 2 / HZ_GOAL;
    if (g_hzone >= 0) fillRect(110 + gw / 2, 3, f, 5, RGB(90,200,255));
    else fillRect(110 + gw / 2 + f, 3, -f, 5, RGB(230,80,230));
    fillRect(110 + gw / 2, 2, 1, 7, C_WHITE);
    drawText("ZONE", 110, 10, C_WHITE, C_HUD, false);
  }
  else if (g_mode == M_SURVIE) {
    buf[0] = 'V'; buf[1] = 'A'; buf[2] = 'G'; buf[3] = 'U'; buf[4] = 'E'; buf[5] = ' ';
    buf[6] = (char)('0' + (g_waveN / 10) % 10); buf[7] = (char)('0' + g_waveN % 10); buf[8] = 0;
    drawText(buf, 106, 3, C_ZONE, C_HUD, true);
  }
  if (p.super >= 100 && (g_frame / 6) % 2) drawText("SUPER !", 278, 3, C_SUPER, C_HUD, false);
}

/* ================================================================ */
/* Menus                                                             */
/* ================================================================ */

static void drawGradientBg() {
  for (int y = 0; y < SCREEN_H; y += 8)
    fillRect(0, y, SCREEN_W, 8, RGB(14 + y / 10, 10 + y / 16, 34 + y / 6));
  for (int k = 0; k < 24; k++) {
    int x = (k * 137 + 40) % SCREEN_W;
    int y = (k * 89) % (SCREEN_H - 40);
    if ((g_frame / 20 + k) % 5) fillRect(x, y, 1, 1, RGB(90,80,140));
  }
}

static void drawTitle() {
  drawGradientBg();
  for (int y = 170; y < SCREEN_H; y += 16)
    for (int x = (y / 16) % 2 * 16; x < SCREEN_W; x += 32)
      fillRect(x, y, 16, 16, RGB(26,60,26));
  uint16_t tc = (g_frame / 16) % 2 ? C_GOLD : RGB(255,235,110);
  drawTextC("TYMBRAWL", 54, C_OUT, C_BG, true);
  drawTextC("TYMBRAWL", 52, C_OUT, C_BG, true);
  drawTextC("TYMBRAWL", 50, tc, C_BG, true);
  drawTextC("SHOWDOWN 10 JOUEURS", 78, C_WHITE, C_BG, false);
  int bob = (g_frame / 12) % 2;
  blit(g_sprites[g_sel], 24, 24, 104, 138 + bob, false);
  blit(g_sprites[(g_sel + 3) % 10], 24, 24, 190, 139 - bob, true);
  int sparkY = 148 + (g_frame / 4) % 4;
  fillRect(156, sparkY, 4, 2, C_SUPER);
  fillRect(157, sparkY - 2, 2, 6, C_SUPER);
  drawTextC("OK : JOUER", 188, C_WHITE, C_BG, false);
  drawTextC("Hommage non-officiel a Brawl Stars", 216, C_GREY, C_BG, false);
}

static void drawSelect() {
  /* Fond lobby : bleu nuit Brawl Stars + etoiles */
  for (int y = 0; y < SCREEN_H; y += 8)
    fillRect(0, y, SCREEN_W, 8, RGB(10 + y / 14, 16 + y / 18, 52 + y / 9));
  for (int k = 0; k < 30; k++) {
    int x = (k * 113 + 17) % SCREEN_W;
    int y = (k * 71 + 9) % (SCREEN_H - 60);
    if ((g_frame / 25 + k) % 6) fillRect(x, y, 1, 1, RGB(120,140,220));
  }
  /* rayons behind portrait */
  int pcx = 160, pcy = 66;
  for (int k = 0; k < 12; k++) {
    int ang = k * 30 + (g_frame / 3);
    int rx = pcx + (ang * 7 % 70) - 35;
    int ry = pcy + (ang * 13 % 46) - 23;
    if ((g_frame / 6 + k) % 3) fillRect(rx, ry, 2, 1, RGB(70,80,160));
  }

  /* ---- Barre du haut : trophées + PP ---- */
  fillRect(0, 0, SCREEN_W, 24, RGB(22,34,80));
  fillRect(0, 24, SCREEN_W, 2, C_OUT);
  /* coupe d'or */
  fillRect(8, 5, 10, 8, C_GOLD);
  fillRect(9, 4, 8, 1, C_GOLD);
  fillRect(6, 6, 2, 4, C_GOLD);
  fillRect(18, 6, 2, 4, C_GOLD);
  fillRect(10, 13, 6, 2, RGB(200,150,20));
  fillRect(8, 15, 10, 2, C_GOLD);
  int tot = 0;
  for (int i = 0; i < 10; i++) tot += g_troph[i];
  char buf[24];
  buf[0] = (char)('0' + (tot / 1000) % 10);
  buf[1] = (char)('0' + (tot / 100) % 10);
  buf[2] = (char)('0' + (tot / 10) % 10);
  buf[3] = (char)('0' + tot % 10);
  buf[4] = 0;
  drawText(buf, 24, 6, C_WHITE, RGB(22,34,80), true);
  /* gemme PP a droite */
  fillRect(244, 5, 12, 10, C_PURPLE);
  fillRect(246, 3, 8, 2, RGB(190,140,255));
  fillRect(246, 7, 3, 3, RGB(190,140,255));
  int pp = g_pp;
  buf[0] = (char)('0' + (pp / 100) % 10);
  buf[1] = (char)('0' + (pp / 10) % 10);
  buf[2] = (char)('0' + pp % 10);
  buf[3] = 0;
  drawText(buf, 262, 6, C_WHITE, RGB(22,34,80), true);

  /* ---- Portrait central + cadre ---- */
  const BrawlerDef & d = BRAWLERS[g_sel];
  int lvl = g_power[g_sel] ? g_power[g_sel] : 1;
  uint16_t frame = (g_frame / 10) % 2 ? RGB(96,64,160) : RGB(130,90,220);
  fillRect(pcx - 38, 30, 76, 78, C_OUT);
  fillRect(pcx - 36, 32, 72, 74, frame);
  fillRect(pcx - 34, 34, 68, 70, RGB(24,20,44));
  blit(g_portraits[g_sel], 48, 48, pcx - 24, 40, false);
  /* nom sur plaque colorée */
  fillRect(pcx - 34, 92, 68, 12, d.color);
  fillRect(pcx - 34, 92, 68, 1, RGB(255,255,255));
  drawTextC(d.name, 94, C_OUT, d.color, true);

  /* ---- Badge POWER ---- */
  fillRect(pcx - 68, 34, 26, 26, C_OUT);
  fillRect(pcx - 66, 36, 22, 22, C_GOLD);
  drawText("P", pcx - 60, 38, RGB(90,60,0), C_GOLD, true);
  buf[0] = (char)('0' + lvl / 10); buf[1] = (char)('0' + lvl % 10); buf[2] = 0;
  drawText(buf, pcx - 60, 48, C_OUT, C_GOLD, false);

  /* ---- Trophées du brawler ---- */
  fillRect(pcx + 42, 34, 44, 26, C_OUT);
  fillRect(pcx + 44, 36, 40, 22, RGB(30,26,50));
  fillRect(pcx + 48, 40, 8, 6, C_GOLD);
  fillRect(pcx + 46, 41, 2, 3, C_GOLD);
  fillRect(pcx + 56, 41, 2, 3, C_GOLD);
  int t = g_troph[g_sel];
  buf[0] = (char)('0' + (t / 100) % 10); buf[1] = (char)('0' + (t / 10) % 10); buf[2] = (char)('0' + t % 10); buf[3] = 0;
  drawText(buf, pcx + 48, 48, C_GOLD, RGB(30,26,50), true);

  /* ---- Fleches gauche / droite ---- */
  bool blink = (g_frame / 8) % 2;
  uint16_t ac = blink ? C_WHITE : RGB(160,160,200);
  for (int i = 0; i < 6; i++) {
    fillRect(16 + i, 54 + i, 1, 14 - 2 * i, ac);   /* triangle gauche */
    fillRect(303 - i, 54 + i, 1, 14 - 2 * i, ac);  /* triangle droit */
  }

  /* ---- Carte mode SHOWDOWN (gauche) ---- */
  fillRect(8, 148, 136, 60, C_OUT);
  fillRect(10, 150, 132, 56, RGB(34,28,60));
  fillRect(10, 150, 132, 2, RGB(70,60,110));
  fillRect(16, 158, 40, 40, RGB(20,16,40));
  /* crane du showdown */
  fillRect(24, 164, 24, 18, C_WHITE);
  fillRect(26, 162, 20, 2, C_WHITE);
  fillRect(28, 169, 5, 6, C_OUT);
  fillRect(39, 169, 5, 6, C_OUT);
  fillRect(31, 178, 6, 3, C_OUT);
  fillRect(24, 170, 24, 1, C_OUT);
  drawText("SHOWDOWN", 62, 158, C_WHITE, RGB(34,28,60), true);
  drawText("10 JOUEURS", 62, 172, C_GREY, RGB(34,28,60), false);
  drawText("ROYAUME", 62, 184, RGB(120,220,120), RGB(34,28,60), false);

  /* ---- Gros bouton JOUER (droite) ---- */
  bool pulse = (g_frame / 10) % 2;
  fillRect(168, 146, 144, 64, C_OUT);
  fillRect(172, 150, 136, 56, pulse ? RGB(255,225,90) : C_GOLD);
  fillRect(172, 150, 136, 8, RGB(255,245,160));
  fillRect(172, 198, 136, 8, RGB(200,150,20));
  drawTextC("JOUER", 168, C_OUT, pulse ? RGB(255,225,90) : C_GOLD, true);

  /* ---- Aide ---- */
  /* ---- Niveau des bots (progressif) ---- */
  fillRect(10, 110, 96, 18, C_OUT);
  fillRect(12, 112, 92, 14, RGB(24,20,44));
  drawText("BOTS", 16, 116, RGB(240,140,60), RGB(24,20,44), true);
  buf[0] = 'N'; buf[1] = 'V'; buf[2] = ' ';;
  buf[3] = (char)('0' + (g_aiDiff / 10) % 10); buf[4] = (char)('0' + g_aiDiff % 10); buf[5] = '/'; buf[6] = '1'; buf[7] = '0'; buf[8] = 0;
  drawText(buf, 56, 116, C_WHITE, RGB(24,20,44), true);

  drawTextC("OK: JOUER   EXE: AMELIORER   BACK: MODES", 214, RGB(150,150,190), RGB(10,14,40), false);
  if (lvl < MAX_POWER) {
    int cost = PP_COST(lvl);
    bool can = g_pp >= cost;
    buf[0] = 'P'; buf[1] = 'O'; buf[2] = 'W'; buf[3] = '+'; buf[4] = ':'; buf[5] = ' ';
    buf[6] = (char)('0' + (cost / 100) % 10); buf[7] = (char)('0' + (cost / 10) % 10); buf[8] = (char)('0' + cost % 10);
    buf[9] = ' '; buf[10] = 'P'; buf[11] = 'P'; buf[12] = 0;
    drawText(buf, 130, 116, can ? C_SUPER : C_GREY, RGB(24,20,44), false);
  } else {
    drawTextC("POWER MAX !", 130, 116, C_SUPER, RGB(24,20,44), false);
  }
}

static void drawModeSelect() {
  /* Fond decale en diagonale, façon lobby */
  for (int y = 0; y < SCREEN_H; y += 16)
    for (int x = ((y / 16) % 2) * 16 - (g_frame / 8) % 16; x < SCREEN_W; x += 32)
      fillRect(x, y, 16, 16, RGB(20, 40 + y / 10, 70 + y / 10));
  drawTextC("CHOISIS LE MODE", 6, C_WHITE, RGB(10,30,60), true);
  fillRect(0, 26, SCREEN_W, 2, C_OUT);

  static const char * NAMES[4] = { "SHOWDOWN", "GEMMES", "ZONE BRULANTE", "SURVIE" };
  static const char * DESC1[4] = { "10 joueurs, dernier", "Ramasse 10 gemmes", "Controle le carré", "Vagues infinies de" };
  static const char * DESC2[4] = { "vivant gagne", "et tiens le decompte", "central jusqu'a 100%", "bots a eliminer" };
  static const uint16_t MC[4] = { C_ZONE, C_PURPLE, RGB(60,160,240), RGB(240,140,60) };

  for (int m = 0; m < N_MODES; m++) {
    int cx = (m % 2) ? 164 : 8;
    int cy = (m / 2) ? 122 : 44;
    bool on = (g_sel == m);
    fillRect(cx - 2, cy - 2, 154, 72, on ? C_GOLD : C_OUT);
    fillRect(cx, cy, 150, 68, on ? RGB(44,36,80) : RGB(24,20,44));
    /* icone */
    fillRect(cx + 6, cy + 10, 46, 46, on ? MC[m] : RGB(50,44,70));
    if (m == 0) {           /* crane */
      fillRect(cx + 15, cy + 17, 28, 22, C_WHITE);
      fillRect(cx + 19, cy + 24, 6, 8, C_OUT);
      fillRect(cx + 31, cy + 24, 6, 8, C_OUT);
      fillRect(cx + 22, cy + 33, 8, 4, C_OUT);
    } else if (m == 1) {    /* gemme */
      fillRect(cx + 22, cy + 14, 14, 8, RGB(220,160,255));
      fillRect(cx + 15, cy + 22, 28, 12, C_PURPLE);
      fillRect(cx + 22, cy + 34, 14, 8, RGB(100,40,160));
      fillRect(cx + 24, cy + 16, 4, 4, C_WHITE);
    } else if (m == 2) {    /* cible */
      fillRect(cx + 12, cy + 16, 34, 34, C_WHITE);
      fillRect(cx + 17, cy + 21, 24, 24, MC[m]);
      fillRect(cx + 24, cy + 28, 10, 10, C_WHITE);
    } else {                /* eclair de vague */
      fillRect(cx + 12, cy + 20, 34, 5, RGB(120,200,255));
      fillRect(cx + 18, cy + 29, 34, 5, RGB(120,200,255));
      fillRect(cx + 24, cy + 38, 34, 5, RGB(120,200,255));
    }
    drawTextC(NAMES[m], cy + 14, on ? C_GOLD : C_WHITE, on ? RGB(44,36,80) : RGB(24,20,44), true);
    drawText(DESC1[m], cx + 58, cy + 34, C_GREY, on ? RGB(44,36,80) : RGB(24,20,44), false);
    drawText(DESC2[m], cx + 58, cy + 44, C_GREY, on ? RGB(44,36,80) : RGB(24,20,44), false);
  }
  drawTextC("OK : CHOISIR    BACK : TITRE", 212, RGB(150,150,190), RGB(10,30,60), false);
}

static void applyEndRewards() {
  if (g_mode == M_SHOWDOWN) {
    static const int TROPH_BY_RANK[11] = { 0, 8, 6, 5, 4, 3, 2, 1, 0, -1, -2 };
    int r = g_rank > 10 ? 10 : g_rank;
    g_lastTroph = TROPH_BY_RANK[r];
  } else if (g_mode == M_SURVIE) {
    g_lastTroph = 1 + g_waveN / 2;
    if (g_lastTroph > 10) g_lastTroph = 10;
  } else {
    g_lastTroph = g_victory ? 8 : -1;
  }
  g_troph[g_brawler] += g_lastTroph;
  if (g_troph[g_brawler] < 0) g_troph[g_brawler] = 0;
  g_lastPP = (g_mode == M_SURVIE ? 4 * g_waveN : 20 + 5 * g_lastCubes);
  g_pp += g_lastPP;
  g_lastRank = g_rank;
}

static void drawEnd() {
  drawGradientBg();
  uint16_t bc = g_victory ? C_GOLD : RGB(120,30,30);
  fillRect(0, 30, SCREEN_W, 42, bc);
  fillRect(0, 30, SCREEN_W, 2, C_OUT);
  fillRect(0, 70, SCREEN_W, 2, C_OUT);
  static const char * MODE_NAMES[4] = { "SHOWDOWN", "GEMMES", "ZONE BRULANTE", "SURVIE" };
  const char * title;
  if (g_mode == M_SHOWDOWN) title = g_victory ? "VICTOIRE ROYALE !" : "ELIMINE !";
  else if (g_mode == M_GEMS) title = g_victory ? "10 GEMMES !" : "GEMMES PERDUES !";
  else if (g_mode == M_HOTZONE) title = g_victory ? "ZONE CAPTUREE !" : "ZONE PERDUE !";
  else title = g_victory ? "SURVIVANT !" : "SUBMERGE !";
  drawTextC(title, 42, C_WHITE, bc, true);
  drawTextC(MODE_NAMES[g_mode], 62, C_OUT, bc, false);
  if (g_victory) {
    for (int k = 0; k < 20; k++) {
      int x = (k * 113 + g_frame * 3) % SCREEN_W;
      int y = (k * 61 + g_frame * 2) % 30 + 74;
      fillRect(x, y, 2, 2, (k % 3) ? C_SUPER : RGB(90,240,220));
    }
  }
  char buf[24];
  if (g_mode == M_SURVIE) {
    buf[0] = 'V'; buf[1] = 'A'; buf[2] = 'G'; buf[3] = 'U'; buf[4] = 'E'; buf[5] = 'S'; buf[6] = ':';
    buf[7] = (char)('0' + (g_waveN / 10) % 10); buf[8] = (char)('0' + g_waveN % 10); buf[9] = 0;
  } else {
    buf[0] = 'R'; buf[1] = 'A'; buf[2] = 'N'; buf[3] = 'K'; buf[4] = ':';
    int r = g_rank > 10 ? 10 : g_rank;
    buf[5] = ' '; buf[6] = r == 10 ? '1' : (char)('0' + r); buf[7] = r == 10 ? '0' : 0; buf[8] = 0;
  }
  drawTextC(buf, 84, C_WHITE, C_BG, false);
  blit(g_portraits[g_brawler], 48, 48, 136, 96, false);
  int at = g_lastTroph < 0 ? -g_lastTroph : g_lastTroph;
  buf[0] = g_lastTroph >= 0 ? '+' : '-';
  buf[1] = (char)('0' + at / 10); buf[2] = (char)('0' + at % 10); buf[3] = ' ';
  buf[4] = 'T'; buf[5] = 'R'; buf[6] = 'O'; buf[7] = 'P'; buf[8] = 0;
  drawTextC(buf, 140, g_lastTroph >= 0 ? C_GOLD : C_GREY, C_BG, false);
  buf[0] = '+'; buf[1] = (char)('0' + g_lastPP / 100); buf[2] = (char)('0' + (g_lastPP / 10) % 10); buf[3] = (char)('0' + g_lastPP % 10);
  buf[4] = ' '; buf[5] = 'P'; buf[6] = 'P'; buf[7] = 0;
  drawTextC(buf, 154, C_PURPLE, C_BG, false);
  drawTextC("OK: CONTINUER", 196, C_WHITE, C_BG, false);
}

/* ================================================================ */
/* Boucle principale                                                 */
/* ================================================================ */

static uint64_t g_prevKeys = 0;
static bool pressed(uint64_t st, eadk_key_t k) {
  return keyDown(st, k) && !keyDown(g_prevKeys, k);
}

int main(int argc, char * argv[]) {
  (void)argc; (void)argv;
  buildGraphics();
  memset(g_power, 0, sizeof(g_power));
  memset(g_troph, 0, sizeof(g_troph));
  g_pp = 40;
  g_brawler = 0;
  g_mode = 0;

  while (1) {
    uint64_t st = eadk_keyboard_scan();

    if (g_state == S_TITLE) {
      drawTitle();
      g_frame++;
      if (pressed(st, eadk_key_ok) || pressed(st, eadk_key_exe)) {
        g_state = S_MODE;
        g_sel = 0;
        g_frame = 0;
      }
    }
    else if (g_state == S_MODE) {
      drawModeSelect();
      g_frame++;
      if (pressed(st, eadk_key_left) || pressed(st, eadk_key_up))   g_sel = (g_sel + N_MODES - 1) % N_MODES;
      if (pressed(st, eadk_key_right) || pressed(st, eadk_key_down)) g_sel = (g_sel + 1) % N_MODES;
      if (pressed(st, eadk_key_ok) || pressed(st, eadk_key_exe)) {
        g_mode = g_sel;
        g_state = S_SELECT;
        g_sel = g_brawler;
        g_frame = 0;
      }
      if (pressed(st, eadk_key_back)) g_state = S_TITLE;
    }
    else if (g_state == S_SELECT) {
      drawSelect();
      g_frame++;
      if (pressed(st, eadk_key_left))  g_sel = (g_sel + 9) % 10;
      if (pressed(st, eadk_key_right)) g_sel = (g_sel + 1) % 10;
      if (pressed(st, eadk_key_ok)) {
        g_brawler = g_sel;
        initGame();
        g_state = S_GAME;
      }
      if (pressed(st, eadk_key_exe)) {
        int lvl = g_power[g_sel] ? g_power[g_sel] : 1;
        int cost = PP_COST(lvl);
        if (lvl < MAX_POWER && g_pp >= cost) {
          g_pp -= cost;
          g_power[g_sel] = lvl + 1;
          g_flash = C_SUPER;
        }
      }
      if (pressed(st, eadk_key_back)) g_state = S_MODE;
    }
    else if (g_state == S_GAME) {
      Actor & p = g_actors[0];
      if (!g_over) {
        const BrawlerDef & d = BRAWLERS[p.brawler];
        int dx = 0, dy = 0;
        if (keyDown(st, eadk_key_left))  dx -= d.speed;
        if (keyDown(st, eadk_key_right)) dx += d.speed;
        if (keyDown(st, eadk_key_up))    dy -= d.speed;
        if (keyDown(st, eadk_key_down))  dy += d.speed;
        moveActor(&p, dx, dy);

        if ((keyDown(st, eadk_key_ok) || keyDown(st, eadk_key_exe)) && p.cd <= 0) {
          int tx = -1, tdist = d.range * d.range;
          for (int i = 1; i < N_ACTORS; i++) {
            if (!g_actors[i].alive) continue;
            int dd = dist2(p.x, p.y, g_actors[i].x, g_actors[i].y);
            if (inBush(g_actors[i].x, g_actors[i].y) && dd > 40 * 40) continue;
            if (dd < tdist) { tdist = dd; tx = i; }
          }
          float fx = 0, fy = 0;
          bool isSuper = p.super >= 100;
          if (tx >= 0) {
            fx = (float)(g_actors[tx].x - p.x);
            fy = (float)(g_actors[tx].y - p.y);
          } else if (dx || dy) {
            fx = (float)dx; fy = (float)dy;
          } else {
            fx = (float)p.fx; fy = (float)p.fy;
            if (fx == 0 && fy == 0) fx = 1;
          }
          shoot(0, fx, fy, isSuper);
          p.cd = d.cd;
        }
        if (p.cd > 0) p.cd--;

        for (int i = 1; i < N_ACTORS; i++)
          if (g_actors[i].alive) botThink(&g_actors[i], i);

        pickupCubes();

        updateBullets();
        updateZone();
        updateMode();
        updateParts();

        g_frame++;
        draw();

        if (g_flash) {
          fillRect(0, 0, SCREEN_W, SCREEN_H, g_flash);
          g_flash = 0;
        }
      } else {
        applyEndRewards();
        g_state = S_END;
      }
    }
    else if (g_state == S_END) {
      drawEnd();
      g_frame++;
      if (pressed(st, eadk_key_ok) || pressed(st, eadk_key_back)) {
        g_state = S_SELECT;
        g_sel = g_brawler;
      }
    }

    g_prevKeys = st;
    eadk_timing_msleep(g_state == S_GAME ? 40 : 30);
  }
}
