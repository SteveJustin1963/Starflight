/*
 * ╔══════════════════════════════════════════════════════════════╗
 * ║         STARFLIGHT -- ASCII TERMINAL EDITION                  ║
 * ║   Tribute to the 1986/1989 classic by Binary Systems / EA    ║
 * ║                                                              ║
 * ║   Build:  gcc -o starflight starflight.c -lncurses -lm      ║
 * ║   Run:    ./starflight                                       ║
 * ║   Needs:  terminal >= 80 columns x 30 rows                  ║
 * ╚══════════════════════════════════════════════════════════════╝
 */
#define _XOPEN_SOURCE 700
#include <ncurses.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <math.h>

/* ════════════════════ CONSTANTS ════════════════════════════ */
#define GW          72    /* galaxy map width  (cols)         */
#define GH          18    /* galaxy map height (rows)         */
#define NS          50    /* number of star systems           */
#define SW          52    /* surface terrain width            */
#define SH          14    /* surface terrain height           */
#define MC           6    /* max crew members                 */
#define MK           8    /* max distinct cargo types         */
#define NM           4    /* captain's log lines shown        */
#define HIRE_POOL    8    /* crew available at starport       */
#define FUEL_MOVE    2    /* endurium per hyperspace step     */
#define FUEL_LAND    5    /* endurium to land on planet       */

/* Main viewport origin & size (fixed layout, min 80×30) */
#define MX    0
#define MY    1
#define MW   56
#define MH   19

/* Right panel */
#define PX   57
#define PY    1
#define PW   23
#define PH   19

/* Log panel */
#define LX    0
#define LY   20
#define LH    6

/* Control panel */
#define CX    0
#define CY   26
#define CH    4

/* ════════════════════ ENUMS ════════════════════════════════ */
typedef enum {
    GS_TITLE, GS_STARPORT, GS_HYPER, GS_ORBIT,
    GS_SURFACE, GS_ENC, GS_COMBAT, GS_QUIT
} GState;

typedef enum { RC_HUMAN=0, RC_VELOX, RC_THRYNN, RC_ELOWAN, RC_ANDROID } Race;
typedef enum { RL_CAPTAIN=0, RL_NAV, RL_ENG, RL_MED, RL_SCI, RL_GUN } Role;
typedef enum { PT_ROCKY=0, PT_OCEAN, PT_VOLC, PT_ARCT,
               PT_JUNG, PT_DSRT, PT_TOXIC } PType;
typedef enum { AL_VELOX=0, AL_THRYNN, AL_ELOWAN,
               AL_SPEMIN, AL_GAZ, AL_MECH } AType;

/* ════════════════════ STRUCTS ══════════════════════════════ */
typedef struct {
    char name[20];
    Race race; Role role;
    int  hp, skill, morale;   /* 0–100 each */
} Crew;

typedef struct {
    char name[16];
    int  amt, val;            /* units owned, RU value each */
} Cargo;

typedef struct {
    int  hull, mhull;         /* current / max hull pts     */
    int  shld, mshld;         /* current / max shield pts   */
    int  weap;                /* weapon level 0-4           */
    int  eng;                 /* engine level 0-4           */
    int  fuel, mfuel;         /* endurium / max             */
    int  ru;                  /* resource units (money)     */
    int  x, y;                /* galaxy position            */
    Crew  crew[MC]; int nc;
    Cargo cargo[MK]; int nk, cused, ccap;
} Ship;

typedef struct {
    int   x, y;
    char  name[22];
    char  ch;                 /* display character          */
    int   cls;                /* star class 0-5             */
    int   np;                 /* number of planets          */
    PType planets[6];
    int   alien;              /* has alien presence         */
    AType atype;
    int   mood;               /* 0=hostile … 100=friendly   */
    int   visited;
    int   home;               /* Arth home system           */
} Star;

typedef struct {
    char  map[SH][SW+1];
    int   min[SH][SW];        /* mineral type (-1 = none)  */
    int   tx, ty;             /* terrain vehicle position  */
    PType ptype;
    int   atm;                /* 0=none 1=thin 2=ok 3=toxic */
    int   grav;               /* 0=low 1=norm 2=high       */
    int   temp;               /* -2 .. +2                  */
} Surf;

/* ════════════════════ GLOBALS ══════════════════════════════ */
static Ship   ship;
static Star   gal[NS];
static Surf   surf;
static GState gs;

static char   logbuf[NM][120];
static int    nlog = 0;

static int    cstar   = 0;   /* index of star we are at/near  */
static int    cpla    = 0;   /* planet index in orbit/surface  */
static int    cx, cy;        /* hyperspace cursor              */

/* encounter */
static int    emood;
static AType  ealien;

/* combat */
static int    ehull, eshld;
static char   ename[28];
static int    combat_log_idx = 0;
static char   clog[4][80];

/* starport sub-page 0=main 1=hire 2=sell */
static int    spsel     = 0;
static int    sp_cursor = 0;   /* main menu cursor 0-7       */
static int    hisel     = 0;   /* hire pool cursor 0-7       */
static int    sell_sel  = 0;   /* sell cargo cursor 0-nk     */
static Crew   hipool[HIRE_POOL];
static int    hiused[HIRE_POOL];

/* orbit */
static int    orbsel = 0;

/* encounter cursor 0-7: 0-2=tone, 3-5=msg, 6=attack, 7=break */
static int    enc_sel = 0;

/* combat cursor 0-3: fire/shield/evade/retreat */
static int    cbt_sel = 0;

/* ════════════════════ STRING TABLES ════════════════════════ */
static const char *RN[] = {"Human","Velox","Thrynn","Elowan","Android"};
static const char *RL[] = {"Captain","Nav","Eng","Med","Sci","Gun"};
static const char *PT[] = {"Rocky","Ocean","Volcanic","Arctic","Jungle","Desert","Toxic"};
static const char *AN[] = {"Velox","Thrynn","Elowan","Spemin","Gazurtoid","Mechans"};
static const char *AG[] = {
    "VELOX> We observe your vessel. State your purpose here.",
    "THRYNN> You enter Thrynn territory. Explain yourself!",
    "ELOWAN> Greetings, traveler. We sense your peaceful intent.",
    "SPEMIN> Oh! Please don't fire! We are very friendly!",
    "GAZURTOID> GRAAAK. You disturb... our hunting grounds.",
    "MECHANS> UNIT DETECTED. IDENTIFY OR BE TERMINATED."
};
static const char *MN[] = {
    "Endurium","Nexidium","Cobalt","Silicon","Fluorine","Zenon","Plutanium"
};
static const int MV[] = {5, 12, 8, 3, 6, 15, 20};

/* surface tile per planet type */
static const char PC[]  = ".~^*TZ%";
/* star display char per class  */
static const char SC[]  = ".*+oO@";

/* ════════════════════ COLOR PAIRS ══════════════════════════ */
enum {
    CP_TITLE=1, CP_STAT, CP_STAR, CP_SHIP,
    CP_MSG, CP_MENU, CP_CREW, CP_GOOD,
    CP_WARN, CP_DANGER, CP_SURF, CP_TV, CP_DIM
};

static void setup_colors(void) {
    start_color();
    use_default_colors();
    init_pair(CP_TITLE,  COLOR_CYAN,    COLOR_BLUE);
    init_pair(CP_STAT,   COLOR_YELLOW,  -1);
    init_pair(CP_STAR,   COLOR_YELLOW,  -1);
    init_pair(CP_SHIP,   COLOR_WHITE,   -1);
    init_pair(CP_MSG,    COLOR_WHITE,   -1);
    init_pair(CP_MENU,   COLOR_CYAN,    -1);
    init_pair(CP_CREW,   COLOR_GREEN,   -1);
    init_pair(CP_GOOD,   COLOR_GREEN,   -1);
    init_pair(CP_WARN,   COLOR_YELLOW,  -1);
    init_pair(CP_DANGER, COLOR_RED,     -1);
    init_pair(CP_SURF,   COLOR_GREEN,   -1);
    init_pair(CP_TV,     COLOR_BLACK,   COLOR_WHITE);
    init_pair(CP_DIM,    COLOR_BLUE,    -1);
}

/* ════════════════════ UTILITIES ════════════════════════════ */
static void logmsg(const char *fmt, ...) {
    if (nlog >= NM) {
        memmove(logbuf[0], logbuf[1], sizeof(logbuf[0]) * (NM - 1));
        nlog = NM - 1;
    }
    va_list ap; va_start(ap, fmt);
    vsnprintf(logbuf[nlog++], 120, fmt, ap);
    va_end(ap);
}

static int clamp(int v, int lo, int hi) {
    return v < lo ? lo : v > hi ? hi : v;
}

/* ════════════════════ DEBUG LOG FILE ═══════════════════════ */
static FILE *logfp = NULL;

static void dlog(const char *fmt, ...) {
    if (!logfp) return;
    va_list ap; va_start(ap, fmt);
    vfprintf(logfp, fmt, ap);
    va_end(ap);
    fputc('\n', logfp);
    fflush(logfp);
}

/* ════════════════════ INIT ══════════════════════════════════ */
static void init_galaxy(void) {
    srand(42);
    static const char *snames[] = {
        "Arth","Rigel","Vega","Sirius","Proxima","Tau Ceti",
        "Epsilon Eri","Barnard's","Wolf 359","Lalande 21",
        "Luyten 726","Ross 154","Ross 248","Epsilon Ind",
        "Kruger 60","Delta Pav","82 Eridani","Eta Cass",
        "Beta Hyd","Zeta Tuc","HR 1614","Gliese 667",
        "55 Cancri","HD 40307","Kepler-22","Upsilon And",
        "47 UMa","Mu Ara","GJ 1214","Gliese 581",
        "HD 85512","Tau Bootis","HD 189733","51 Peg",
        "Psi Ser","Xi Pup","Theta Sco","Iota Hor",
        "Alpha For","Zeta Ret","Kappa Cet","Chi Dra",
        "Omicron Eri","HR 4523","Gamma Lep","Pi Men",
        "Zeta Dor","Fomalhaut","Altair","Deneb"
    };
    for (int i = 0; i < NS; i++) {
        gal[i].x       = 2 + rand() % (GW - 4);
        gal[i].y       = 1 + rand() % (GH - 2);
        gal[i].cls     = rand() % 6;
        gal[i].ch      = SC[gal[i].cls];
        gal[i].np      = 1 + rand() % 6;
        gal[i].alien   = (rand() % 3 == 0) ? 1 : 0;
        gal[i].atype   = (AType)(rand() % 6);
        gal[i].mood    = 30 + rand() % 50;
        gal[i].visited = 0;
        gal[i].home    = 0;
        strncpy(gal[i].name, snames[i % 50], 21);
        for (int p = 0; p < gal[i].np; p++)
            gal[i].planets[p] = (PType)(rand() % 7);
    }
    /* Home system */
    gal[0].x = GW / 2; gal[0].y = GH / 2;
    gal[0].home = 1; gal[0].alien = 0; gal[0].visited = 1;
    strcpy(gal[0].name, "Arth");
    srand((unsigned)time(NULL));
}

static void make_hire_pool(void) {
    static const char *names[] = {
        "Zax Dorn","Lira Vel","Unit-9","Nova Crix",
        "Sela Moon","Vorn Kix","Eko Rish","Doom Axe"
    };
    for (int i = 0; i < HIRE_POOL; i++) {
        strncpy(hipool[i].name, names[i], 19);
        hipool[i].race   = (Race)(rand() % 5);
        hipool[i].role   = (Role)(1 + rand() % 5);
        hipool[i].hp     = 70 + rand() % 31;
        hipool[i].skill  = 40 + rand() % 51;
        hipool[i].morale = 70 + rand() % 31;
        hiused[i] = 0;
    }
}

static void gen_surface(int si, int pi) {
    srand(si * 13 + pi * 7 + 42);
    surf.ptype = gal[si].planets[pi];
    surf.atm   = rand() % 4;
    surf.grav  = rand() % 3;
    surf.temp  = (rand() % 5) - 2;
    surf.tx    = SW / 2;
    surf.ty    = SH / 2;
    char base  = PC[(int)surf.ptype];
    for (int y = 0; y < SH; y++) {
        for (int x = 0; x < SW; x++) {
            int r = rand() % 10;
            if (r < 5)      surf.map[y][x] = base;
            else if (r < 8) surf.map[y][x] = '#';
            else            surf.map[y][x] = '.';
            surf.min[y][x] = (rand() % 100 < 18) ? (int)(rand() % 7) : -1;
        }
        surf.map[y][SW] = '\0';
    }
    srand((unsigned)time(NULL));
}

static void init_ship(void) {
    memset(&ship, 0, sizeof(ship));
    ship.hull = ship.mhull = 100;
    ship.shld = ship.mshld = 60;
    ship.weap = 1;
    ship.eng  = 2;
    ship.fuel = 250; ship.mfuel = 500;
    ship.ru   = 2500;
    ship.x    = gal[0].x;
    ship.y    = gal[0].y;
    ship.ccap = 50;
    /* Starting captain */
    strcpy(ship.crew[0].name, "COMMANDER");
    ship.crew[0].race  = RC_HUMAN;
    ship.crew[0].role  = RL_CAPTAIN;
    ship.crew[0].hp    = 100;
    ship.crew[0].skill = 100;
    ship.crew[0].morale= 100;
    ship.nc = 1;
    cx = ship.x; cy = ship.y;
    cstar = 0;
}

/* ════════════════════ DRAW HELPERS ══════════════════════════ */
static void draw_box(int y, int x, int h, int w, const char *t) {
    int r;
    mvaddch(y, x, ACS_ULCORNER);
    mvaddch(y, x+w-1, ACS_URCORNER);
    mvaddch(y+h-1, x, ACS_LLCORNER);
    mvaddch(y+h-1, x+w-1, ACS_LRCORNER);
    mvhline(y, x+1, ACS_HLINE, w-2);
    mvhline(y+h-1, x+1, ACS_HLINE, w-2);
    for (r = 1; r < h-1; r++) {
        mvaddch(y+r, x, ACS_VLINE);
        mvaddch(y+r, x+w-1, ACS_VLINE);
    }
    if (t && t[0]) {
        attron(A_BOLD);
        mvprintw(y, x+2, " %s ", t);
        attroff(A_BOLD);
    }
}

static void draw_status(void) {
    int cols = getmaxx(stdscr);
    attron(COLOR_PAIR(CP_TITLE)|A_BOLD);
    mvhline(0, 0, ' ', cols);
    mvprintw(0, 1, " STARFLIGHT: ASCII TERMINAL EDITION ");
    attroff(COLOR_PAIR(CP_TITLE)|A_BOLD);
    attron(COLOR_PAIR(CP_STAT)|A_BOLD);
    mvprintw(0, 37, "Hull:%3d  Shld:%3d  Fuel:%3d  RU:%5d  Crew:%d",
             ship.hull, ship.shld, ship.fuel, ship.ru, ship.nc);
    attroff(COLOR_PAIR(CP_STAT)|A_BOLD);
}

static void draw_right_panel(void) {
    draw_box(PY, PX, PH, PW, "CREW");
    for (int i = 0; i < ship.nc && i < PH-2; i++) {
        Crew *c = &ship.crew[i];
        int hpcolor = c->hp > 60 ? CP_GOOD : c->hp > 30 ? CP_WARN : CP_DANGER;
        attron(COLOR_PAIR(hpcolor));
        mvprintw(PY+1+i, PX+1, "%-9.9s %-3.3s %3d%%",
                 c->name, RL[c->role], c->hp);
        attroff(COLOR_PAIR(hpcolor));
    }
    /* Cargo summary below crew if room */
    int cargo_y = PY + 1 + ship.nc + 1;
    if (cargo_y < PY + PH - 2) {
        attron(A_BOLD);
        mvprintw(cargo_y, PX+1, "CARGO: %d/%d", ship.cused, ship.ccap);
        attroff(A_BOLD);
        for (int k = 0; k < ship.nk && cargo_y+1+k < PY+PH-1; k++) {
            attron(COLOR_PAIR(CP_STAT));
            mvprintw(cargo_y+1+k, PX+1, "%-10.10s x%d",
                     ship.cargo[k].name, ship.cargo[k].amt);
            attroff(COLOR_PAIR(CP_STAT));
        }
    }
}

static void draw_log(void) {
    int w = getmaxx(stdscr);
    draw_box(LY, LX, LH, w, "CAPTAIN'S LOG");
    for (int i = 0; i < nlog; i++) {
        attron(COLOR_PAIR(CP_MSG));
        mvprintw(LY+1+i, LX+2, "> %-*.*s", w-6, w-6, logbuf[i]);
        attroff(COLOR_PAIR(CP_MSG));
    }
}

static void draw_controls(const char *l1, const char *l2) {
    int w = getmaxx(stdscr);
    draw_box(CY, CX, CH, w, "CONTROLS");
    attron(COLOR_PAIR(CP_MENU)|A_BOLD);
    mvprintw(CY+1, CX+2, "%-*.*s", w-4, w-4, l1);
    mvprintw(CY+2, CX+2, "%-*.*s", w-4, w-4, l2);
    attroff(COLOR_PAIR(CP_MENU)|A_BOLD);
}

/* ════════════════════ TITLE SCREEN ══════════════════════════ */
static void draw_title(void) {
    erase();
    draw_status();
    draw_box(MY, MX, MH, MW, NULL);

    /* Star field */
    attron(COLOR_PAIR(CP_DIM));
    mvprintw(MY+1, MX+2, ".  *  .   *   .  *  .   *   .  *  .   *   .  *  .");
    mvprintw(MY+2, MX+2, "*   .  *   .  *   .  *   .  *   .  *   .  *   . *");
    attroff(COLOR_PAIR(CP_DIM));

    /* Block-letter title -- each letter 3 chars wide, 1 space gap = 39 chars total */
    attron(COLOR_PAIR(CP_STAR)|A_BOLD);
    mvprintw(MY+4, MX+8, "##  ###  #   ##  ###  #   ###  ##  # #  ###");
    mvprintw(MY+5, MX+8, "#    #   # # # # #    #    #   #   # # # # ");
    mvprintw(MY+6, MX+8, "##   #   ### ##  ##   #    #   # # ###  # ");
    mvprintw(MY+7, MX+8, "  #  #   # # # # #    #    #   # # # #  # ");
    mvprintw(MY+8, MX+8, "##   #   # # # # #   ###  ###  ##  # #  # ");
    attroff(COLOR_PAIR(CP_STAR)|A_BOLD);

    attron(COLOR_PAIR(CP_DIM));
    mvprintw(MY+9, MX+2, ".  *  .   *   .  *  .   *   .  *  .   *   .  *  .");
    attroff(COLOR_PAIR(CP_DIM));

    attron(COLOR_PAIR(CP_WARN)|A_BOLD);
    mvprintw(MY+11, MX+7, "A S C I I   T E R M I N A L   E D I T I O N");
    attroff(COLOR_PAIR(CP_WARN)|A_BOLD);

    attron(COLOR_PAIR(CP_MSG));
    mvprintw(MY+13, MX+6, "Tribute to the 1986/1989 classic by Binary Systems");
    mvprintw(MY+14, MX+9, "Published by Electronic Arts for the C64");
    attroff(COLOR_PAIR(CP_MSG));

    attron(COLOR_PAIR(CP_CREW)|A_BOLD);
    mvprintw(MY+16, MX+6, "270 star systems.  800 planets.  Infinite mystery.");
    attroff(COLOR_PAIR(CP_CREW)|A_BOLD);

    attron(COLOR_PAIR(CP_GOOD)|A_BOLD);
    mvprintw(MY+MH-3, MX+16, "[ Press ENTER to begin ]");
    attroff(COLOR_PAIR(CP_GOOD)|A_BOLD);

    draw_right_panel();
    draw_log();
    draw_controls(
        "[ENTER] Begin new game",
        "[Q] Quit"
    );
}

/* ════════════════════ STARPORT ══════════════════════════════ */
static int upgrade_cost(int level) { return 500 * (level + 1); }

static void starport_add_cargo(int mtype, int amount) {
    for (int k = 0; k < ship.nk; k++) {
        if (strcmp(ship.cargo[k].name, MN[mtype]) == 0) {
            ship.cargo[k].amt  += amount;
            ship.cused         += amount;
            return;
        }
    }
    if (ship.nk < MK) {
        strncpy(ship.cargo[ship.nk].name, MN[mtype], 15);
        ship.cargo[ship.nk].amt  = amount;
        ship.cargo[ship.nk].val  = MV[mtype];
        ship.cused += amount;
        ship.nk++;
    }
}

static void draw_starport(void) {
    erase();
    draw_status();
    draw_box(MY, MX, MH, MW, "INTERSTEL STARPORT -- ARTH");

    if (spsel == 0) {
        /* Main menu */
        const char *opts[] = {
            "1. Buy Fuel (100 RU for 50 units)",
            "2. Upgrade Weapons (current: %d/4)  cost: %d RU",
            "3. Upgrade Shields (current: %d/4)  cost: %d RU",
            "4. Upgrade Engines (current: %d/4)  cost: %d RU",
            "5. Hire Crew",
            "6. Sell Cargo",
            "7. Repair Hull (5 RU per point)",
            "8. Launch into Space"
        };
        attron(COLOR_PAIR(CP_GOOD)|A_BOLD);
        mvprintw(MY+1, MX+3, "Welcome to Arth Starport. Outfit your vessel, Commander.");
        attroff(COLOR_PAIR(CP_GOOD)|A_BOLD);
        for (int i = 0; i < 8; i++) {
            char buf[80];
            if (i == 1) snprintf(buf, 80, opts[i], ship.weap, upgrade_cost(ship.weap));
            else if (i == 2) snprintf(buf, 80, opts[i], ship.mshld/25, upgrade_cost(ship.mshld/25));
            else if (i == 3) snprintf(buf, 80, opts[i], ship.eng, upgrade_cost(ship.eng));
            else strncpy(buf, opts[i], 79);
            int hl = (i == sp_cursor);
            attron(COLOR_PAIR(hl ? CP_WARN : CP_MSG) | (hl ? A_BOLD : 0));
            mvprintw(MY+3+i*2, MX+2, "%s%s", hl ? "> " : "  ", buf);
            attroff(COLOR_PAIR(hl ? CP_WARN : CP_MSG) | (hl ? A_BOLD : 0));
        }
    } else if (spsel == 1) {
        /* Hire crew */
        mvprintw(MY+1, MX+3, "HIRE CREW (500 RU each)  [UP/DN] select  [ENTER] hire  [B] back");
        for (int i = 0; i < HIRE_POOL; i++) {
            Crew *c = &hipool[i];
            attron(hiused[i] ? COLOR_PAIR(CP_DIM) : (i == hisel ? COLOR_PAIR(CP_WARN)|A_BOLD : COLOR_PAIR(CP_MSG)));
            mvprintw(MY+2+i, MX+3,
                "%d. %-14.14s  %-7.7s  %-8.8s  Skill:%3d  HP:%3d  %s",
                i, c->name, RN[c->race], RL[c->role],
                c->skill, c->hp, hiused[i] ? "[HIRED]" : "");
            attroff(hiused[i] ? COLOR_PAIR(CP_DIM) : (i == hisel ? COLOR_PAIR(CP_WARN)|A_BOLD : COLOR_PAIR(CP_MSG)));
        }
    } else if (spsel == 2) {
        /* Sell cargo */
        mvprintw(MY+1, MX+3, "SELL CARGO  [UP/DN] select  [ENTER] sell  [B] back");
        int total = 0;
        for (int k = 0; k < ship.nk; k++) {
            int earn = ship.cargo[k].amt * ship.cargo[k].val;
            total += earn;
            int hl = (k == sell_sel);
            attron(COLOR_PAIR(hl ? CP_WARN : CP_STAT) | (hl ? A_BOLD : 0));
            mvprintw(MY+3+k, MX+2, "%s%d. %-12.12s  x%3d  @ %2d RU  = %4d RU",
                     hl ? "> " : "  ", k, ship.cargo[k].name,
                     ship.cargo[k].amt, ship.cargo[k].val, earn);
            attroff(COLOR_PAIR(hl ? CP_WARN : CP_STAT) | (hl ? A_BOLD : 0));
        }
        attron(COLOR_PAIR(CP_GOOD)|A_BOLD);
        mvprintw(MY+3+ship.nk+1, MX+4, "Total if sold: %d RU", total);
        attroff(COLOR_PAIR(CP_GOOD)|A_BOLD);
        if (ship.nk == 0)
            mvprintw(MY+3, MX+4, "(no cargo to sell)");
    }

    draw_right_panel();
    draw_log();
    draw_controls(
        "[1-8] Choose action   [B] Back / cancel",
        "[ENTER] Confirm selection"
    );
}

static void handle_starport(int ch) {
    /* ── arrow / enter navigation for all sub-pages ─────────── */
    if (ch == KEY_UP) {
        if (spsel == 0) sp_cursor  = (sp_cursor  - 1 + 8)        % 8;
        if (spsel == 1) hisel      = (hisel       - 1 + HIRE_POOL)% HIRE_POOL;
        if (spsel == 2 && ship.nk) sell_sel = (sell_sel - 1 + ship.nk) % ship.nk;
        return;
    }
    if (ch == KEY_DOWN) {
        if (spsel == 0) sp_cursor  = (sp_cursor  + 1) % 8;
        if (spsel == 1) hisel      = (hisel      + 1) % HIRE_POOL;
        if (spsel == 2 && ship.nk) sell_sel = (sell_sel + 1) % ship.nk;
        return;
    }
    if (ch == '\n' || ch == '\r') {
        if (spsel == 0) { handle_starport('1' + sp_cursor); return; }
        if (spsel == 1) { handle_starport('H'); return; }
        if (spsel == 2 && ship.nk) { handle_starport('0' + sell_sel); return; }
        return;
    }
    /* ── number shortcuts also update cursor position ────────── */
    if (spsel == 0 && ch >= '1' && ch <= '8') sp_cursor = ch - '1';

    if (spsel == 0) {
        switch (ch) {
        case '1':
            if (ship.fuel + 50 > ship.mfuel) {
                logmsg("Tank already full."); break;
            }
            if (ship.ru < 100) { logmsg("Not enough RU."); break; }
            ship.fuel = clamp(ship.fuel + 50, 0, ship.mfuel);
            ship.ru  -= 100;
            logmsg("Bought 50 units of Endurium.");
            break;
        case '2':
            if (ship.weap >= 4) { logmsg("Weapons at max."); break; }
            if (ship.ru < upgrade_cost(ship.weap))
                { logmsg("Not enough RU."); break; }
            ship.ru -= upgrade_cost(ship.weap);
            ship.weap++;
            logmsg("Weapons upgraded to level %d.", ship.weap);
            break;
        case '3': {
            int slv = ship.mshld / 25;
            if (slv >= 4) { logmsg("Shields at max."); break; }
            if (ship.ru < upgrade_cost(slv))
                { logmsg("Not enough RU."); break; }
            ship.ru -= upgrade_cost(slv);
            ship.mshld += 25;
            ship.shld   = ship.mshld;
            logmsg("Shields upgraded (max: %d).", ship.mshld);
            break;
        }
        case '4':
            if (ship.eng >= 4) { logmsg("Engines at max."); break; }
            if (ship.ru < upgrade_cost(ship.eng))
                { logmsg("Not enough RU."); break; }
            ship.ru -= upgrade_cost(ship.eng);
            ship.eng++;
            logmsg("Engines upgraded to level %d.", ship.eng);
            break;
        case '5':
            spsel = 1; hisel = 0;
            break;
        case '6':
            spsel = 2; sell_sel = 0;
            break;
        case '7': {
            int need = ship.mhull - ship.hull;
            int cost = need * 5;
            if (need == 0) { logmsg("Hull is at full integrity."); break; }
            if (ship.ru < cost) cost = ship.ru;
            int pts = cost / 5;
            ship.hull = clamp(ship.hull + pts, 0, ship.mhull);
            ship.ru  -= pts * 5;
            logmsg("Hull repaired by %d points.", pts);
            break;
        }
        case '8':
        case '\n': case '\r':
            if (ship.fuel < 10) {
                logmsg("Not enough fuel to launch!");
            } else {
                logmsg("Launching into hyperspace from Arth.");
                dlog("LAUNCH: fuel=%d ru=%d crew=%d", ship.fuel, ship.ru, ship.nc);
                gs = GS_HYPER;
                cx = ship.x; cy = ship.y;
            }
            break;
        }
    } else if (spsel == 1) {
        /* hire menu -- number keys jump cursor directly */
        if (ch >= '0' && ch <= '0' + HIRE_POOL - 1)
            hisel = ch - '0';
        else if (ch == 'h' || ch == 'H') {
            if (hiused[hisel]) {
                logmsg("Already hired.");
            } else if (ship.nc >= MC) {
                logmsg("Crew quarters full.");
            } else if (ship.ru < 500) {
                logmsg("Not enough RU to hire.");
            } else {
                ship.crew[ship.nc++] = hipool[hisel];
                hiused[hisel] = 1;
                ship.ru -= 500;
                logmsg("Hired %s as %s.", hipool[hisel].name, RL[hipool[hisel].role]);
            }
        } else if (ch == 'b' || ch == 'B') {
            spsel = 0;
        }
    } else if (spsel == 2) {
        /* sell cargo */
        if (ch >= '0' && ch <= '0' + ship.nk - 1) {
            int k = ch - '0';
            int earn = ship.cargo[k].amt * ship.cargo[k].val;
            ship.ru    += earn;
            ship.cused -= ship.cargo[k].amt;
            logmsg("Sold %s for %d RU.", ship.cargo[k].name, earn);
            /* remove from cargo list */
            for (int j = k; j < ship.nk - 1; j++)
                ship.cargo[j] = ship.cargo[j+1];
            ship.nk--;
        } else if (ch == 'b' || ch == 'B') {
            spsel = 0;
        }
    }
}

/* ════════════════════ HYPERSPACE ════════════════════════════ */
static int star_at(int x, int y) {
    for (int i = 0; i < NS; i++)
        if (gal[i].x == x && gal[i].y == y) return i;
    return -1;
}

static void draw_hyperspace(void) {
    erase();
    draw_status();
    draw_box(MY, MX, MH, MW, "HYPERSPACE -- GALAXY MAP");

    /* Draw star field */
    for (int i = 0; i < NS; i++) {
        int sy = MY + 1 + gal[i].y;
        int sx = MX + 1 + gal[i].x;
        if (sy < MY+MH-1 && sx < MX+MW-1) {
            if (gal[i].home) {
                attron(COLOR_PAIR(CP_GOOD)|A_BOLD);
                mvaddch(sy, sx, 'A'); /* Arth marker */
                attroff(COLOR_PAIR(CP_GOOD)|A_BOLD);
            } else {
                int col = gal[i].visited ? CP_DIM : CP_STAR;
                attron(COLOR_PAIR(col)|(gal[i].visited ? 0 : A_BOLD));
                mvaddch(sy, sx, (chtype)gal[i].ch);
                attroff(COLOR_PAIR(col)|(gal[i].visited ? 0 : A_BOLD));
            }
        }
    }

    /* Draw ship */
    int sy = MY + 1 + ship.y;
    int sx = MX + 1 + ship.x;
    if (sy < MY+MH-1 && sx < MX+MW-1) {
        attron(COLOR_PAIR(CP_SHIP)|A_BOLD);
        mvaddch(sy, sx, '>');
        attroff(COLOR_PAIR(CP_SHIP)|A_BOLD);
    }

    /* Info about nearby star */
    int near = star_at(ship.x, ship.y);
    if (near < 0) {
        /* check adjacent */
        int dx[] = {0,0,-1,1,-1,1,-1,1};
        int dy[] = {-1,1,0,0,-1,-1,1,1};
        for (int d = 0; d < 8; d++) {
            near = star_at(ship.x+dx[d], ship.y+dy[d]);
            if (near >= 0) break;
        }
    }

    attron(COLOR_PAIR(CP_MSG));
    mvprintw(MY+MH-3, MX+2, "Position: (%2d,%2d)  Fuel: %d", ship.x, ship.y, ship.fuel);
    if (near >= 0) {
        attron(COLOR_PAIR(CP_WARN)|A_BOLD);
        mvprintw(MY+MH-2, MX+2, "NEAR: %-18.18s  [%d planets]  %s",
                 gal[near].name, gal[near].np,
                 gal[near].alien ? "ALIEN PRESENCE" : "");
        attroff(COLOR_PAIR(CP_WARN)|A_BOLD);
    }
    attroff(COLOR_PAIR(CP_MSG));

    draw_right_panel();

    /* Map legend -- drawn over the empty lower half of the right panel */
    {
        int ly = PY + 1 + ship.nc + 3;   /* below crew + cargo header  */
        if (ship.nk > 0) ly += ship.nk + 1;
        if (ly > PY + PH - 11) ly = PY + PH - 11;
        attron(A_BOLD);
        mvprintw(ly,   PX+1, "MAP LEGEND");
        attroff(A_BOLD);
        /* star symbols */
        static const struct { char sym; int cp; const char *desc; } leg[] = {
            { '>',  CP_SHIP,  "Your ship"   },
            { 'A',  CP_GOOD,  "Arth (home)" },
            { '@',  CP_STAR,  "O-class star" },
            { 'O',  CP_STAR,  "A-class star" },
            { 'o',  CP_STAT,  "G-class star" },
            { '+',  CP_STAT,  "K-class star" },
            { '*',  CP_WARN,  "M-class star" },
            { '.',  CP_DIM,   "far/dim star" },
        };
        for (int i = 0; i < 8; i++) {
            attron(COLOR_PAIR(leg[i].cp) | (leg[i].cp==CP_STAR ? A_BOLD : 0));
            mvaddch(ly+1+i, PX+2, (chtype)leg[i].sym);
            attroff(A_COLOR);
            attron(COLOR_PAIR(CP_MSG));
            mvprintw(ly+1+i, PX+4, "%-14.14s", leg[i].desc);
            attroff(COLOR_PAIR(CP_MSG));
        }
        attron(COLOR_PAIR(CP_DIM));
        mvprintw(ly+9,  PX+1, "Bright=unvisited");
        mvprintw(ly+10, PX+1, "Dim=visited");
        attroff(COLOR_PAIR(CP_DIM));
    }

    draw_log();
    draw_controls(
        "[W/A/S/D] or arrow keys: Move ship  [ENTER] Orbit star  [P] Return to port",
        "[A]=Arth (home)   Each move costs 2 Endurium fuel"
    );
}

static void handle_hyperspace(int ch) {
    int nx = ship.x, ny = ship.y;
    switch (ch) {
    case 'w': case 'W': case KEY_UP:    ny--; break;
    case 's': case 'S': case KEY_DOWN:  ny++; break;
    case 'a': case 'A': case KEY_LEFT:  nx--; break;
    case 'd': case 'D': case KEY_RIGHT: nx++; break;
    case 'p': case 'P':
        ship.x = gal[0].x; ship.y = gal[0].y;
        gs = GS_STARPORT; spsel = 0;
        logmsg("Returned to Arth Starport.");
        return;
    case '\n': case '\r': {
        int si = star_at(ship.x, ship.y);
        if (si < 0) {
            /* check adjacent */
            int adx[] = {0,0,-1,1};
            int ady[] = {-1,1,0,0};
            for (int d = 0; d < 4 && si < 0; d++)
                si = star_at(ship.x+adx[d], ship.y+ady[d]);
        }
        if (si >= 0) {
            cstar = si;
            gal[si].visited = 1;
            orbsel = 0;
            gs = GS_ORBIT;
            logmsg("Entering orbit around %s.", gal[si].name);
            dlog("ORBIT: star=%d (%s) np=%d alien=%d", si, gal[si].name, gal[si].np, gal[si].alien);
            if (gal[si].alien) {
                emood  = gal[si].mood;
                ealien = gal[si].atype;
                gs     = GS_ENC;
                logmsg("Alien vessel detected! %s", AG[(int)ealien]);
            }
        } else {
            logmsg("No star system here. Move closer.");
        }
        return;
    }
    default: return;
    }

    nx = clamp(nx, 0, GW-1);
    ny = clamp(ny, 0, GH-1);
    if (ship.fuel < FUEL_MOVE) {
        logmsg("Out of fuel! Return to starport.");
        return;
    }
    ship.x    = nx; ship.y = ny;
    ship.fuel = clamp(ship.fuel - FUEL_MOVE, 0, ship.mfuel);

    /* Auto-enter if on a star */
    int si = star_at(ship.x, ship.y);
    if (si >= 0 && !gal[si].home) {
        cstar = si;
        gal[si].visited = 1;
        logmsg("Approaching %s. Press ENTER to orbit.", gal[si].name);
    } else if (si == 0) {
        gs = GS_STARPORT; spsel = 0;
        logmsg("Welcome back to Arth.");
    }
}

/* ════════════════════ ORBIT ═════════════════════════════════ */
static void draw_orbit(void) {
    erase();
    draw_status();
    char title[40];
    snprintf(title, 40, "ORBIT -- %s", gal[cstar].name);
    draw_box(MY, MX, MH, MW, title);

    /* Simple planetary system diagram */
    int cx2 = MX + MW/2;
    int cy2 = MY + MH/2;

    /* Star */
    attron(COLOR_PAIR(CP_STAR)|A_BOLD);
    mvaddch(cy2, cx2, '@');
    attroff(COLOR_PAIR(CP_STAR)|A_BOLD);

    /* Planets in orbit */
    for (int p = 0; p < gal[cstar].np; p++) {
        int orbit_r = 4 + p * 3;
        int px2 = cx2 + orbit_r;
        if (px2 < MX + MW - 1) {
            char marker = (p == orbsel) ? 'O' : PC[(int)gal[cstar].planets[p]];
            int col = (p == orbsel) ? CP_WARN : CP_SURF;
            attron(COLOR_PAIR(col)|(p==orbsel ? A_BOLD : 0));
            mvaddch(cy2, px2, (chtype)marker);
            attroff(COLOR_PAIR(col)|(p==orbsel ? A_BOLD : 0));
            /* orbit ring (partial) */
            attron(COLOR_PAIR(CP_DIM));
            for (int dx = -orbit_r; dx <= orbit_r; dx++) {
                int ox = cx2 + dx;
                if (ox > MX && ox < MX+MW-1) mvaddch(cy2+1, ox, '-');
                if (ox > MX && ox < MX+MW-1) mvaddch(cy2-1, ox, '-');
            }
            attroff(COLOR_PAIR(CP_DIM));
        }
        /* Planet details */
        if (p == orbsel) {
            attron(COLOR_PAIR(CP_MENU)|A_BOLD);
            mvprintw(MY+MH-5, MX+3, "Selected: Planet %d -- %s",
                     p+1, PT[(int)gal[cstar].planets[p]]);
            attroff(COLOR_PAIR(CP_MENU)|A_BOLD);
        }
    }

    attron(COLOR_PAIR(CP_MSG));
    mvprintw(MY+MH-4, MX+3, "Star: %-18.18s  Class: %c  Planets: %d",
             gal[cstar].name, "OBAFGK"[gal[cstar].cls], gal[cstar].np);
    mvprintw(MY+MH-3, MX+3, "Planet %d/%d -- %s terrain",
             orbsel+1, gal[cstar].np, PT[(int)gal[cstar].planets[orbsel]]);
    attroff(COLOR_PAIR(CP_MSG));

    draw_right_panel();
    draw_log();
    draw_controls(
        "[</,] Prev planet   [>/.]  Next planet   [L] Land   [H] Hail aliens",
        "[ENTER/L] Land on planet   [ESC/Q] Leave orbit"
    );
}

static void handle_orbit(int ch) {
    switch (ch) {
    case ',': case '<': case KEY_LEFT: case KEY_UP:
        orbsel = (orbsel > 0) ? orbsel - 1 : gal[cstar].np - 1;
        break;
    case '.': case '>': case KEY_RIGHT: case KEY_DOWN:
        orbsel = (orbsel + 1) % gal[cstar].np;
        break;
    case 'l': case 'L': case '\n': case '\r':
        if (ship.fuel < FUEL_LAND) {
            logmsg("Not enough fuel to land.");
            break;
        }
        cpla = orbsel;
        gen_surface(cstar, cpla);
        ship.fuel -= FUEL_LAND;
        gs = GS_SURFACE;
        dlog("LAND: star=%d planet=%d type=%s", cstar, cpla, PT[(int)gal[cstar].planets[cpla]]);
        logmsg("Descending to %s -- %s terrain.",
               gal[cstar].name, PT[(int)gal[cstar].planets[cpla]]);
        break;
    case 'h': case 'H':
        if (gal[cstar].alien) {
            emood  = gal[cstar].mood;
            ealien = gal[cstar].atype;
            gs     = GS_ENC;
            logmsg("Opening comm channel...");
            logmsg("%s", AG[(int)ealien]);
        } else {
            logmsg("No alien vessels detected in this system.");
        }
        break;
    case 27: case 'q': case 'Q':
        gs = GS_HYPER;
        logmsg("Leaving orbit of %s.", gal[cstar].name);
        break;
    }
}

/* ════════════════════ SURFACE ═══════════════════════════════ */
static void draw_surface(void) {
    erase();
    draw_status();
    char title[64];
    snprintf(title, 64, "SURFACE -- %.20s / Planet %d",
             gal[cstar].name, cpla+1);
    draw_box(MY, MX, MH, MW, title);

    /* Terrain map */
    for (int y = 0; y < SH && y < MH-4; y++) {
        for (int x = 0; x < SW && x < MW-2; x++) {
            int sy2 = MY + 1 + y;
            int sx  = MX + 1 + x;
            if (y == surf.ty && x == surf.tx) {
                attron(COLOR_PAIR(CP_TV)|A_BOLD);
                mvaddch(sy2, sx, 'T');
                attroff(COLOR_PAIR(CP_TV)|A_BOLD);
            } else if (surf.min[y][x] >= 0) {
                attron(COLOR_PAIR(CP_WARN)|A_BOLD);
                mvaddch(sy2, sx, '*');
                attroff(COLOR_PAIR(CP_WARN)|A_BOLD);
            } else {
                attron(COLOR_PAIR(CP_SURF));
                mvaddch(sy2, sx, (chtype)surf.map[y][x]);
                attroff(COLOR_PAIR(CP_SURF));
            }
        }
    }

    /* Planet stats */
    static const char *atm_str[] = {"None","Thin","Breathable","Toxic"};
    static const char *grav_str[]= {"Low","Normal","High"};
    static const char *temp_str[]= {"Frozen","Cold","Temperate","Warm","Hot"};
    attron(COLOR_PAIR(CP_MSG));
    mvprintw(MY+MH-3, MX+2,
        "Atm:%-11.11s  Grav:%-7.7s  Temp:%-9.9s  TV@(%d,%d)",
        atm_str[surf.atm], grav_str[surf.grav],
        temp_str[surf.temp+2], surf.tx, surf.ty);
    attroff(COLOR_PAIR(CP_MSG));

    draw_right_panel();
    draw_log();
    draw_controls(
        "[W/A/S/D] Drive terrain vehicle   [M] Mine minerals at current tile",
        "[T] Scan area for minerals   [ESC/Q] Return to orbit"
    );
}

static void handle_surface(int ch) {
    int nx = surf.tx, ny = surf.ty;
    switch (ch) {
    case 'w': case 'W': case KEY_UP:    ny--; break;
    case 's': case 'S': case KEY_DOWN:  ny++; break;
    case 'a': case 'A': case KEY_LEFT:  nx--; break;
    case 'd': case 'D': case KEY_RIGHT: nx++; break;
    case 'm': case 'M': {
        int tx = surf.tx, ty2 = surf.ty;
        if (surf.min[ty2][tx] >= 0) {
            int mtype = surf.min[ty2][tx];
            int amt   = 1 + rand() % 5;
            if (ship.cused + amt > ship.ccap) {
                logmsg("Cargo hold full! Sell before mining more.");
                break;
            }
            starport_add_cargo(mtype, amt);
            surf.min[ty2][tx] = -1;
            logmsg("Mined %d units of %s.", amt, MN[mtype]);
        } else {
            logmsg("Nothing to mine here.");
        }
        return;
    }
    case 't': case 'T': {
        int found = 0;
        for (int y = 0; y < SH; y++)
            for (int x = 0; x < SW; x++)
                if (surf.min[y][x] >= 0) found++;
        logmsg("Science scan: %d mineral deposits detected.", found);
        return;
    }
    case 27: case 'q': case 'Q':
        gs = GS_ORBIT;
        logmsg("Terrain vehicle recalled. Returning to orbit.");
        return;
    default: return;
    }
    surf.tx = clamp(nx, 0, SW-1);
    surf.ty = clamp(ny, 0, SH-1);
}

/* ════════════════════ ENCOUNTER ═════════════════════════════ */
static void draw_encounter(void) {
    erase();
    draw_status();
    char title[40];
    snprintf(title, 40, "COMM CHANNEL -- %s", AN[(int)ealien]);
    draw_box(MY, MX, MH, MW, title);

    /* ASCII alien art */
    static const char *alien_art[6][5] = {
        {"  /\\_/\\  "," ( o.o ) ","  > ^ <  ","  |   |  ","  V   V  "},
        {" /\\___/\\ ","| [===] |","|_[|||]_|","  |   |  ","  |___|  "},
        {"  (~~~)  "," /|   |\\ ","| |   | |"," \\|   |/ ","  (___) "},
        {"  o   o  "," /_____\\ ","| SPEMIN |"," \\_____/ ","  |   |  "},
        {" /VVVVV\\ ","| GAZURT |","|_______| ","  \\ | /  ","   \\|/   "},
        {" [=====] ","| MECH-1 |","[|||||||||]","  |   |  ","  =====  "}
    };

    int a = (int)ealien;
    attron(COLOR_PAIR(CP_DANGER)|A_BOLD);
    for (int i = 0; i < 5; i++)
        mvprintw(MY+2+i, MX+4, "%s", alien_art[a][i]);
    attroff(COLOR_PAIR(CP_DANGER)|A_BOLD);

    /* Mood bar */
    attron(COLOR_PAIR(CP_MSG));
    mvprintw(MY+8, MX+3, "Alien Mood: [");
    int barlen = 30;
    int filled = (emood * barlen) / 100;
    for (int b = 0; b < barlen; b++) {
        if (b < filled) {
            attron(b < 10 ? COLOR_PAIR(CP_DANGER) :
                   b < 20 ? COLOR_PAIR(CP_WARN) :
                             COLOR_PAIR(CP_GOOD));
            mvaddch(MY+8, MX+16+b, '|');
        } else {
            attron(COLOR_PAIR(CP_DIM));
            mvaddch(MY+8, MX+16+b, ' ');
        }
        attroff(A_COLOR);
    }
    attron(COLOR_PAIR(CP_MSG));
    mvprintw(MY+8, MX+16+barlen, "] %d/100  (%s)",
             emood, emood > 60 ? "Friendly" : emood > 30 ? "Cautious" : "Hostile");

    mvprintw(MY+10, MX+3, "%s", AG[(int)ealien]);

    /* Choice list with cursor */
    static const char *enc_opts[] = {
        "1. Friendly tone  (+15 mood)",
        "2. Neutral tone   (no change)",
        "3. Hostile tone   (-25 mood)",
        "---",
        "F. Trade offer",
        "G. Request info",
        "H. Greet",
        "C. Attack!",
        "ESC. Break off comm"
    };
    /* 8 selectable items: skip separator at index 3 */
    int sel_rows[] = {0,1,2,4,5,6,7,8}; /* enc_opts indices for enc_sel 0-7 */
    attron(COLOR_PAIR(CP_MSG));
    mvprintw(MY+12, MX+3, "TONE:                      ACTION:");
    attroff(COLOR_PAIR(CP_MSG));
    for (int i = 0; i < 8; i++) {
        int oi  = sel_rows[i];
        int hl  = (i == enc_sel);
        int row = (i < 3) ? MY+13+i : MY+13+(i-3);
        int col = (i < 3) ? MX+5 : MX+31;
        attron(COLOR_PAIR(hl ? CP_WARN : CP_MSG) | (hl ? A_BOLD : 0));
        mvprintw(row, col, "%s%s", hl ? "> " : "  ", enc_opts[oi]);
        attroff(COLOR_PAIR(hl ? CP_WARN : CP_MSG) | (hl ? A_BOLD : 0));
    }
    attroff(COLOR_PAIR(CP_MSG));

    draw_right_panel();
    draw_log();
    draw_controls(
        "[UP/DN] Move cursor   [ENTER] Confirm   or press key directly",
        "[1-3] Tone   [F]Trade [G]Info [H]Greet [C]Attack [ESC]Break off"
    );
}

static void handle_encounter(int ch) {
    /* enc_sel 0-2=tone, 3-5=msg, 6=attack, 7=break */
    static const char enc_map[] = {'1','2','3','f','g','h','c',27};
    if (ch == KEY_UP)   { enc_sel = (enc_sel - 1 + 8) % 8; return; }
    if (ch == KEY_DOWN) { enc_sel = (enc_sel + 1) % 8;     return; }
    if (ch == '\n' || ch == '\r') { handle_encounter(enc_map[enc_sel]); return; }
    /* Direct key presses also update enc_sel */
    if (ch=='1') enc_sel=0; else if(ch=='2') enc_sel=1; else if(ch=='3') enc_sel=2;
    else if(ch=='f'||ch=='F') enc_sel=3; else if(ch=='g'||ch=='G') enc_sel=4;
    else if(ch=='h'||ch=='H') enc_sel=5; else if(ch=='c'||ch=='C') enc_sel=6;
    switch (ch) {
    case '1':
        emood = clamp(emood + 15, 0, 100);
        gal[cstar].mood = emood;
        logmsg("Friendly tone: mood now %d.", emood);
        break;
    case '2':
        logmsg("Neutral tone: mood unchanged at %d.", emood);
        break;
    case '3':
        emood = clamp(emood - 25, 0, 100);
        gal[cstar].mood = emood;
        logmsg("Hostile tone: mood now %d.", emood);
        if (emood < 20) {
            logmsg("Aliens are ATTACKING!");
            goto combat;
        }
        break;
    case 'f': case 'F':
        if (emood > 50) {
            int bonus = 100 + rand() % 200;
            ship.ru += bonus;
            logmsg("Trade deal! Received %d RU.", bonus);
        } else {
            logmsg("They are not interested in trade.");
        }
        break;
    case 'g': case 'G':
        if (emood > 40)
            logmsg("%s reveals star charts -- visit Epsilon Eri for Endurium.", AN[ealien]);
        else
            logmsg("They refuse to share information.");
        break;
    case 'h': case 'H':
        emood = clamp(emood + 5, 0, 100);
        logmsg("Greetings exchanged. Mood: %d.", emood);
        break;
    case 'c': case 'C':
        logmsg("Opening fire!");
        goto combat;
    case 27: case 'q': case 'Q':
        gs = GS_ORBIT;
        logmsg("Comm channel closed.");
        break;
    default: return;
    }
    return;

combat:
    ehull   = 40 + rand() % 60;
    eshld   = 20 + rand() % 40;
    strncpy(ename, AN[(int)ealien], 27);
    memset(clog, 0, sizeof(clog));
    combat_log_idx = 0;
    gs = GS_COMBAT;
    dlog("COMBAT: vs %s ehull=%d eshld=%d ship_weap=%d", ename, ehull, eshld, ship.weap);
}

/* ════════════════════ COMBAT ════════════════════════════════ */
static void clog_add(const char *fmt, ...) {
    va_list ap; va_start(ap, fmt);
    vsnprintf(clog[combat_log_idx % 4], 80, fmt, ap);
    va_end(ap);
    combat_log_idx++;
}

static void draw_stat_bar(int y, int x, int val, int max, int cp, const char *lbl) {
    mvprintw(y, x, "%s", lbl);
    int bw = 20;
    int filled = max > 0 ? (val * bw) / max : 0;
    mvaddch(y, x+8, '[');
    for (int b = 0; b < bw; b++) {
        attron(b < filled ? COLOR_PAIR(cp) : COLOR_PAIR(CP_DIM));
        mvaddch(y, x+9+b, b < filled ? '#' : ' ');
    }
    attroff(A_COLOR);
    mvprintw(y, x+9+bw, "] %d/%d", val, max);
}

static void draw_combat(void) {
    erase();
    draw_status();
    char title[40];
    snprintf(title, 40, "COMBAT -- vs %.20s", ename);
    draw_box(MY, MX, MH, MW, title);

    /* Ship diagram */
    attron(COLOR_PAIR(CP_SHIP)|A_BOLD);
    mvprintw(MY+2,  MX+4, "YOUR VESSEL:");
    mvprintw(MY+3,  MX+4, "  [=====|>");
    attroff(COLOR_PAIR(CP_SHIP)|A_BOLD);

    attron(COLOR_PAIR(CP_DANGER)|A_BOLD);
    mvprintw(MY+2, MX+34, "ENEMY (%s):", ename);
    mvprintw(MY+3, MX+34, " <|====]");
    attroff(COLOR_PAIR(CP_DANGER)|A_BOLD);

    draw_stat_bar(MY+5, MX+4,  ship.hull, ship.mhull, CP_GOOD, "Hull:  ");
    draw_stat_bar(MY+6, MX+4,  ship.shld, ship.mshld, CP_WARN, "Shield:");
    draw_stat_bar(MY+5, MX+34, ehull, 100,             CP_GOOD, "Hull:  ");
    draw_stat_bar(MY+6, MX+34, eshld, 60,              CP_WARN, "Shield:");

    /* Weapon/engine info */
    attron(COLOR_PAIR(CP_STAT));
    mvprintw(MY+8, MX+4, "Weapons: Level %d   Engines: Level %d",
             ship.weap, ship.eng);
    attroff(COLOR_PAIR(CP_STAT));

    /* Combat log */
    attron(COLOR_PAIR(CP_MSG));
    mvprintw(MY+10, MX+4, "BATTLE LOG:");
    for (int i = 0; i < 4; i++) {
        int idx = (combat_log_idx - 4 + i + 4) % 4;
        if (clog[idx][0])
            mvprintw(MY+11+i, MX+6, "> %s", clog[idx]);
    }
    attroff(COLOR_PAIR(CP_MSG));

    /* Actions with cursor */
    static const char *cbt_opts[] = {
        "1. Fire weapons",
        "2. Boost shields",
        "3. Evasive maneuver",
        "4. Retreat"
    };
    for (int i = 0; i < 4; i++) {
        int hl = (i == cbt_sel);
        attron(COLOR_PAIR(hl ? CP_WARN : CP_MENU) | (hl ? A_BOLD : 0));
        mvprintw(MY+16+(i/2), MX+4+(i%2)*26, "%s%s", hl ? "> " : "  ", cbt_opts[i]);
        attroff(COLOR_PAIR(hl ? CP_WARN : CP_MENU) | (hl ? A_BOLD : 0));
    }

    draw_right_panel();
    draw_log();
    draw_controls(
        "[1] Fire weapons   [2] Boost shields   [3] Evade   [4] Retreat",
        "Defeat enemy to gain RU bonus. Retreat returns to orbit."
    );
}

static void combat_enemy_attack(void) {
    if (ehull <= 0) return;
    int hit = rand() % 2;
    if (!hit) { clog_add("Enemy misses!"); return; }
    int dmg = 5 + rand() % (10 + (int)ealien * 2);
    if (ship.shld > 0) {
        int absorbed = clamp(dmg, 0, ship.shld);
        ship.shld = clamp(ship.shld - absorbed, 0, ship.mshld);
        dmg      -= absorbed;
    }
    if (dmg > 0) {
        ship.hull = clamp(ship.hull - dmg, 0, ship.mhull);
        clog_add("Enemy hits hull for %d damage!", dmg);
    } else {
        clog_add("Shields absorbed enemy fire.");
    }
}

static void handle_combat(int ch) {
    if (ch == KEY_UP)   { cbt_sel = (cbt_sel - 1 + 4) % 4; return; }
    if (ch == KEY_DOWN) { cbt_sel = (cbt_sel + 1) % 4;     return; }
    if (ch == '\n' || ch == '\r') { handle_combat('1' + cbt_sel); return; }
    if (ch>='1'&&ch<='4') cbt_sel = ch-'1';
    switch (ch) {
    case '1': {
        int hit = rand() % 2;
        if (hit) {
            int dmg = 8 + ship.weap * 5 + rand() % 10;
            if (eshld > 0) {
                int abs = clamp(dmg, 0, eshld);
                eshld -= abs; dmg -= abs;
            }
            ehull = clamp(ehull - dmg, 0, 100);
            clog_add("Direct hit! Enemy hull at %d.", ehull);
        } else {
            clog_add("Weapons fire missed the enemy.");
        }
        if (ehull <= 0) {
            int prize = 200 + rand() % 300;
            ship.ru += prize;
            logmsg("Enemy destroyed! Recovered %d RU from debris.", prize);
            logmsg("Victory over %s!", ename);
            gs = GS_ORBIT;
            return;
        }
        combat_enemy_attack();
        break;
    }
    case '2':
        ship.shld = clamp(ship.shld + 20, 0, ship.mshld);
        clog_add("Shield boosted to %d.", ship.shld);
        combat_enemy_attack();
        break;
    case '3': {
        int evade = rand() % 100 < 40 + ship.eng * 10;
        if (evade) {
            clog_add("Evasive maneuver successful! Enemy missed.");
        } else {
            clog_add("Evasion failed!");
            combat_enemy_attack();
        }
        break;
    }
    case '4': case 27:
        logmsg("Retreating from combat...");
        gs = GS_ORBIT;
        return;
    default: return;
    }

    if (ship.hull <= 0) {
        logmsg("HULL BREACHED! Ship destroyed. Game over.");
        logmsg("Final RU: %d  Stars visited: %d", ship.ru,
               (int)(NS/10));
        gs = GS_QUIT;
    }
}

/* ════════════════════ MAIN DRAW DISPATCH ════════════════════ */
static void draw_all(void) {
    switch (gs) {
    case GS_TITLE:    draw_title();     break;
    case GS_STARPORT: draw_starport();  break;
    case GS_HYPER:    draw_hyperspace();break;
    case GS_ORBIT:    draw_orbit();     break;
    case GS_SURFACE:  draw_surface();   break;
    case GS_ENC:      draw_encounter(); break;
    case GS_COMBAT:   draw_combat();    break;
    case GS_QUIT:     break;
    }
    refresh();
}

static void handle_input(int ch) {
    switch (gs) {
    case GS_TITLE:
        if (ch == '\n' || ch == '\r' || ch == ' ') gs = GS_STARPORT;
        else if (ch == 'q' || ch == 'Q') gs = GS_QUIT;
        break;
    case GS_STARPORT: handle_starport(ch);  break;
    case GS_HYPER:    handle_hyperspace(ch);break;
    case GS_ORBIT:    handle_orbit(ch);     break;
    case GS_SURFACE:  handle_surface(ch);   break;
    case GS_ENC:      handle_encounter(ch); break;
    case GS_COMBAT:   handle_combat(ch);    break;
    case GS_QUIT:     break;
    }
}

/* ════════════════════ MAIN ══════════════════════════════════ */
int main(void) {
    srand((unsigned)time(NULL));

    /* Open debug log */
    logfp = fopen("log.txt", "w");
    if (logfp) dlog("=== STARFLIGHT log started ===");

    initscr();
    cbreak();
    noecho();
    keypad(stdscr, TRUE);
    curs_set(0);

    if (!has_colors()) {
        endwin();
        fprintf(stderr, "This game requires a colour terminal.\n");
        return 1;
    }
    setup_colors();

    int rows = getmaxy(stdscr);
    int cols = getmaxx(stdscr);
    if (rows < 30 || cols < 80) {
        endwin();
        fprintf(stderr, "Terminal too small. Need at least 80x30 (got %dx%d).\n",
                cols, rows);
        return 1;
    }

    init_galaxy();
    init_ship();
    make_hire_pool();

    dlog("Galaxy initialised: %d stars, terminal %dx%d", NS, cols, rows);
    dlog("Ship: hull=%d shld=%d fuel=%d ru=%d", ship.hull, ship.shld, ship.fuel, ship.ru);

    gs = GS_TITLE;
    logmsg("Year 4620 -- The people of Arth rediscover spaceflight.");
    logmsg("Hire your crew, outfit your ship, explore the galaxy.");
    logmsg("Find the source of the stellar flares. Save your world.");

    while (gs != GS_QUIT) {
        draw_all();
        int ch = getch();
        if (ch == 'Q' && gs != GS_STARPORT) {
            /* Global quit via Q from any non-starport screen */
        }
        handle_input(ch);
    }

    endwin();
    printf("\n  STARFLIGHT -- ASCII Terminal Edition\n");
    printf("  Final score: %d RU | Hull: %d%%\n\n", ship.ru, ship.hull);

    if (logfp) {
        int vis = 0;
        for (int i = 0; i < NS; i++) vis += gal[i].visited;
        dlog("=== Game ended. RU=%d Hull=%d Stars_visited=%d ===",
             ship.ru, ship.hull, vis);
        fclose(logfp);
    }
    return 0;
}
