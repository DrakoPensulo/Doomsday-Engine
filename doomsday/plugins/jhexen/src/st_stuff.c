/**\file
 *\section License
 * License: GPL
 * Online License Link: http://www.gnu.org/licenses/gpl.html
 *
 *\author Copyright © 2003-2008 Jaakko Keränen <jaakko.keranen@iki.fi>
 *\author Copyright © 2005-2008 Daniel Swanson <danij@dengine.net>
 *\author Copyright © 1999 Activision
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor,
 * Boston, MA  02110-1301  USA
 */

/**
 * st_stuff.c:
 */

// HEADER FILES ------------------------------------------------------------

#include "jhexen.h"
#include "st_lib.h"
#include "d_net.h"

#include "p_tick.h" // for P_IsPaused
#include "am_map.h"
#include "g_common.h"

// MACROS ------------------------------------------------------------------

// inventory
#define ST_INVENTORYX           50
#define ST_INVENTORYY           163

// how many inventory slots are visible
#define NUMVISINVSLOTS 7

// invslot artifact count (relative to each slot)
#define ST_INVCOUNTOFFX         30
#define ST_INVCOUNTOFFY         22

// current artifact (sbbar)
#define ST_ARTIFACTWIDTH        24
#define ST_ARTIFACTX            143
#define ST_ARTIFACTY            163

// current artifact count (sbar)
#define ST_ARTIFACTCWIDTH       2
#define ST_ARTIFACTCX           174
#define ST_ARTIFACTCY           184

// HEALTH number pos.
#define ST_HEALTHWIDTH          3
#define ST_HEALTHX              64
#define ST_HEALTHY              176

// MANA A
#define ST_MANAAWIDTH           3
#define ST_MANAAX               91
#define ST_MANAAY               181

// MANA A ICON
#define ST_MANAAICONX           77
#define ST_MANAAICONY           164

// MANA A VIAL
#define ST_MANAAVIALX           94
#define ST_MANAAVIALY           164

// MANA B
#define ST_MANABWIDTH           3
#define ST_MANABX               123
#define ST_MANABY               181

// MANA B ICON
#define ST_MANABICONX           110
#define ST_MANABICONY           164

// MANA B VIAL
#define ST_MANABVIALX           102
#define ST_MANABVIALY           164

// ARMOR number pos.
#define ST_ARMORWIDTH           2
#define ST_ARMORX               274
#define ST_ARMORY               176

// Frags pos.
#define ST_FRAGSWIDTH           3
#define ST_FRAGSX               64
#define ST_FRAGSY               176

// TYPES -------------------------------------------------------------------

typedef struct {
    boolean         stopped;
    int             hideTics;
    float           hideAmount;
    float           alpha; // Fullscreen hud alpha value.

    float           showbar; // Slide statusbar amount 1.0 is fully open.
    float           statusbarCounterAlpha;

    boolean         firstTime; // ST_Start() has just been called.
    boolean         blended; // Whether to use alpha blending.
    boolean         statusbaron; // Whether left-side main status bar is active.
    boolean         fragson; // !deathmatch.
    int             fragscount; // Number of frags so far in deathmatch.

    int             oldhealth;
    int             healthMarker;
    int             oldarti;
    int             oldartiCount;

    int             artifactFlash;
    int             invslot[NUMVISINVSLOTS]; // Current inventory slot indices. 0 = none.
    int             invslotcount[NUMVISINVSLOTS]; // Current inventory slot count indices. 0 = none.
    int             armorlevel; // Current armor level.
    int             artici; // Current artifact index. 0 = none.
    int             manaAicon; // Current mana A icon index. 0 = none.
    int             manaBicon; // Current mana B icon index. 0 = none.
    int             manaAvial; // Current mana A vial index. 0 = none.
    int             manaBvial; // Current mana B vial index. 0 = none.
    int             manaACount;
    int             manaBCount;

    int             inventoryTics;
    boolean         inventory;
    boolean         hitCenterFrame;

    // Widgets:
    st_multicon_t   wArti; // Current artifact icon.
    st_number_t     wArtiCount; // Current artifact count.
    st_multicon_t   wInvSlot[NUMVISINVSLOTS]; // Inventory slot icons.
    st_number_t     wInvSlotCount[NUMVISINVSLOTS]; // Inventory slot counts.
    st_multicon_t   wManaA; // Current mana A icon.
    st_multicon_t   wManaB; // Current mana B icon.
    st_number_t     wManaACount; // Current mana A count.
    st_number_t     wManaBCount; // Current mana B count.
    st_multicon_t   wManaAVial; // Current mana A vial.
    st_multicon_t   wManaBVial; // Current mana B vial.
    st_number_t     wHealth; // Health.
    st_number_t     wFrags; // In deathmatch only, summary of frags stats.
    st_number_t     wArmor; // Armor.
} hudstate_t;

// EXTERNAL FUNCTION PROTOTYPES --------------------------------------------

// PUBLIC FUNCTION PROTOTYPES ----------------------------------------------

// Console commands for the HUD/Statusbar
DEFCC(CCmdStatusBarSize);

// PRIVATE FUNCTION PROTOTYPES ---------------------------------------------

static void DrINumber(signed int val, int x, int y, float r, float g,
                      float b, float a);
static void DrBNumber(signed int val, int x, int y, float Red, float Green,
                      float Blue, float Alpha);

// EXTERNAL DATA DECLARATIONS ----------------------------------------------

// PUBLIC DATA DECLARATIONS ------------------------------------------------

// PRIVATE DATA DEFINITIONS ------------------------------------------------

static hudstate_t hudStates[MAXPLAYERS];

static int fontBNumBase;

static dpatch_t PatchNumH2BAR;
static dpatch_t PatchNumH2TOP;
static dpatch_t PatchNumLFEDGE;
static dpatch_t PatchNumRTEDGE;
static dpatch_t PatchNumKILLS;
static dpatch_t PatchNumSTATBAR;
static dpatch_t PatchNumKEYBAR;
static dpatch_t PatchNumSELECTBOX;
static dpatch_t PatchNumINumbers[10];
static dpatch_t PatchNumNEGATIVE;
static dpatch_t PatchNumSmNumbers[10];
static dpatch_t PatchMANAAVIALS[2];
static dpatch_t PatchMANABVIALS[2];
static dpatch_t PatchMANAAICONS[2];
static dpatch_t PatchMANABICONS[2];
static dpatch_t PatchNumINVBAR;
static dpatch_t PatchNumWEAPONSLOT[3]; // [Fighter, Cleric, Mage]
static dpatch_t PatchNumWEAPONFULL[3]; // [Fighter, Cleric, Mage]
static dpatch_t PatchNumLifeGem[3][8]; // [Fighter, Cleric, Mage][color]
static dpatch_t PatchNumPIECE1[3]; // [Fighter, Cleric, Mage]
static dpatch_t PatchNumPIECE2[3]; // [Fighter, Cleric, Mage]
static dpatch_t PatchNumPIECE3[3]; // [Fighter, Cleric, Mage]
static dpatch_t PatchNumCHAIN[3]; // [Fighter, Cleric, Mage]
static dpatch_t PatchNumINVLFGEM1;
static dpatch_t PatchNumINVLFGEM2;
static dpatch_t PatchNumINVRTGEM1;
static dpatch_t PatchNumINVRTGEM2;
static dpatch_t PatchARTIFACTS[38];
static dpatch_t SpinFlylump;
static dpatch_t SpinMinotaurLump;
static dpatch_t SpinSpeedLump;
static dpatch_t SpinDefenseLump;

char    artifactlist[][10] = {
    {"USEARTIA"},               // use artifact flash
    {"USEARTIB"},
    {"USEARTIC"},
    {"USEARTID"},
    {"USEARTIE"},
    {"ARTIBOX"},                // none
    {"ARTIINVU"},               // invulnerability
    {"ARTIPTN2"},               // health
    {"ARTISPHL"},               // superhealth
    {"ARTIHRAD"},               // healing radius
    {"ARTISUMN"},               // summon maulator
    {"ARTITRCH"},               // torch
    {"ARTIPORK"},               // egg
    {"ARTISOAR"},               // fly
    {"ARTIBLST"},               // blast radius
    {"ARTIPSBG"},               // poison bag
    {"ARTITELO"},               // teleport other
    {"ARTISPED"},               // speed
    {"ARTIBMAN"},               // boost mana
    {"ARTIBRAC"},               // boost armor
    {"ARTIATLP"},               // teleport
    {"ARTISKLL"},               // AFT_PUZZSKULL
    {"ARTIBGEM"},               // AFT_PUZZGEMBIG
    {"ARTIGEMR"},               // AFT_PUZZGEMRED
    {"ARTIGEMG"},               // AFT_PUZZGEMGREEN1
    {"ARTIGMG2"},               // AFT_PUZZGEMGREEN2
    {"ARTIGEMB"},               // AFT_PUZZGEMBLUE1
    {"ARTIGMB2"},               // AFT_PUZZGEMBLUE2
    {"ARTIBOK1"},               // AFT_PUZZBOOK1
    {"ARTIBOK2"},               // AFT_PUZZBOOK2
    {"ARTISKL2"},               // AFT_PUZZSKULL2
    {"ARTIFWEP"},               // AFT_PUZZFWEAPON
    {"ARTICWEP"},               // AFT_PUZZCWEAPON
    {"ARTIMWEP"},               // AFT_PUZZMWEAPON
    {"ARTIGEAR"},               // AFT_PUZZGEAR1
    {"ARTIGER2"},               // AFT_PUZZGEAR2
    {"ARTIGER3"},               // AFT_PUZZGEAR3
    {"ARTIGER4"},               // AFT_PUZZGEAR4
};

// CVARs for the HUD/Statusbar
cvar_t sthudCVars[] =
{
    // HUD scale
    {"hud-scale", 0, CVT_FLOAT, &cfg.hudScale, 0.1f, 10},

    {"hud-status-size", CVF_PROTECTED, CVT_INT, &cfg.statusbarScale, 1, 20},

    // HUD colour + alpha
    {"hud-color-r", 0, CVT_FLOAT, &cfg.hudColor[0], 0, 1},
    {"hud-color-g", 0, CVT_FLOAT, &cfg.hudColor[1], 0, 1},
    {"hud-color-b", 0, CVT_FLOAT, &cfg.hudColor[2], 0, 1},
    {"hud-color-a", 0, CVT_FLOAT, &cfg.hudColor[3], 0, 1},
    {"hud-icon-alpha", 0, CVT_FLOAT, &cfg.hudIconAlpha, 0, 1},

    {"hud-status-alpha", 0, CVT_FLOAT, &cfg.statusbarAlpha, 0, 1},
    {"hud-status-icon-a", 0, CVT_FLOAT, &cfg.statusbarCounterAlpha, 0, 1},

    // HUD icons
    {"hud-mana", 0, CVT_BYTE, &cfg.hudShown[HUD_MANA], 0, 2},
    {"hud-health", 0, CVT_BYTE, &cfg.hudShown[HUD_HEALTH], 0, 1},
    {"hud-artifact", 0, CVT_BYTE, &cfg.hudShown[HUD_ARTI], 0, 1},

    // HUD displays
    {"hud-inventory-timer", 0, CVT_FLOAT, &cfg.inventoryTimer, 0, 30},

    {"hud-timer", 0, CVT_FLOAT, &cfg.hudTimer, 0, 60},

    {"hud-unhide-damage", 0, CVT_BYTE, &cfg.hudUnHide[HUE_ON_DAMAGE], 0, 1},
    {"hud-unhide-pickup-health", 0, CVT_BYTE, &cfg.hudUnHide[HUE_ON_PICKUP_HEALTH], 0, 1},
    {"hud-unhide-pickup-armor", 0, CVT_BYTE, &cfg.hudUnHide[HUE_ON_PICKUP_ARMOR], 0, 1},
    {"hud-unhide-pickup-powerup", 0, CVT_BYTE, &cfg.hudUnHide[HUE_ON_PICKUP_POWER], 0, 1},
    {"hud-unhide-pickup-weapon", 0, CVT_BYTE, &cfg.hudUnHide[HUE_ON_PICKUP_WEAPON], 0, 1},
    {"hud-unhide-pickup-ammo", 0, CVT_BYTE, &cfg.hudUnHide[HUE_ON_PICKUP_AMMO], 0, 1},
    {"hud-unhide-pickup-key", 0, CVT_BYTE, &cfg.hudUnHide[HUE_ON_PICKUP_KEY], 0, 1},
    {"hud-unhide-pickup-invitem", 0, CVT_BYTE, &cfg.hudUnHide[HUE_ON_PICKUP_INVITEM], 0, 1},
    {NULL}
};

// Console commands for the HUD/Status bar
ccmd_t  sthudCCmds[] = {
    {"sbsize",      "s",    CCmdStatusBarSize},
    {NULL}
};

// CODE --------------------------------------------------------------------

/**
 * Register CVARs and CCmds for the HUD/Status bar.
 */
void ST_Register(void)
{
    int                 i;

    for(i = 0; sthudCVars[i].name; ++i)
        Con_AddVariable(sthudCVars + i);
    for(i = 0; sthudCCmds[i].name; ++i)
        Con_AddCommand(sthudCCmds + i);
}

static void drawAnimatedIcons(hudstate_t* hud)
{
    int             leftoff = 0;
    int             frame;
    float           iconalpha = (hud->statusbaron? 1: hud->alpha) - (1 - cfg.hudIconAlpha);
    int             player = hud - hudStates;
    player_t*       plr = &players[player];

    // If the fullscreen mana is drawn, we need to move the icons on the left
    // a bit to the right.
    if(cfg.hudShown[HUD_MANA] == 1 && cfg.screenBlocks > 10)
        leftoff = 42;

    Draw_BeginZoom(cfg.hudScale, 2, 2);

    // Wings of wrath
    if(plr->powers[PT_FLIGHT])
    {
        if(plr->powers[PT_FLIGHT] > BLINKTHRESHOLD ||
           !(plr->powers[PT_FLIGHT] & 16))
        {
            frame = (levelTime / 3) & 15;
            if(plr->plr->mo->flags2 & MF2_FLY)
            {
                if(hud->hitCenterFrame && (frame != 15 && frame != 0))
                {
                    GL_DrawPatchLitAlpha(20 + leftoff, 19, 1, iconalpha,
                                         SpinFlylump.lump + 15);
                }
                else
                {
                    GL_DrawPatchLitAlpha(20 + leftoff, 19, 1, iconalpha,
                                         SpinFlylump.lump + frame);
                    hud->hitCenterFrame = false;
                }
            }
            else
            {
                if(!hud->hitCenterFrame && (frame != 15 && frame != 0))
                {
                    GL_DrawPatchLitAlpha(20 + leftoff, 19, 1, iconalpha,
                                         SpinFlylump.lump + frame);
                    hud->hitCenterFrame = false;
                }
                else
                {
                    GL_DrawPatchLitAlpha(20 + leftoff, 19, 1, iconalpha,
                                         SpinFlylump.lump + 15);
                    hud->hitCenterFrame = true;
                }
            }
        }
    }

    // Speed Boots
    if(plr->powers[PT_SPEED])
    {
        if(plr->powers[PT_SPEED] > BLINKTHRESHOLD ||
           !(plr->powers[PT_SPEED] & 16))
        {
            frame = (levelTime / 3) & 15;
            GL_DrawPatchLitAlpha(60 + leftoff, 19, 1, iconalpha,
                                 SpinSpeedLump.lump + frame);
        }
    }

    Draw_EndZoom();

    Draw_BeginZoom(cfg.hudScale, 318, 2);

    // Defensive power
    if(plr->powers[PT_INVULNERABILITY])
    {
        if(plr->powers[PT_INVULNERABILITY] > BLINKTHRESHOLD ||
           !(plr->powers[PT_INVULNERABILITY] & 16))
        {
            frame = (levelTime / 3) & 15;
            GL_DrawPatchLitAlpha(260, 19, 1, iconalpha,
                                 SpinDefenseLump.lump + frame);
        }
    }

    // Minotaur Active
    if(plr->powers[PT_MINOTAUR])
    {
        if(plr->powers[PT_MINOTAUR] > BLINKTHRESHOLD ||
           !(plr->powers[PT_MINOTAUR] & 16))
        {
            frame = (levelTime / 3) & 15;
            GL_DrawPatchLitAlpha(300, 19, 1, iconalpha,
                                 SpinMinotaurLump.lump + frame);
        }
    }

    Draw_EndZoom();
}

static void drawKeyBar(hudstate_t* hud)
{
    int                 i, xPosition, temp, pClass;
    int                 player = hud - hudStates;
    player_t*           plr = &players[player];

    // Original player class (i.e. not pig).
    pClass = cfg.playerClass[player];

    xPosition = 46;
    for(i = 0; i < NUM_KEY_TYPES && xPosition <= 126; ++i)
    {
        if(plr->keys & (1 << i))
        {
            GL_DrawPatchLitAlpha(xPosition, 163, 1, hud->statusbarCounterAlpha,
                                 W_GetNumForName("keyslot1") + i);
            xPosition += 20;
        }
    }

    temp =
        PCLASS_INFO(pClass)->autoArmorSave + plr->armorPoints[ARMOR_ARMOR] +
        plr->armorPoints[ARMOR_SHIELD] +
        plr->armorPoints[ARMOR_HELMET] +
        plr->armorPoints[ARMOR_AMULET];

    for(i = 0; i < NUMARMOR; ++i)
    {
        if(!plr->armorPoints[i])
            continue;

        if(plr->armorPoints[i] <= (PCLASS_INFO(pClass)->armorIncrement[i] >> 2))
        {
            GL_DrawPatchLitAlpha(150 + 31 * i, 164, 1, hud->statusbarCounterAlpha * 0.3,
                             W_GetNumForName("armslot1") + i);
        }
        else if(plr->armorPoints[i] <=
                (PCLASS_INFO(pClass)->armorIncrement[i] >> 1))
        {
            GL_DrawPatchLitAlpha(150 + 31 * i, 164, 1, hud->statusbarCounterAlpha * 0.6,
                                W_GetNumForName("armslot1") + i);
        }
        else
        {
            GL_DrawPatchLitAlpha(150 + 31 * i, 164, 1, hud->statusbarCounterAlpha,
                                 W_GetNumForName("armslot1") + i);
        }
    }
}

static void drawWeaponPieces(hudstate_t* hud)
{
    int                 player = hud - hudStates, pClass;
    player_t*           plr = &players[player];
    float               alpha;

    // Original player class (i.e. not pig).
    pClass = cfg.playerClass[player];

    alpha = cfg.statusbarAlpha - hud->hideAmount;
    // Clamp
    CLAMP(alpha, 0.0f, 1.0f);

    GL_DrawPatchLitAlpha(190, 162, 1, alpha, PatchNumWEAPONSLOT[pClass].lump);

    if(plr->pieces == 7) // All pieces
        GL_DrawPatchLitAlpha(190, 162, 1, hud->statusbarCounterAlpha, PatchNumWEAPONFULL[pClass].lump);
    else
    {
        if(plr->pieces & WPIECE1)
        {
            GL_DrawPatchLitAlpha(PCLASS_INFO(pClass)->pieceX[0],
                                 162, 1, hud->statusbarCounterAlpha,
                                 PatchNumPIECE1[pClass].lump);
        }
        if(plr->pieces & WPIECE2)
        {
            GL_DrawPatchLitAlpha(PCLASS_INFO(pClass)->pieceX[1],
                                 162, 1, hud->statusbarCounterAlpha,
                                 PatchNumPIECE2[pClass].lump);
        }
        if(plr->pieces & WPIECE3)
        {
            GL_DrawPatchLitAlpha(PCLASS_INFO(pClass)->pieceX[2],
                                 162, 1, hud->statusbarCounterAlpha,
                                 PatchNumPIECE3[pClass].lump);
        }
    }
}

static void drawChain(hudstate_t* hud)
{
    int                 x, x2, y, w, w2, w3, h;
    int                 gemoffset = 36, pClass, pColor;
    float               cw, cw2;
    float               healthPos;
    float               gemglow;
    int                 player = hud - hudStates;
    player_t*           plr = &players[player];
    dpatch_t*           gemPatch;

    // Original player class (i.e. not pig).
    pClass = cfg.playerClass[player];
    pColor = cfg.playerColor[player];

    healthPos = hud->healthMarker;
    if(healthPos < 0)
        healthPos = 0;
    if(healthPos > 100)
        healthPos = 100;

    gemglow = healthPos / 100;

    // Draw the chain.
    x = 44;
    y = 193;
    w = 232;
    h = 7;
    cw = (healthPos / 113) + 0.054f;

    GL_SetPatch(PatchNumCHAIN[pClass].lump, DGL_REPEAT, DGL_CLAMP);

    DGL_Color4f(1, 1, 1, hud->statusbarCounterAlpha);

    DGL_Begin(DGL_QUADS);
        DGL_TexCoord2f( 0 - cw, 0);
        DGL_Vertex2f(x, y);

        DGL_TexCoord2f( 0.948f - cw, 0);
        DGL_Vertex2f(x + w, y);

        DGL_TexCoord2f( 0.948f - cw, 1);
        DGL_Vertex2f(x + w, y + h);

        DGL_TexCoord2f( 0 - cw, 1);
        DGL_Vertex2f(x, y + h);
    DGL_End();

    healthPos = ((healthPos * 256) / 117) - gemoffset;

    x = 44;
    y = 193;
    w2 = 86;
    h = 7;
    cw = 0;
    cw2 = 1;

    // calculate the size of the quad, position and tex coords
    if(x + healthPos < x)
    {
        x2 = x;
        w3 = w2 + healthPos;
        cw = (1.0f / w2) * (w2 - w3);
        cw2 = 1;
    }
    else if(x + healthPos + w2 > x + w)
    {
        x2 = x + healthPos;
        w3 = w2 - ((x + healthPos + w2) - (x + w));
        cw = 0;
        cw2 = (1.0f / w2) * (w2 - (w2 - w3));
    }
    else
    {
        x2 = x + healthPos;
        w3 = w2;
        cw = 0;
        cw2 = 1;
    }

    if(!IS_NETGAME)
    {   // Always use the red life gem (the second gem).
        gemPatch = &PatchNumLifeGem[pClass][1];
    }
    else
    {
        gemPatch = &PatchNumLifeGem[pClass][pColor];
    }

    GL_SetPatch(gemPatch->lump, DGL_CLAMP, DGL_CLAMP);

    // Draw the life gem.
    DGL_Color4f(1, 1, 1, hud->statusbarCounterAlpha);

    DGL_Begin(DGL_QUADS);
        DGL_TexCoord2f( cw, 0);
        DGL_Vertex2f(x2, y);

        DGL_TexCoord2f( cw2, 0);
        DGL_Vertex2f(x2 + w3, y);

        DGL_TexCoord2f( cw2, 1);
        DGL_Vertex2f(x2 + w3, y + h);

        DGL_TexCoord2f( cw, 1);
        DGL_Vertex2f(x2, y + h);
    DGL_End();

    // how about a glowing gem?
    GL_BlendMode(BM_ADD);
    DGL_Bind(Get(DD_DYNLIGHT_TEXTURE));

    GL_DrawRect(x + healthPos + 25, y - 3, 34, 18, 1, 0, 0,
                gemglow - (1 - hud->statusbarCounterAlpha));

    GL_BlendMode(BM_NORMAL);
}

static void drawStatusBarBackground(int player)
{
    int                 x, y, w, h;
    int                 pClass;
    float               cw, cw2, ch;
    hudstate_t*         hud = &hudStates[player];
    player_t*           plr = &players[player];
    float               alpha;

    // Original class (i.e. not pig).
    pClass = cfg.playerClass[player];

    if(hud->blended)
    {
        alpha = cfg.statusbarAlpha - hud->hideAmount;
        // Clamp
        CLAMP(alpha, 0.0f, 1.0f);
        if(!(alpha > 0))
            return;
    }
    else
        alpha = 1.0f;

    if(!(alpha < 1))
    {
        GL_DrawPatch(0, 134, PatchNumH2BAR.lump);
        GL_DrawPatch(0, 134, PatchNumH2TOP.lump);

        if(!hud->inventory)
        {
            // Main interface
            if(!AM_IsMapActive(player))
            {
                GL_DrawPatch(38, 162, PatchNumSTATBAR.lump);

                if(plr->pieces == 7)
                {
                    GL_DrawPatch(190, 162, PatchNumWEAPONFULL[pClass].lump);
                }
                else
                {
                    GL_DrawPatch(190, 162, PatchNumWEAPONSLOT[pClass].lump);
                }

                drawWeaponPieces(hud);
            }
            else
            {
                GL_DrawPatch(38, 162, PatchNumKEYBAR.lump);
                drawKeyBar(hud);
            }
        }
        else
        {
            GL_DrawPatch(38, 162, PatchNumINVBAR.lump);
        }

        drawChain(hud);
    }
    else
    {
        DGL_Color4f(1, 1, 1, alpha);

        GL_SetPatch(PatchNumH2BAR.lump, DGL_REPEAT, DGL_REPEAT);

        DGL_Begin(DGL_QUADS);

        // top
        x = 0;
        y = 135;
        w = ST_WIDTH;
        h = 27;
        ch = 0.41538461538461538461538461538462;

        DGL_TexCoord2f(0, 0);
        DGL_Vertex2f(x, y);
        DGL_TexCoord2f(1, 0);
        DGL_Vertex2f(x + w, y);
        DGL_TexCoord2f(1, ch);
        DGL_Vertex2f(x + w, y + h);
        DGL_TexCoord2f(0, ch);
        DGL_Vertex2f(x, y + h);

        // left statue
        x = 0;
        y = 162;
        w = 38;
        h = 38;
        cw = (float) 38 / ST_WIDTH;
        ch = 0.41538461538461538461538461538462;

        DGL_TexCoord2f(0, ch);
        DGL_Vertex2f(x, y);
        DGL_TexCoord2f(cw, ch);
        DGL_Vertex2f(x + w, y);
        DGL_TexCoord2f(cw, 1);
        DGL_Vertex2f(x + w, y + h);
        DGL_TexCoord2f(0, 1);
        DGL_Vertex2f(x, y + h);

        // right statue
        x = 282;
        y = 162;
        w = 38;
        h = 38;
        cw = (float) (ST_WIDTH - 38) / ST_WIDTH;
        ch = 0.41538461538461538461538461538462f;

        DGL_TexCoord2f(cw, ch);
        DGL_Vertex2f(x, y);
        DGL_TexCoord2f(1, ch);
        DGL_Vertex2f(x + w, y);
        DGL_TexCoord2f(1, 1);
        DGL_Vertex2f(x + w, y + h);
        DGL_TexCoord2f(cw, 1);
        DGL_Vertex2f(x, y + h);

        // bottom (behind the chain)
        x = 38;
        y = 192;
        w = 244;
        h = 8;
        cw = (float) 38 / ST_WIDTH;
        cw2 = (float) (ST_WIDTH - 38) / ST_WIDTH;
        ch = 0.87692307692307692307692307692308f;

        DGL_TexCoord2f(cw, ch);
        DGL_Vertex2f(x, y);
        DGL_TexCoord2f(cw2, ch);
        DGL_Vertex2f(x + w, y);
        DGL_TexCoord2f(cw2, 1);
        DGL_Vertex2f(x + w, y + h);
        DGL_TexCoord2f(cw, 1);
        DGL_Vertex2f(x, y + h);

        DGL_End();

        if(!hud->inventory)
        {
            // Main interface
            if(!AM_IsMapActive(player))
            {
                if(deathmatch)
                {
                    GL_DrawPatch_CS(38, 162, PatchNumKILLS.lump);
                }

                // left of statbar (upto weapon puzzle display)
                GL_SetPatch(PatchNumSTATBAR.lump, DGL_CLAMP, DGL_CLAMP);
                DGL_Begin(DGL_QUADS);

                x = deathmatch ? 68 : 38;
                y = 162;
                w = deathmatch ? 122 : 152;
                h = 30;
                cw = deathmatch ? (float) 15 / 122 : 0;
                cw2 = 0.62295081967213114754098360655738f;
                ch = 0.96774193548387096774193548387097f;

                DGL_TexCoord2f(cw, 0);
                DGL_Vertex2f(x, y);
                DGL_TexCoord2f(cw2, 0);
                DGL_Vertex2f(x + w, y);
                DGL_TexCoord2f(cw2, ch);
                DGL_Vertex2f(x + w, y + h);
                DGL_TexCoord2f(cw, ch);
                DGL_Vertex2f(x, y + h);

                // right of statbar (after weapon puzzle display)
                x = 247;
                y = 162;
                w = 35;
                h = 30;
                cw = 0.85655737704918032786885245901639f;
                ch = 0.96774193548387096774193548387097f;

                DGL_TexCoord2f(cw, 0);
                DGL_Vertex2f(x, y);
                DGL_TexCoord2f(1, 0);
                DGL_Vertex2f(x + w, y);
                DGL_TexCoord2f(1, ch);
                DGL_Vertex2f(x + w, y + h);
                DGL_TexCoord2f(cw, ch);
                DGL_Vertex2f(x, y + h);

                DGL_End();

                drawWeaponPieces(hud);
            }
            else
            {
                GL_DrawPatch_CS(38, 162, PatchNumKEYBAR.lump);
            }
        }
        else
        {
            // INVBAR
            GL_SetPatch(PatchNumINVBAR.lump, DGL_CLAMP, DGL_CLAMP);
            DGL_Begin(DGL_QUADS);

            x = 38;
            y = 162;
            w = 244;
            h = 30;
            ch = 0.96774193548387096774193548387097f;

            DGL_TexCoord2f(0, 0);
            DGL_Vertex2f(x, y);
            DGL_TexCoord2f(1, 0);
            DGL_Vertex2f(x + w, y);
            DGL_TexCoord2f(1, ch);
            DGL_Vertex2f(x + w, y + h);
            DGL_TexCoord2f(0, ch);
            DGL_Vertex2f(x, y + h);

            DGL_End();
        }

        drawChain(hud);
    }
}

void ST_loadGraphics(void)
{
    int         i;
    char        namebuf[9];

    fontBNumBase = W_GetNumForName("FONTB16");  // to be removed

    R_CachePatch(&PatchNumH2BAR, "H2BAR");
    R_CachePatch(&PatchNumH2TOP, "H2TOP");
    R_CachePatch(&PatchNumINVBAR, "INVBAR");
    R_CachePatch(&PatchNumLFEDGE, "LFEDGE");
    R_CachePatch(&PatchNumRTEDGE, "RTEDGE");
    R_CachePatch(&PatchNumSTATBAR, "STATBAR");
    R_CachePatch(&PatchNumKEYBAR, "KEYBAR");
    R_CachePatch(&PatchNumSELECTBOX, "SELECTBOX");

    R_CachePatch(&PatchMANAAVIALS[0], "MANAVL1D");
    R_CachePatch(&PatchMANABVIALS[0], "MANAVL2D");
    R_CachePatch(&PatchMANAAVIALS[1], "MANAVL1");
    R_CachePatch(&PatchMANABVIALS[1], "MANAVL2");

    R_CachePatch(&PatchMANAAICONS[0], "MANADIM1");
    R_CachePatch(&PatchMANABICONS[0], "MANADIM2");
    R_CachePatch(&PatchMANAAICONS[1], "MANABRT1");
    R_CachePatch(&PatchMANABICONS[1], "MANABRT2");

    R_CachePatch(&PatchNumINVLFGEM1, "invgeml1");
    R_CachePatch(&PatchNumINVLFGEM2, "invgeml2");
    R_CachePatch(&PatchNumINVRTGEM1, "invgemr1");
    R_CachePatch(&PatchNumINVRTGEM2, "invgemr2");
    R_CachePatch(&PatchNumNEGATIVE, "NEGNUM");
    R_CachePatch(&PatchNumKILLS, "KILLS");
    R_CachePatch(&SpinFlylump, "SPFLY0");
    R_CachePatch(&SpinMinotaurLump, "SPMINO0");
    R_CachePatch(&SpinSpeedLump, "SPBOOT0");
    R_CachePatch(&SpinDefenseLump, "SPSHLD0");

    // Fighter:
    R_CachePatch(&PatchNumPIECE1[PCLASS_FIGHTER], "wpiecef1");
    R_CachePatch(&PatchNumPIECE2[PCLASS_FIGHTER], "wpiecef2");
    R_CachePatch(&PatchNumPIECE3[PCLASS_FIGHTER], "wpiecef3");
    R_CachePatch(&PatchNumCHAIN[PCLASS_FIGHTER], "chain");
    R_CachePatch(&PatchNumWEAPONSLOT[PCLASS_FIGHTER], "wpslot0");
    R_CachePatch(&PatchNumWEAPONFULL[PCLASS_FIGHTER], "wpfull0");
    R_CachePatch(&PatchNumLifeGem[PCLASS_FIGHTER][0], "lifegem");
    for(i = 1; i < 8; ++i)
    {
        sprintf(namebuf, "lifegmf%d", i);
        R_CachePatch(&PatchNumLifeGem[PCLASS_FIGHTER][i], namebuf);
    }

    // Cleric:
    R_CachePatch(&PatchNumPIECE1[PCLASS_CLERIC], "wpiecec1");
    R_CachePatch(&PatchNumPIECE2[PCLASS_CLERIC], "wpiecec2");
    R_CachePatch(&PatchNumPIECE3[PCLASS_CLERIC], "wpiecec3");
    R_CachePatch(&PatchNumCHAIN[PCLASS_CLERIC], "chain2");
    R_CachePatch(&PatchNumWEAPONSLOT[PCLASS_CLERIC], "wpslot1");
    R_CachePatch(&PatchNumWEAPONFULL[PCLASS_CLERIC], "wpfull1");
    for(i = 0; i < 8; ++i)
    {
        sprintf(namebuf, "lifegmc%d", i);
        R_CachePatch(&PatchNumLifeGem[PCLASS_CLERIC][i], namebuf);
    }

    // Mage:
    R_CachePatch(&PatchNumPIECE1[PCLASS_MAGE], "wpiecem1");
    R_CachePatch(&PatchNumPIECE2[PCLASS_MAGE], "wpiecem2");
    R_CachePatch(&PatchNumPIECE3[PCLASS_MAGE], "wpiecem3");
    R_CachePatch(&PatchNumCHAIN[PCLASS_MAGE], "chain3");
    R_CachePatch(&PatchNumWEAPONSLOT[PCLASS_MAGE], "wpslot2");
    R_CachePatch(&PatchNumWEAPONFULL[PCLASS_MAGE], "wpfull2");
    for(i = 0; i < 8; ++i)
    {
        sprintf(namebuf, "lifegmm%d", i);
        R_CachePatch(&PatchNumLifeGem[PCLASS_MAGE][i], namebuf);
    }

    for(i = 0; i < 10; ++i)
    {
        sprintf(namebuf, "IN%d", i);
        R_CachePatch(&PatchNumINumbers[i], namebuf);
    }

    for(i = 0; i < 10; ++i)
    {
        sprintf(namebuf, "SMALLIN%d", i);
        R_CachePatch(&PatchNumSmNumbers[i], namebuf);
    }

    // Artifact icons (+5 for the use-artifact flash patches)
    for(i = 0; i < (NUM_ARTIFACT_TYPES + 5); ++i)
    {
        sprintf(namebuf, "%s", artifactlist[i]);
        R_CachePatch(&PatchARTIFACTS[i], namebuf);
    }
}

void ST_loadData(void)
{
    ST_loadGraphics();
}

static void initData(hudstate_t* hud)
{
    int                 i;
    int                 player = hud - hudStates;
    player_t*           plr = &players[player];

    // Ensure the HUD widget lib has been inited.
    STlib_init();

    hud->alpha = 0.0f;
    hud->stopped = true;
    hud->showbar = 0.0f;
    hud->statusbarCounterAlpha = 0.0f;
    hud->blended = false;
    hud->oldhealth = -1;
    hud->oldarti = 0;
    hud->oldartiCount = 0;

    hud->armorlevel = 0;
    hud->artici = 0;
    hud->manaAicon = 0;
    hud->manaBicon = 0;
    hud->manaAvial = 0;
    hud->manaBvial = 0;
    hud->manaACount = 0;
    hud->manaBCount = 0;
    hud->inventory = false;

    hud->firstTime = true;

    hud->statusbaron = true;

    for(i = 0; i < NUMVISINVSLOTS; ++i)
    {
        hud->invslot[i] = 0;
        hud->invslotcount[i] = 0;
    }

    ST_HUDUnHide(player, HUE_FORCE);
}

void ST_createWidgets(int player)
{
    int                 i, width, temp;
    player_t*           plr = &players[player];
    hudstate_t*         hud = &hudStates[player];

    // Health num.
    STlib_initNum(&hud->wHealth, ST_HEALTHX, ST_HEALTHY, PatchNumINumbers,
                  &plr->health, &hud->statusbaron, ST_HEALTHWIDTH,
                  &hud->statusbarCounterAlpha);

    // Frags sum.
    STlib_initNum(&hud->wFrags, ST_FRAGSX, ST_FRAGSY, PatchNumINumbers,
                  &hud->fragscount, &hud->fragson, ST_FRAGSWIDTH,
                  &hud->statusbarCounterAlpha);

    // Armor num - should be colored later.
    STlib_initNum(&hud->wArmor, ST_ARMORX, ST_ARMORY, PatchNumINumbers,
                  &hud->armorlevel, &hud->statusbaron, ST_ARMORWIDTH,
                  &hud->statusbarCounterAlpha);

    // ManaA count.
    STlib_initNum(&hud->wManaACount, ST_MANAAX, ST_MANAAY,
                  PatchNumSmNumbers, &hud->manaACount, &hud->statusbaron,
                  ST_MANAAWIDTH, &hud->statusbarCounterAlpha);

    // ManaB count.
    STlib_initNum(&hud->wManaBCount, ST_MANABX, ST_MANABY,
                  PatchNumSmNumbers, &hud->manaBCount, &hud->statusbaron,
                  ST_MANABWIDTH, &hud->statusbarCounterAlpha);

    // Current mana A icon.
    STlib_initMultIcon(&hud->wManaA, ST_MANAAICONX, ST_MANAAICONY, PatchMANAAICONS,
                       &hud->manaAicon, &hud->statusbaron, &hud->statusbarCounterAlpha);

    // Current mana B icon.
    STlib_initMultIcon(&hud->wManaB, ST_MANABICONX, ST_MANABICONY, PatchMANABICONS,
                       &hud->manaBicon, &hud->statusbaron, &hud->statusbarCounterAlpha);

    // Current mana A vial.
    STlib_initMultIcon(&hud->wManaAVial, ST_MANAAVIALX, ST_MANAAVIALY, PatchMANAAVIALS,
                       &hud->manaAvial, &hud->statusbaron, &hud->statusbarCounterAlpha);

    // Current mana B vial.
    STlib_initMultIcon(&hud->wManaBVial, ST_MANABVIALX, ST_MANABVIALY, PatchMANABVIALS,
                       &hud->manaBvial, &hud->statusbaron, &hud->statusbarCounterAlpha);

    // Current artifact (stbar not inventory).
    STlib_initMultIcon(&hud->wArti, ST_ARTIFACTX, ST_ARTIFACTY, PatchARTIFACTS,
                       &hud->artici, &hud->statusbaron, &hud->statusbarCounterAlpha);

    // Current artifact count.
    STlib_initNum(&hud->wArtiCount, ST_ARTIFACTCX, ST_ARTIFACTCY, PatchNumSmNumbers,
                  &hud->oldartiCount, &hud->statusbaron, ST_ARTIFACTCWIDTH,
                  &hud->statusbarCounterAlpha);

    // Inventory slots
    width = PatchARTIFACTS[5].width + 1;
    temp = 0;

    for(i = 0; i < NUMVISINVSLOTS; ++i)
    {
        // Inventory slot icon.
        STlib_initMultIcon(&hud->wInvSlot[i], ST_INVENTORYX + temp , ST_INVENTORYY,
                           PatchARTIFACTS, &hud->invslot[i],
                           &hud->statusbaron, &hud->statusbarCounterAlpha);

        // Inventory slot counter.
        STlib_initNum(&hud->wInvSlotCount[i], ST_INVENTORYX + temp + ST_INVCOUNTOFFX,
                      ST_INVENTORYY + ST_INVCOUNTOFFY, PatchNumSmNumbers,
                      &hud->invslotcount[i], &hud->statusbaron, ST_ARTIFACTCWIDTH,
                      &hud->statusbarCounterAlpha);

        temp += width;
    }
}

void ST_Start(int player)
{
    hudstate_t*         hud;

    if(player < 0 || player >= MAXPLAYERS)
        return;

    hud = &hudStates[player];

    if(!hud->stopped)
        ST_Stop(player);

    initData(hud);
    ST_createWidgets(player);

    hud->stopped = false;
}

void ST_Stop(int player)
{
    hudstate_t*         hud;

    if(player < 0 || player >= MAXPLAYERS)
        return;

    hud = &hudStates[player];
    if(hud->stopped)
        return;

    hud->stopped = true;
}

void ST_Init(void)
{
    ST_loadData();
}

void ST_Inventory(int player, boolean show)
{
    player_t*           plr;
    hudstate_t*         hud;

    if(player < 0 || player >= MAXPLAYERS)
        return;

    plr = &players[player];
    if(!((plr->plr->flags & DDPF_LOCAL) && plr->plr->inGame))
        return;

    hud = &hudStates[player];

    if(show)
    {
        hud->inventory = true;

        hud->inventoryTics = (int) (cfg.inventoryTimer * TICSPERSEC);
        if(hud->inventoryTics < 1)
            hud->inventoryTics = 1;

        ST_HUDUnHide(player, HUE_FORCE);
    }
    else
    {
        hud->inventory = false;
    }
}

boolean ST_IsInventoryVisible(int player)
{
    player_t*           plr;
    hudstate_t*         hud;

    if(player < 0 || player >= MAXPLAYERS)
        return false;

    plr = &players[player];
    if(!((plr->plr->flags & DDPF_LOCAL) && plr->plr->inGame))
        return false;

    hud = &hudStates[player];

    return hud->inventory;
}

void ST_InventoryFlashCurrent(int player)
{
    player_t*           plr;
    hudstate_t*         hud;

    if(player < 0 || player >= MAXPLAYERS)
        return;

    plr = &players[player];
    if(!((plr->plr->flags & DDPF_LOCAL) && plr->plr->inGame))
        return;

    hud = &hudStates[player];

    hud->artifactFlash = 4;
}

void ST_updateWidgets(int player)
{
    int                 i, x, pClass;
    hudstate_t*         hud = &hudStates[player];
    player_t*           plr = &players[player];

    // Original player class (i.e. not pig).
    pClass = cfg.playerClass[player];

    if(hud->blended)
    {
        hud->statusbarCounterAlpha = cfg.statusbarCounterAlpha - hud->hideAmount;
        CLAMP(hud->statusbarCounterAlpha, 0.0f, 1.0f);
    }
    else
        hud->statusbarCounterAlpha = 1.0f;

    // Used by w_frags widget.
    hud->fragson = deathmatch && hud->statusbaron;

    hud->fragscount = 0;

    for(i = 0; i < MAXPLAYERS; ++i)
    {
        if(!players[i].plr->inGame)
            continue;

        hud->fragscount += plr->frags[i] * (i != player ? 1 : -1);
    }

    // Current artifact.
    if(hud->artifactFlash)
    {
        hud->artici = 5 - hud->artifactFlash;
        hud->artifactFlash--;
        hud->oldarti = -1; // So that the correct artifact fills in after the flash
    }
    else if(hud->oldarti != plr->readyArtifact ||
            hud->oldartiCount != plr->inventory[plr->invPtr].count)
    {
        if(plr->readyArtifact > 0)
        {
            hud->artici = plr->readyArtifact + 5;
        }
        hud->oldarti = plr->readyArtifact;
        hud->oldartiCount = plr->inventory[plr->invPtr].count;
    }

    // Armor
    hud->armorlevel = FixedDiv(
        PCLASS_INFO(pClass)->autoArmorSave + plr->armorPoints[ARMOR_ARMOR] +
        plr->armorPoints[ARMOR_SHIELD] +
        plr->armorPoints[ARMOR_HELMET] +
        plr->armorPoints[ARMOR_AMULET], 5 * FRACUNIT) >> FRACBITS;

    // mana A
    hud->manaACount = plr->ammo[AT_BLUEMANA].owned;

    // mana B
    hud->manaBCount = plr->ammo[AT_GREENMANA].owned;

    hud->manaAicon = hud->manaBicon = hud->manaAvial = hud->manaBvial = -1;

    // Mana
    if(!(plr->ammo[AT_BLUEMANA].owned > 0))
        hud->manaAicon = 0; // Draw dim Mana icon.

    if(!(plr->ammo[AT_GREENMANA].owned > 0))
        hud->manaBicon = 0; // Draw dim Mana icon.

    // Update mana graphics based upon mana count weapon type
    if(plr->readyWeapon == WT_FIRST)
    {
        hud->manaAicon = 0;
        hud->manaBicon = 0;

        hud->manaAvial = 0;
        hud->manaBvial = 0;
    }
    else if(plr->readyWeapon == WT_SECOND)
    {
        // If there is mana for this weapon, make it bright!
        if(hud->manaAicon == -1)
        {
            hud->manaAicon = 1;
        }

        hud->manaAvial = 1;

        hud->manaBicon = 0;
        hud->manaBvial = 0;
    }
    else if(plr->readyWeapon == WT_THIRD)
    {
        hud->manaAicon = 0;
        hud->manaAvial = 0;

        // If there is mana for this weapon, make it bright!
        if(hud->manaBicon == -1)
        {
            hud->manaBicon = 1;
        }

        hud->manaBvial = 1;
    }
    else
    {
        hud->manaAvial = 1;
        hud->manaBvial = 1;

        // If there is mana for this weapon, make it bright!
        if(hud->manaAicon == -1)
        {
            hud->manaAicon = 1;
        }
        if(hud->manaBicon == -1)
        {
            hud->manaBicon = 1;
        }
    }

    // update the inventory

    x = plr->invPtr - plr->curPos;

    for(i = 0; i < NUMVISINVSLOTS; ++i)
    {
        hud->invslot[i] = plr->inventory[x + i].type +5;  // plus 5 for useartifact patches
        hud->invslotcount[i] = plr->inventory[x + i].count;
    }
}

void ST_Ticker(void)
{
    int                 i;

    for(i = 0; i < MAXPLAYERS; ++i)
    {
        player_t*           plr = &players[i];
        hudstate_t*         hud = &hudStates[i];

        if(!(plr->plr->inGame && (plr->plr->flags & DDPF_LOCAL)))
            continue;

        if(!P_IsPaused())
        {
            int                 delta;
            int                 curHealth;

            if(cfg.hudTimer == 0)
            {
                hud->hideTics = hud->hideAmount = 0;
            }
            else
            {
                if(hud->hideTics > 0)
                    hud->hideTics--;
                if(hud->hideTics == 0 && cfg.hudTimer > 0 && hud->hideAmount < 1)
                    hud->hideAmount += 0.1f;
            }

            ST_updateWidgets(i);

            curHealth = plr->plr->mo->health;
            if(curHealth < 0)
            {
                curHealth = 0;
            }
            if(curHealth < hud->healthMarker)
            {
                delta = (hud->healthMarker - curHealth) >> 2;
                if(delta < 1)
                {
                    delta = 1;
                }
                else if(delta > 6)
                {
                    delta = 6;
                }
                hud->healthMarker -= delta;
            }
            else if(curHealth > hud->healthMarker)
            {
                delta = (curHealth - hud->healthMarker) >> 2;
                if(delta < 1)
                {
                    delta = 1;
                }
                else if(delta > 6)
                {
                    delta = 6;
                }
                hud->healthMarker += delta;
            }

            // Turn inventory off after a certain amount of time.
            if(hud->inventory && !(--hud->inventoryTics))
            {
                plr->readyArtifact = plr->inventory[plr->invPtr].type;
                hud->inventory = false;
            }
        }
    }
}

/**
 * Sets the new palette based upon the current values of
 * player_t->damageCount and player_t->bonusCount.
 */
void ST_doPaletteStuff(int player, boolean forceChange)
{
    int                 palette;
    player_t*           plr;

    if(player < 0 || player >= MAXPLAYERS)
        return;

    plr = &players[player];

    if(G_GetGameState() == GS_LEVEL)
    {
        plr = &players[CONSOLEPLAYER];
        if(plr->poisonCount)
        {
            palette = 0;
            palette = (plr->poisonCount + 7) >> 3;
            if(palette >= NUMPOISONPALS)
            {
                palette = NUMPOISONPALS - 1;
            }
            palette += STARTPOISONPALS;
        }
        else if(plr->damageCount)
        {
            palette = (plr->damageCount + 7) >> 3;
            if(palette >= NUMREDPALS)
            {
                palette = NUMREDPALS - 1;
            }
            palette += STARTREDPALS;
        }
        else if(plr->bonusCount)
        {
            palette = (plr->bonusCount + 7) >> 3;
            if(palette >= NUMBONUSPALS)
            {
                palette = NUMBONUSPALS - 1;
            }
            palette += STARTBONUSPALS;
        }
        else if(plr->plr->mo->flags2 & MF2_ICEDAMAGE)
        {                       // Frozen player
            palette = STARTICEPAL;
        }
        else
        {
            palette = 0;
        }
    }
    else
    {
        palette = 0;
    }

    plr->plr->filter = R_GetFilterColor(palette);
}

static void drawWidgets(hudstate_t* hud)
{
    int                 i, x;
    int                 player = hud - hudStates;
    player_t*           plr = &players[player];
    boolean             refresh = true;

    hud->oldhealth = -1;
    if(!hud->inventory)
    {
        if(!AM_IsMapActive(player))
        {
            // Frags
            if(deathmatch)
                STlib_updateNum(&hud->wFrags, refresh);
            else
                STlib_updateNum(&hud->wHealth, refresh);

            STlib_updateNum(&hud->wArmor, refresh);

            if(plr->readyArtifact > 0)
            {
                STlib_updateMultIcon(&hud->wArti, refresh);
                if(!hud->artifactFlash && plr->inventory[plr->invPtr].count > 1)
                    STlib_updateNum(&hud->wArtiCount, refresh);
            }

            if(hud->manaACount > 0)
                STlib_updateNum(&hud->wManaACount, refresh);

            if(hud->manaBCount > 0)
                STlib_updateNum(&hud->wManaBCount, refresh);

            STlib_updateMultIcon(&hud->wManaA, refresh);
            STlib_updateMultIcon(&hud->wManaB, refresh);
            STlib_updateMultIcon(&hud->wManaAVial, refresh);
            STlib_updateMultIcon(&hud->wManaBVial, refresh);

            // Draw the mana bars
            GL_SetNoTexture();
            GL_DrawRect(95, 165, 3,
                        22 - (22 * plr->ammo[AT_BLUEMANA].owned) / MAX_MANA,
                        0, 0, 0, hud->statusbarCounterAlpha);
            GL_DrawRect(103, 165, 3,
                        22 - (22 * plr->ammo[AT_GREENMANA].owned) / MAX_MANA,
                        0, 0, 0, hud->statusbarCounterAlpha);
        }
        else
        {
            drawKeyBar(hud);
        }
    }
    else
    {   // Draw Inventory

        x = plr->invPtr - plr->curPos;

        for(i = 0; i < NUMVISINVSLOTS; ++i)
        {
            if(plr->inventory[x + i].type != AFT_NONE)
            {
                STlib_updateMultIcon(&hud->wInvSlot[i], refresh);

                if(plr->inventory[x + i].count > 1)
                    STlib_updateNum(&hud->wInvSlotCount[i], refresh);
            }
        }

        // Draw selector box
        GL_DrawPatch(ST_INVENTORYX + plr->curPos * 31, 163, PatchNumSELECTBOX.lump);

        // Draw more left indicator
        if(x != 0)
            GL_DrawPatchLitAlpha(42, 163, 1, hud->statusbarCounterAlpha,
                                 !(levelTime & 4) ? PatchNumINVLFGEM1.lump : PatchNumINVLFGEM2.lump);

        // Draw more right indicator
        if(plr->inventorySlotNum - x > 7)
            GL_DrawPatchLitAlpha(269, 163, 1, hud->statusbarCounterAlpha,
                                 !(levelTime & 4) ? PatchNumINVRTGEM1.lump : PatchNumINVRTGEM2.lump);
    }
}

/**
 * Draws a three digit number.
 */
static void DrINumber(signed int val, int x, int y, float r, float g,
                      float b, float a)
{
    int         oldval;

    DGL_Color4f(r,g,b,a);

    // Make sure it's a three digit number.
    if(val < -999)
        val = -999;
    if(val > 999)
        val = 999;

    oldval = val;
    if(val < 0)
    {
        val = -val;
        if(val > 99)
        {
            val = 99;
        }
        if(val > 9)
        {
            GL_DrawPatch_CS(x + 8, y, PatchNumINumbers[val / 10].lump);
            GL_DrawPatch_CS(x, y, PatchNumNEGATIVE.lump);
        }
        else
        {
            GL_DrawPatch_CS(x + 8, y, PatchNumNEGATIVE.lump);
        }
        val = val % 10;
        GL_DrawPatch_CS(x + 16, y, PatchNumINumbers[val].lump);
        return;
    }
    if(val > 99)
    {
        GL_DrawPatch_CS(x, y, PatchNumINumbers[val / 100].lump);
    }
    val = val % 100;
    if(val > 9 || oldval > 99)
    {
        GL_DrawPatch_CS(x + 8, y, PatchNumINumbers[val / 10].lump);
    }
    val = val % 10;
    GL_DrawPatch_CS(x + 16, y, PatchNumINumbers[val].lump);

}

/**
 * Draws a three digit number using FontB
 */
static void DrBNumber(signed int val, int x, int y, float red, float green,
                      float blue, float alpha)
{
    lumppatch_t    *patch;
    int         xpos;
    int         oldval;

    // Limit to three digits.
    if(val > 999)
        val = 999;
    if(val < -999)
        val = -999;

    oldval = val;
    xpos = x;
    if(val < 0)
    {
        val = 0;
    }
    if(val > 99)
    {
        patch = W_CacheLumpNum(fontBNumBase + val / 100, PU_CACHE);
        GL_DrawPatchLitAlpha(xpos + 8 - SHORT(patch->width) / 2, y +2, 0,
                             alpha * .4f,
                             fontBNumBase + val / 100);
        DGL_Color4f(red, green, blue, alpha);
        GL_DrawPatch_CS(xpos + 6 - SHORT(patch->width) / 2, y,
                             fontBNumBase + val / 100);
        DGL_Color4f(1, 1, 1, 1);
    }
    val = val % 100;
    xpos += 12;
    if(val > 9 || oldval > 99)
    {
        patch = W_CacheLumpNum(fontBNumBase + val / 10, PU_CACHE);
        GL_DrawPatchLitAlpha(xpos + 8 - SHORT(patch->width) / 2, y +2, 0,
                             alpha * .4f,
                             fontBNumBase + val / 10);
        DGL_Color4f(red, green, blue, alpha);
        GL_DrawPatch_CS(xpos + 6 - SHORT(patch->width) / 2, y,
                             fontBNumBase + val / 10);
        DGL_Color4f(1, 1, 1, 1);
    }
    val = val % 10;
    xpos += 12;
    patch = W_CacheLumpNum(fontBNumBase + val, PU_CACHE);
    GL_DrawPatchLitAlpha(xpos + 8 - SHORT(patch->width) / 2, y +2, 0,
                         alpha *.4f, fontBNumBase + val);
    DGL_Color4f(red, green, blue, alpha);
    GL_DrawPatch_CS(xpos + 6 - SHORT(patch->width) / 2, y,
                             fontBNumBase + val);
    DGL_Color4f(1, 1, 1, 1);
}

/**
 * Draws a small two digit number.
 */
static void DrSmallNumber(int val, int x, int y, float r, float g, float b, float a)
{
    DGL_Color4f(r,g,b,a);

    if(val <= 0)
    {
        return;
    }
    if(val > 999)
    {
        val %= 1000;
    }
    if(val > 99)
    {
        GL_DrawPatch_CS(x, y, PatchNumSmNumbers[val / 100].lump);
        GL_DrawPatch_CS(x + 4, y, PatchNumSmNumbers[(val % 100) / 10].lump);
    }
    else if(val > 9)
    {
        GL_DrawPatch_CS(x + 4, y, PatchNumSmNumbers[val / 10].lump);
    }
    val %= 10;
    GL_DrawPatch_CS(x + 8, y, PatchNumSmNumbers[val].lump);
}

#if 0 // Unused atm
/**
 * Displays sound debugging information.
 */
static void DrawSoundInfo(void)
{
    int         i;
    SoundInfo_t s;
    ChanInfo_t *c;
    char        text[32];
    int         x;
    int         y;
    int         xPos[7] = { 1, 75, 112, 156, 200, 230, 260 };

    if(leveltime & 16)
    {
        MN_DrTextA("*** SOUND DEBUG INFO ***", xPos[0], 20);
    }
    S_GetChannelInfo(&s);
    if(s.channelCount == 0)
    {
        return;
    }
    x = 0;
    MN_DrTextA("NAME", xPos[x++], 30);
    MN_DrTextA("MO.T", xPos[x++], 30);
    MN_DrTextA("MO.X", xPos[x++], 30);
    MN_DrTextA("MO.Y", xPos[x++], 30);
    MN_DrTextA("ID", xPos[x++], 30);
    MN_DrTextA("PRI", xPos[x++], 30);
    MN_DrTextA("DIST", xPos[x++], 30);
    for(i = 0; i < s.channelCount; ++i)
    {
        c = &s.chan[i];
        x = 0;
        y = 40 + i * 10;
        if(c->mo == NULL)
        {                       // Channel is unused
            MN_DrTextA("------", xPos[0], y);
            continue;
        }
        sprintf(text, "%s", c->name);
        strupr(text);
        MN_DrTextA(text, xPos[x++], y);
        sprintf(text, "%d", c->mo->type);
        MN_DrTextA(text, xPos[x++], y);
        sprintf(text, "%d", c->mo->x >> FRACBITS);
        MN_DrTextA(text, xPos[x++], y);
        sprintf(text, "%d", c->mo->y >> FRACBITS);
        MN_DrTextA(text, xPos[x++], y);
        sprintf(text, "%d", c->id);
        MN_DrTextA(text, xPos[x++], y);
        sprintf(text, "%d", S_sfx[c->id].usefulness);
        MN_DrTextA(text, xPos[x++], y);
        sprintf(text, "%d", c->distance);
    }
}
#endif

/**
 * Unhides the current HUD display if hidden.
 *
 * @param player        The player whoose HUD to (maybe) unhide.
 * @param event         The HUD Update Event type to check for triggering.
 */
void ST_HUDUnHide(int player, hueevent_t ev)
{
    player_t*           plr;

    if(ev < HUE_FORCE || ev > NUMHUDUNHIDEEVENTS)
        return;

    plr = &players[player];
    if(!(plr->plr->inGame && (plr->plr->flags & DDPF_LOCAL)))
        return;

    if(ev == HUE_FORCE || cfg.hudUnHide[ev])
    {
        hudStates[player].hideTics = (cfg.hudTimer * TICSPERSEC);
        hudStates[player].hideAmount = 0;
    }
}

/**
 * All drawing for the status bar starts and ends here
 */
void ST_doRefresh(int player)
{
    hudstate_t*         hud;

    if(player < 0 || player > MAXPLAYERS)
        return;

    hud = &hudStates[player];

    hud->firstTime = false;

    if(cfg.statusbarScale < 20 || (cfg.statusbarScale == 20 && hud->showbar < 1.0f))
    {
        float fscale = cfg.statusbarScale / 20.0f;
        float h = 200 * (1 - fscale);

        DGL_MatrixMode(DGL_MODELVIEW);
        DGL_PushMatrix();
        DGL_Translatef(160 - 320 * fscale / 2, h / hud->showbar, 0);
        DGL_Scalef(fscale, fscale, 1);
    }

    drawStatusBarBackground(player);
    drawWidgets(hud);

    if(cfg.statusbarScale < 20 ||
       (cfg.statusbarScale == 20 && hud->showbar < 1.0f))
    {
        // Restore the normal modelview matrix.
        DGL_MatrixMode(DGL_MODELVIEW);
        DGL_PopMatrix();
    }
}

void ST_doFullscreenStuff(int player)
{
    int                 i, x, temp;
    hudstate_t*         hud = &hudStates[player];
    player_t*           plr = &players[player];
    float               textalpha =
        hud->alpha - hud->hideAmount - (1 - cfg.hudColor[3]);
    float               iconalpha =
        hud->alpha - hud->hideAmount - (1 - cfg.hudIconAlpha);

    if(cfg.hudShown[HUD_HEALTH])
    {
        Draw_BeginZoom(cfg.hudScale, 5, 198);
        if(plr->plr->mo->health > 0)
        {
            DrBNumber(plr->plr->mo->health, 5, 180,
                      cfg.hudColor[0], cfg.hudColor[1], cfg.hudColor[2], textalpha);
        }
        else
        {
            DrBNumber(0, 5, 180, cfg.hudColor[0], cfg.hudColor[1], cfg.hudColor[2],
                      textalpha);
        }
        Draw_EndZoom();
    }

    if(cfg.hudShown[HUD_MANA])
    {
        int     dim[2] = { PatchMANAAICONS[0].lump, PatchMANABICONS[0].lump };
        int     bright[2] = { PatchMANAAICONS[0].lump, PatchMANABICONS[0].lump };
        int     patches[2] = { 0, 0 };
        int     ypos = cfg.hudShown[HUD_MANA] == 2 ? 152 : 2;

        for(i = 0; i < 2; i++)
            if(!(plr->ammo[i].owned > 0))
                patches[i] = dim[i];
        if(plr->readyWeapon == WT_FIRST)
        {
            for(i = 0; i < 2; i++)
                patches[i] = dim[i];
        }
        if(plr->readyWeapon == WT_SECOND)
        {
            if(!patches[0])
                patches[0] = bright[0];
            patches[1] = dim[1];
        }
        if(plr->readyWeapon == WT_THIRD)
        {
            patches[0] = dim[0];
            if(!patches[1])
                patches[1] = bright[1];
        }
        if(plr->readyWeapon == WT_FOURTH)
        {
            for(i = 0; i < 2; i++)
                if(!patches[i])
                    patches[i] = bright[i];
        }
        Draw_BeginZoom(cfg.hudScale, 2, ypos);
        for(i = 0; i < 2; i++)
        {
            GL_DrawPatchLitAlpha(2, ypos + i * 13, 1, iconalpha, patches[i]);
            DrINumber(plr->ammo[i].owned, 18, ypos + i * 13,
                      1, 1, 1, textalpha);
        }
        Draw_EndZoom();
    }

    if(deathmatch)
    {
        temp = 0;
        for(i = 0; i < MAXPLAYERS; i++)
        {
            if(players[i].plr->inGame)
            {
                temp += plr->frags[i];
            }
        }
        Draw_BeginZoom(cfg.hudScale, 2, 198);
        DrINumber(temp, 45, 185, 1, 1, 1, textalpha);
        Draw_EndZoom();
    }

    if(!hud->inventory)
    {
        if(cfg.hudShown[HUD_ARTI])
        {
            if(plr->readyArtifact > 0)
            {
                    Draw_BeginZoom(cfg.hudScale, 318, 198);
                GL_DrawPatchLitAlpha(286, 170, 1, iconalpha/2, W_GetNumForName("ARTIBOX"));
                GL_DrawPatchLitAlpha(284, 169, 1, iconalpha,
                         W_GetNumForName(artifactlist[plr->readyArtifact+5]));
                if(plr->inventory[plr->invPtr].count > 1)
                {
                    DrSmallNumber(plr->inventory[plr->invPtr].count,
                                  302, 192, 1, 1, 1, textalpha);
                }
                    Draw_EndZoom();
            }
        }
    }
    else
    {
        float invScale;

        invScale = MINMAX_OF(0.25f, cfg.hudScale - 0.25f, 0.8f);

        Draw_BeginZoom(invScale, 160, 198);
        x = plr->invPtr - plr->curPos;
        for(i = 0; i < 7; i++)
        {
            GL_DrawPatchLitAlpha(50 + i * 31, 168, 1, iconalpha/2, W_GetNumForName("ARTIBOX"));
            if(plr->inventorySlotNum > x + i &&
               plr->inventory[x + i].type != AFT_NONE)
            {
                GL_DrawPatchLitAlpha(49 + i * 31, 167, 1, i==plr->curPos? hud->alpha : iconalpha,
                             W_GetNumForName(artifactlist[plr->inventory
                                                       [x + i].type+5]));

                if(plr->inventory[x + i].count > 1)
                {
                    DrSmallNumber(plr->inventory[x + i].count, 66 + i * 31,
                                  188,1, 1, 1, i==plr->curPos? hud->alpha : textalpha/2);
                }
            }
        }
        GL_DrawPatchLitAlpha(50 + plr->curPos * 31, 167, 1, hud->alpha,PatchNumSELECTBOX.lump);
        if(x != 0)
        {
            GL_DrawPatchLitAlpha(40, 167, 1, iconalpha,
                         !(levelTime & 4) ? PatchNumINVLFGEM1.lump :
                         PatchNumINVLFGEM2.lump);
        }
        if(plr->inventorySlotNum - x > 7)
        {
            GL_DrawPatchLitAlpha(268, 167, 1, iconalpha,
                         !(levelTime & 4) ? PatchNumINVRTGEM1.lump :
                         PatchNumINVRTGEM2.lump);
        }
        Draw_EndZoom();
    }
}

void ST_Drawer(int player, int fullscreenmode, boolean refresh)
{
    hudstate_t*         hud;
    player_t*           plr;

    if(player < 0 || player >= MAXPLAYERS)
        return;

    plr = &players[player];
    if(!((plr->plr->flags & DDPF_LOCAL) && plr->plr->inGame))
        return;

    hud = &hudStates[player];

    hud->firstTime = hud->firstTime || refresh;
    hud->statusbaron = (fullscreenmode < 2) ||
                      (AM_IsMapActive(player) &&
                       (cfg.automapHudDisplay == 0 || cfg.automapHudDisplay == 2));

    // Do palette shifts
    ST_doPaletteStuff(player, false);

    // Either slide the status bar in or fade out the fullscreen hud
    if(hud->statusbaron)
    {
        if(hud->alpha > 0.0f)
        {
            hud->statusbaron = 0;
            hud->alpha-=0.1f;
        }
        else if(hud->showbar < 1.0f)
            hud->showbar += 0.1f;
    }
    else
    {
        if(fullscreenmode == 3)
        {
            if(hud->alpha > 0.0f)
            {
                hud->alpha-=0.1f;
                fullscreenmode = 2;
            }
        }
        else
        {
            if(hud->showbar > 0.0f)
            {
                hud->showbar -= 0.1f;
                hud->statusbaron = 1;
            }
            else if(hud->alpha < 1.0f)
                hud->alpha += 0.1f;
        }
    }

    // Always try to render statusbar with alpha in fullscreen modes
    if(fullscreenmode)
        hud->blended = 1;
    else
        hud->blended = 0;

    if(hud->statusbaron)
    {
        ST_doRefresh(player);
    }
    else if(fullscreenmode != 3)
    {
        ST_doFullscreenStuff(player);
    }

    drawAnimatedIcons(hud);
}

/**
 * Draw teleport icon and show it on screen.
 */
void Draw_TeleportIcon(void)
{
    // Dedicated servers don't draw anything.
    if(IS_DEDICATED)
        return;

    GL_DrawRawScreen(W_CheckNumForName("TRAVLPIC"), 0, 0);
    GL_DrawPatch(100, 68, W_GetNumForName("teleicon"));
}

/**
 * Console command to change the size of the status bar.
 */
DEFCC(CCmdStatusBarSize)
{
    int                 min = 1, max = 20, *val = &cfg.statusbarScale;

    if(!stricmp(argv[1], "+"))
        (*val)++;
    else if(!stricmp(argv[1], "-"))
        (*val)--;
    else
        *val = strtol(argv[1], NULL, 0);

    if(*val < min)
        *val = min;
    if(*val > max)
        *val = max;

    // Update the view size if necessary.
    R_SetViewSize(cfg.screenBlocks);
    ST_HUDUnHide(CONSOLEPLAYER, HUE_FORCE); // So the user can see the change.

    return true;
}

/**
 * Changes the class of the given player. Will not work if the player
 * is currently morphed.
 */
void SB_ChangePlayerClass(player_t *player, int newclass)
{
    int         i;
    mobj_t     *oldMobj;
    spawnspot_t dummy;

    // Don't change if morphed.
    if(player->morphTics)
        return;

    if(newclass < 0 || newclass > 2)
        return;                 // Must be 0-2.
    player->class = newclass;
    // Take away armor.
    for(i = 0; i < NUMARMOR; i++)
        player->armorPoints[i] = 0;
    cfg.playerClass[player - players] = newclass;

    P_PostMorphWeapon(player, WT_FIRST);

    player->update |= PSF_ARMOR_POINTS;

    // Respawn the player and destroy the old mobj.
    oldMobj = player->plr->mo;
    if(oldMobj)
    {   // Use a dummy as the spawn point.
        dummy.pos[VX] = oldMobj->pos[VX];
        dummy.pos[VY] = oldMobj->pos[VY];
        dummy.angle = oldMobj->angle;

        P_SpawnPlayer(&dummy, player - players);
        P_MobjRemove(oldMobj, true);
    }
}
