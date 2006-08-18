/* DE1: $Id$
 * Copyright (C) 2004 Jaakko Ker�nen <jaakko.keranen@iki.fi>
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
 * along with this program; if not: http://www.opensource.org/
 */

/*
 * d_netcl.c : Common code related to net games (client-side).
 */

// HEADER FILES ------------------------------------------------------------

#if   __WOLFTC__
#  include "wolftc.h"
#elif __JDOOM__
#  include "jdoom.h"
#elif __JHERETIC__
#  include "jheretic.h"
#  include "p_inventory.h"
#elif __JHEXEN__
#  include "jhexen.h"
#  include "p_inventory.h"
#elif __JSTRIFE__
#  include "jstrife.h"
#  include "p_inventory.h"
#endif

#include "am_map.h"
#include "p_saveg.h"
#include "d_net.h"
#include "d_netsv.h"
#include "f_infine.h"
#include "p_player.h"

// MACROS ------------------------------------------------------------------

// TYPES -------------------------------------------------------------------

// EXTERNAL FUNCTION PROTOTYPES --------------------------------------------

// PUBLIC FUNCTION PROTOTYPES ----------------------------------------------

// PRIVATE FUNCTION PROTOTYPES ---------------------------------------------

// EXTERNAL DATA DECLARATIONS ----------------------------------------------

// PUBLIC DATA DEFINITIONS -------------------------------------------------

// PRIVATE DATA DEFINITIONS ------------------------------------------------

static byte *readbuffer;

// CODE --------------------------------------------------------------------

// Mini-Msg routines.
void NetCl_SetReadBuffer(byte *data)
{
    readbuffer = data;
}

byte NetCl_ReadByte()
{
    return *readbuffer++;
}

short NetCl_ReadShort()
{
    readbuffer += 2;
    return SHORT( *(short *) (readbuffer - 2) );
}

int NetCl_ReadLong()
{
    readbuffer += 4;
    return LONG( *(int *) (readbuffer - 4) );
}

void NetCl_Read(byte *buf, int len)
{
    memcpy(buf, readbuffer, len);
    readbuffer += len;
}

#ifdef __JDOOM__
int NetCl_IsCompatible(int other, int us)
{
    char    comp[5][5] =        // [other][us]
    {
        {1, 1, 0, 1, 0},
        {0, 1, 0, 1, 0},
        {0, 0, 1, 0, 0},
        {0, 0, 0, 1, 0},
        {0, 0, 0, 0, 0}
    };
    /*  shareware,  // DOOM 1 shareware, E1, M9
       registered,  // DOOM 1 registered, E3, M27
       commercial,  // DOOM 2 retail, E1 M34
       retail,      // DOOM 1 retail, E4, M36
     */
    return comp[other][us];
}
#endif

void NetCl_UpdateGameState(byte *data)
{
    byte gsGameMode = 0;
    byte gsFlags = 0;
    byte gsEpisode = 0;
    byte gsMap = 0;
    byte gsDeathmatch = 0;
    byte gsMonsters = 0;
    byte gsRespawn = 0;
    byte gsJumping = 0;
    byte gsSkill = 0;
    fixed_t gsGravity = 0;

    gsGameMode = data[0];
    gsFlags = data[1];
    gsEpisode = data[2];
    gsMap = data[3];
    gsDeathmatch = data[4] & 0x3;
    gsMonsters = (data[4] & 0x4? true : false);
    gsRespawn = (data[4] & 0x8? true : false);
    gsJumping = (data[4] & 0x10? true : false);
#if __JDOOM__ || __JHERETIC__
    gsSkill = (data[4] >> 5);
#else
    gsSkill = data[5] & 0x7;
#endif
    gsGravity = (data[6] << 8) | (data[7] << 16);

    // Demo game state changes are only effective during demo playback.
    if(gsFlags & GSF_DEMO && !Get(DD_PLAYBACK))
        return;

#ifdef __JDOOM__
    if(!NetCl_IsCompatible(gsGameMode, gamemode))
    {
        // Wrong game mode! This is highly irregular!
        Con_Message("NetCl_UpdateGameState: Game mode mismatch!\n");
        // Stop the demo if one is being played.
        DD_Execute("stopdemo", false);
        return;
    }
#endif

    deathmatch = gsDeathmatch;
    nomonsters = !gsMonsters;
    respawnmonsters = gsRespawn;

    // Some statistics.
#if __JHEXEN__ || __JSTRIFE__
    Con_Message("Game state: Map=%i Skill=%i %s\n", gsMap, gsSkill,
                deathmatch == 1 ? "Deathmatch" : deathmatch ==
                2 ? "Deathmatch2" : "Co-op");
#else
    Con_Message("Game state: Map=%i Episode=%i Skill=%i %s\n", gsMap,
                gsEpisode, gsSkill,
                deathmatch == 1 ? "Deathmatch" : deathmatch ==
                2 ? "Deathmatch2" : "Co-op");
#endif
    Con_Message("  Respawn=%s Monsters=%s Jumping=%s Gravity=%.1f\n",
                respawnmonsters ? "yes" : "no", !nomonsters ? "yes" : "no",
                gsJumping ? "yes" : "no", FIX2FLT(gsGravity));

#ifdef __JHERETIC__
    prevmap = gamemap;
#endif

    // Start reading after the GS packet.
#if __JHEXEN__ || __JSTRIFE__
    NetCl_SetReadBuffer(data + 16);
#else
    NetCl_SetReadBuffer(data + 8);
#endif

    // Do we need to change the map?
    if(gsFlags & GSF_CHANGE_MAP)
    {
        G_InitNew(gsSkill, gsEpisode, gsMap);
    }
    else
    {
        gameskill = gsSkill;
        gameepisode = gsEpisode;
        gamemap = gsMap;
    }

    // Set gravity.
    Set(DD_GRAVITY, gsGravity);

    // Camera init included?
    if(gsFlags & GSF_CAMERA_INIT)
    {
        player_t *pl = players + consoleplayer;
        mobj_t *mo;

        mo = pl->plr->mo;
        if(mo)
        {
            P_UnsetThingPosition(mo);
            mo->pos[VX] = NetCl_ReadShort() << 16;
            mo->pos[VY] = NetCl_ReadShort() << 16;
            mo->pos[VZ] = NetCl_ReadShort() << 16;
            P_SetThingPosition(mo);
            pl->plr->clAngle = mo->angle = NetCl_ReadShort() << 16;
            pl->plr->viewz = mo->pos[VZ] + pl->plr->viewheight;
            // Update floorz and ceilingz.
#ifdef __JDOOM__
            P_CheckPosition2(mo, mo->pos[VX], mo->pos[VY], mo->pos[VZ]);
#else
            P_CheckPosition(mo, mo->pos[VX], mo->pos[VY]);
#endif
            mo->floorz = tmfloorz;
            mo->ceilingz = tmceilingz;
        }
        else // mo == NULL
        {
            Con_Message("NetCl_UpdateGameState: Got camera init, but player has no mobj.\n");
            Con_Message("  Pos=%i,%i,%i Angle=%i\n",
                        NetCl_ReadShort(), NetCl_ReadShort(), NetCl_ReadShort(),
                        NetCl_ReadShort());
        }
    }

    // Tell the server we're ready to begin receiving frames.
    Net_SendPacket(DDSP_CONFIRM, DDPT_OK, NULL, 0);
}

void NetCl_UpdatePlayerState2(byte *data, int plrNum)
{
    player_t *pl = &players[plrNum];
    unsigned int flags;
    byte    b;
    int     i, k;

    if(!Get(DD_GAME_READY))
        return;

    NetCl_SetReadBuffer(data);
    flags = NetCl_ReadLong();

    if(flags & PSF2_OWNED_WEAPONS)
    {
        k = NetCl_ReadShort();
        for(i = 0; i < NUMWEAPONS; i++)
            pl->weaponowned[i] = (k & (1 << i)) != 0;
    }

    if(flags & PSF2_STATE)
    {
        b = NetCl_ReadByte();
        pl->playerstate = b & 0xf;
#if __JDOOM__ || __JHERETIC__
        pl->armortype = b >> 4;
#endif
        // Set or clear the DEAD flag for this player.
        if(pl->playerstate == PST_LIVE)
            pl->plr->flags &= ~DDPF_DEAD;
        else
            pl->plr->flags |= DDPF_DEAD;

        pl->cheats = NetCl_ReadByte();

        // Set or clear the NOCLIP flag.
        if(pl->cheats & CF_NOCLIP)
            pl->plr->flags |= DDPF_NOCLIP;
        else
            pl->plr->flags &= ~DDPF_NOCLIP;
    }
}

void NetCl_UpdatePlayerState(byte *data, int plrNum)
{
    player_t *pl = &players[plrNum];
    byte    b;
    unsigned short flags;
    int     i;
    int     oldstate = pl->playerstate;
    unsigned short s;

    if(!Get(DD_GAME_READY))
        return;

    NetCl_SetReadBuffer(data);
    flags = NetCl_ReadShort();

    //Con_Printf("NetCl_UpdPlrState: fl=%x\n", flags);

    if(flags & PSF_STATE)       // and armor type (the same bit)
    {
        b = NetCl_ReadByte();
        pl->playerstate = b & 0xf;
#if __JDOOM__ || __JHERETIC__
        pl->armortype = b >> 4;
#endif
        // Set or clear the DEAD flag for this player.
        if(pl->playerstate == PST_LIVE)
            pl->plr->flags &= ~DDPF_DEAD;
        else
            pl->plr->flags |= DDPF_DEAD;
    }
    if(flags & PSF_HEALTH)
    {
        pl->health = NetCl_ReadByte();
        pl->plr->mo->health = pl->health;
    }
    if(flags & PSF_ARMOR_POINTS)
    {
#if __JHEXEN__
        for(i = 0; i < NUMARMOR; i++)
            pl->armorpoints[i] = NetCl_ReadByte();
#else
        pl->armorpoints = NetCl_ReadByte();
#endif
    }

#if __JHERETIC__ || __JHEXEN__ || __JSTRIFE__
    if(flags & PSF_INVENTORY)
    {
        pl->inventorySlotNum = NetCl_ReadByte();
        pl->artifactCount = 0;
        for(i = 0; i < NUMINVENTORYSLOTS; i++)
        {
            if(i >= pl->inventorySlotNum)
            {
                pl->inventory[i].type = arti_none;
                pl->inventory[i].count = 0;
                continue;
            }
            s = NetCl_ReadShort();
            pl->inventory[i].type = s & 0xff;
            pl->inventory[i].count = s >> 8;
            if(pl->inventory[i].type != arti_none)
                pl->artifactCount += pl->inventory[i].count;
        }
#  if __JHERETIC__
        if(plrNum == consoleplayer)
            P_InventoryCheckReadyArtifact(&players[consoleplayer]);
#  endif
    }
#endif

    if(flags & PSF_POWERS)
    {
        b = NetCl_ReadByte();
        // Only the non-zero powers are included in the message.
#if __JHEXEN__ || __JSTRIFE__
        for(i = 0; i < NUMPOWERS - 1; i++)
            if(b & (1 << i))
                pl->powers[i + 1] = NetCl_ReadByte() * 35;
            else
                pl->powers[i + 1] = 0;
#else
        for(i = 0; i < NUMPOWERS; i++)
        {
#  if __JDOOM__
            if(i == pw_ironfeet || i == pw_strength)
                continue;
#  endif
            if(b & (1 << i))
                pl->powers[i] = NetCl_ReadByte() * 35;
            else
                pl->powers[i] = 0;
        }
#endif
    }
    if(flags & PSF_KEYS)
    {
        b = NetCl_ReadByte();
#if __JDOOM__ | __JHERETIC__
        for(i = 0; i < NUMKEYS; i++)
            pl->keys[i] = (b & (1 << i)) != 0;
#endif
    }
    if(flags & PSF_FRAGS)
    {
        memset(pl->frags, 0, sizeof(pl->frags));
        // First comes the number of frag counts included.
        for(i = NetCl_ReadByte(); i > 0; i--)
        {
            s = NetCl_ReadShort();
            pl->frags[s >> 12] = s & 0xfff;
        }

        /*// A test...
           Con_Printf("Frags update: ");
           for(i=0; i<4; i++)
           Con_Printf("%i ", pl->frags[i]);
           Con_Printf("\n"); */
    }
    if(flags & PSF_OWNED_WEAPONS)
    {
        b = NetCl_ReadByte();
        for(i = 0; i < NUMWEAPONS; i++)
            pl->weaponowned[i] = (b & (1 << i)) != 0;
    }
    if(flags & PSF_AMMO)
    {
        for(i = 0; i < NUMAMMO; i++)
#if __JHEXEN__ || __JSTRIFE__
            pl->ammo[i] = NetCl_ReadByte();
#else
            pl->ammo[i] = NetCl_ReadShort();
#endif
    }
    if(flags & PSF_MAX_AMMO)
    {
#if __JDOOM__ || __JHERETIC__                   // Hexen has no use for max ammo.
        for(i = 0; i < NUMAMMO; i++)
            pl->maxammo[i] = NetCl_ReadShort();
#endif
    }
    if(flags & PSF_COUNTERS)
    {
        pl->killcount = NetCl_ReadShort();
        pl->itemcount = NetCl_ReadByte();
        pl->secretcount = NetCl_ReadByte();

        /*Con_Printf( "plr%i: kills=%i items=%i secret=%i\n", pl-players,
           pl->killcount, pl->itemcount, pl->secretcount); */
    }
    if(flags & PSF_PENDING_WEAPON || flags & PSF_READY_WEAPON)
    {
        b = NetCl_ReadByte();
        if(flags & PSF_PENDING_WEAPON)
        {
            pl->pendingweapon = b & 0xf;
        }
        if(flags & PSF_READY_WEAPON)
        {
            pl->readyweapon = b >> 4;

#if _DEBUG
            Con_Message("NetCl_UpdPlSt: rdyw=%i\n", pl->readyweapon);
#endif
        }
    }
    if(flags & PSF_VIEW_HEIGHT)
    {
        pl->plr->viewheight = NetCl_ReadByte() << 16;
    }

#if __JHERETIC || __JHEXEN__ || __JSTRIFE__
    if(flags & PSF_MORPH_TIME)
    {
        pl->morphTics = NetCl_ReadByte() * 35;
    }
#endif

#if __JHEXEN__ || __JSTRIFE__
    if(flags & PSF_LOCAL_QUAKE)
    {
        localQuakeHappening[plrNum] = NetCl_ReadByte();
    }
#endif

    if(oldstate != pl->playerstate && oldstate == PST_DEAD)
    {
        P_SetupPsprites(pl);
    }
}

void NetCl_UpdatePSpriteState(byte *data)
{
    unsigned short s;

    NetCl_SetReadBuffer(data);
    s = NetCl_ReadShort();
    P_SetPsprite(&players[consoleplayer], ps_weapon, s);
}

void NetCl_Intermission(byte *data)
{
    int     flags;

#ifdef __JHERETIC__
    extern int interstate;
    extern int intertime;
#endif
#if __JHEXEN__ || __JSTRIFE__
    extern int interstate;
    extern int LeaveMap, LeavePosition;
#endif

    NetCl_SetReadBuffer(data);
    flags = NetCl_ReadByte();

    //Con_Printf( "NetCl_Intermission: flags=%x\n", flags);

#ifdef __JDOOM__
    if(flags & IMF_BEGIN)
    {
        wminfo.maxkills = NetCl_ReadShort();
        wminfo.maxitems = NetCl_ReadShort();
        wminfo.maxsecret = NetCl_ReadShort();
        wminfo.next = NetCl_ReadByte();
        wminfo.last = NetCl_ReadByte();
        wminfo.didsecret = NetCl_ReadByte();

        G_PrepareWIData();

        gamestate = GS_INTERMISSION;
        viewactive = false;
        if(automapactive)
            AM_Stop();

        WI_Start(&wminfo);
    }
    if(flags & IMF_END)
    {
        WI_End();
    }
    if(flags & IMF_STATE)
    {
        WI_SetState(NetCl_ReadByte());
    }
#endif

#ifdef __JHERETIC__
    if(flags & IMF_STATE)
        interstate = (int) NetCl_ReadByte();
    if(flags & IMF_TIME)
        intertime = NetCl_ReadShort();
    if(flags & IMF_BEGIN)
    {
        gamestate = GS_INTERMISSION;
        IN_Start();
    }
    if(flags & IMF_END)
    {
        IN_Stop();
    }
#endif

#if __JHEXEN__ || __JSTRIFE__
    if(flags & IMF_BEGIN)
    {
        LeaveMap = NetCl_ReadByte();
        LeavePosition = NetCl_ReadByte();
        gamestate = GS_INTERMISSION;
        IN_Start();
    }
    if(flags & IMF_END)
    {
        IN_Stop();
    }
    if(flags & IMF_STATE)
        interstate = (int) NetCl_ReadByte();
#endif
}

/*
 * This is where clients start their InFine interludes.
 */
void NetCl_Finale(int packetType, byte *data)
{
    int     flags;
    int     len, numConds, i;
    byte   *script = NULL;

    NetCl_SetReadBuffer(data);
    flags = NetCl_ReadByte();
    if(flags & FINF_SCRIPT)
    {
        // First read the values of the conditions.
        if(packetType == GPT_FINALE2)
        {
            numConds = NetCl_ReadByte();
            for(i = 0; i < numConds; i++)
            {
                FI_SetCondition(i, NetCl_ReadByte());
            }
        }

        // Read the script into level-scope memory. It will be freed
        // when the next level is loaded.
        len = strlen((char*)readbuffer);
        script = Z_Malloc(len + 1, PU_LEVEL, 0);
        strcpy((char*)script, (char*)readbuffer);
    }
    if(flags & FINF_BEGIN && script)
    {
        // Start the script.
        FI_Start((char*)script,
                 flags & FINF_AFTER ? FIMODE_AFTER : flags & FINF_OVERLAY ?
                 FIMODE_OVERLAY : FIMODE_BEFORE);
    }
    if(flags & FINF_END)
    {
        // Stop InFine.
        FI_End();
    }
    if(flags & FINF_SKIP)
    {
        FI_SkipRequest();
    }
}

/*
 * Clients have other players' info, but it's only "FYI"; they don't
 * really need it.
 */
void NetCl_UpdatePlayerInfo(byte *data)
{
    int     num;

    NetCl_SetReadBuffer(data);
    num = NetCl_ReadByte();
    cfg.PlayerColor[num] = NetCl_ReadByte();
#if __JHEXEN__ || __JHERETIC__
    cfg.PlayerClass[num] = NetCl_ReadByte();
    players[num].class = cfg.PlayerClass[num];
    if(num == consoleplayer)
        SB_SetClassData();
#endif
#if __JDOOM__
    ST_updateGraphics();
#endif

#if __JDOOM__ || __JSTRIFE__
    Con_Printf("NetCl_UpdatePlayerInfo: pl=%i color=%i\n", num,
               cfg.PlayerColor[num]);
#else
    Con_Printf("NetCl_UpdatePlayerInfo: pl=%i color=%i class=%i\n", num,
               cfg.PlayerColor[num], cfg.PlayerClass[num]);
#endif
}

// Send consoleplayer's settings to the server.
void NetCl_SendPlayerInfo()
{
    byte    buffer[10], *ptr = buffer;

    if(!IS_CLIENT)
        return;

    *ptr++ = cfg.netColor;
#if __JHEXEN__
    *ptr++ = cfg.netClass;
#elif __JHERETIC__
    *ptr++ = PCLASS_PLAYER;
#endif
    Net_SendPacket(DDSP_ORDERED, GPT_PLAYER_INFO, buffer, ptr - buffer);
}

void NetCl_SaveGame(void *data)
{
    if(Get(DD_PLAYBACK))
        return;
    SV_SaveClient(*(unsigned int *) data);
#ifdef __JDOOM__
    P_SetMessage(&players[consoleplayer], TXT_GAMESAVED);
#endif
}

void NetCl_LoadGame(void *data)
{
    if(!IS_CLIENT)
        return;
    if(Get(DD_PLAYBACK))
        return;
    SV_LoadClient(*(unsigned int *) data);
    //  Net_SendPacket(DDSP_RELIABLE, GPT_LOAD, &con, 1);
#ifdef __JDOOM__
    P_SetMessage(&players[consoleplayer], GET_TXT(TXT_CLNETLOAD));
#endif
}

/*
 * Pause or unpause the game.
 */
void NetCl_Paused(boolean setPause)
{
    paused = (setPause != 0);
    DD_SetInteger(DD_CLIENT_PAUSED, paused);
}

/*
 * Write a DDPT_COMMANDS (32) packet. Returns a pointer to a static
 * buffer that contains the data (kludge to work around the parameter
 * passing from the engine).
 */
void *NetCl_WriteCommands(ticcmd_t *cmd, int count)
{
    static byte msg[1024];      // A shared buffer.
    ushort *size = (ushort *) msg;
    byte   *out = msg + 2, *flags, *start = out;
    ticcmd_t prev;
    int     i;
    byte   buttons;

    // Always compare against the previous command.
    memset(&prev, 0, sizeof(prev));

    for(i = 0; i < count; i++, cmd++)
    {
        flags = out++;
        *flags = 0;

        // What has changed?
        if(cmd->forwardMove != prev.forwardMove)
        {
            *flags |= CMDF_FORWARDMOVE;
            *out++ = cmd->forwardMove;
        }
        if(cmd->sideMove != prev.sideMove)
        {
            *flags |= CMDF_SIDEMOVE;
            *out++ = cmd->sideMove;
        }
        if(cmd->angle != prev.angle)
        {
            *flags |= CMDF_ANGLE;
            *(unsigned short *) out = SHORT(cmd->angle);
            out += 2;
        }
        if(cmd->pitch != prev.pitch)
        {
            *flags |= CMDF_LOOKDIR;
            *(short *) out = SHORT(cmd->pitch);
            out += 2;
        }

        // Compile the button flags.
        buttons = 0;
        if(cmd->attack)
            buttons |= CMDF_BTN_ATTACK;
        if(cmd->use)
            buttons |= CMDF_BTN_USE;
        if(cmd->jump)
            buttons |= CMDF_BTN_JUMP;
        if(cmd->pause)
            buttons |= CMDF_BTN_PAUSE;

        // Always include nonzero buttons.
        if(buttons != 0)
        {
            *flags |= CMDF_BUTTONS;
            *out++ = buttons;
        }

        if(cmd->fly != prev.fly)
        {
            *flags |= CMDF_LOOKFLY;
            *out++ = cmd->fly;
        }
        if(cmd->arti != prev.arti)
        {
            *flags |= CMDF_ARTI;
            *out++ = cmd->arti;
        }
        if(cmd->changeWeapon != prev.changeWeapon)
        {
            *flags |= CMDF_CHANGE_WEAPON;
            *(short *) out = SHORT(cmd->changeWeapon);
            out += 2;
        }

        memcpy(&prev, cmd, sizeof(*cmd));
    }

    // First two bytes contain the size of the buffer (not included in
    // the actual packet).
    *size = out - start;

    return msg;
}

/*
 * Send a GPT_CHEAT_REQUEST packet to the server. If the server is allowing
 * netgame cheating, the cheat will be executed on the server.
 */
void NetCl_CheatRequest(const char *command)
{
    char    msg[40];

    // Copy the cheat command into a NULL-terminated buffer.
    memset(msg, 0, sizeof(msg));
    strncpy(msg, command, sizeof(msg) - 1);

    if(IS_CLIENT)
        Net_SendPacket(DDSP_CONFIRM, GPT_CHEAT_REQUEST, msg, strlen(msg) + 1);
    else
        NetSv_DoCheat(consoleplayer, msg);
}

/*
 * Set the jump power used in client mode.
 */
void NetCl_UpdateJumpPower(void *data)
{
    netJumpPower = FLOAT( *(float *) data );
#ifdef _DEBUG
    Con_Printf("NetCl_UpdateJumpPower: %g\n", netJumpPower);
#endif
}
