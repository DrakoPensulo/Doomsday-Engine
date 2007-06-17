/**\file
 *\section License
 * License: Raven
 * Online License Link: http://www.dengine.net/raven_license/End_User_License_Hexen_Source_Code.html
 *
 *\author Copyright © 2003-2006 Jaakko Keränen <skyjake@dengine.net>
 *\author Copyright © 2006-2007 Daniel Swanson <danij@dengine.net>
 *\author Copyright © 1999 Activision
 *
 * This program is covered by the HERETIC / HEXEN (LIMITED USE) source
 * code license; you can redistribute it and/or modify it under the terms
 * of the HERETIC / HEXEN source code license as published by Activision.
 *
 * THIS MATERIAL IS NOT MADE OR SUPPORTED BY ACTIVISION.
 *
 * WARRANTY INFORMATION.
 * This program is provided as is. Activision and it's affiliates make no
 * warranties of any kind, whether oral or written , express or implied,
 * including any warranty of merchantability, fitness for a particular
 * purpose or non-infringement, and no other representations or claims of
 * any kind shall be binding on or obligate Activision or it's affiliates.
 *
 * LICENSE CONDITIONS.
 * You shall not:
 *
 * 1) Exploit this Program or any of its parts commercially.
 * 2) Use this Program, or permit use of this Program, on more than one
 *    computer, computer terminal, or workstation at the same time.
 * 3) Make copies of this Program or any part thereof, or make copies of
 *    the materials accompanying this Program.
 * 4) Use the program, or permit use of this Program, in a network,
 *    multi-user arrangement or remote access arrangement, including any
 *    online use, except as otherwise explicitly provided by this Program.
 * 5) Sell, rent, lease or license any copies of this Program, without
 *    the express prior written consent of Activision.
 * 6) Remove, disable or circumvent any proprietary notices or labels
 *    contained on or within the Program.
 *
 * You should have received a copy of the HERETIC / HEXEN source code
 * license along with this program (Ravenlic.txt); if not:
 * http://www.ravensoft.com/
 */

#ifdef _MSC_VER
#  pragma optimize("g", off)
#endif

// HEADER FILES ------------------------------------------------------------

#include <math.h>
#include "jhexen.h"

#include "../../../engine/portable/include/m_bams.h" // for BANG2RAD

#include "p_map.h"
#include "p_player.h"

// MACROS ------------------------------------------------------------------

#define MAX_TID_COUNT 200

#define MAX_BOB_OFFSET  0x80000

#define BLAST_RADIUS_DIST   255*FRACUNIT
#define BLAST_SPEED         20*FRACUNIT
#define BLAST_FULLSTRENGTH  255
#define HEAL_RADIUS_DIST    255*FRACUNIT

#define STOPSPEED           0x1000

#define FRICTION_NORMAL     0xe800
#define FRICTION_LOW        0xf900
#define FRICTION_FLY        0xeb00

#define SMALLSPLASHCLIP (12);

// TYPES -------------------------------------------------------------------

// EXTERNAL FUNCTION PROTOTYPES --------------------------------------------

void    G_PlayerReborn(int player);
void    P_MarkAsLeaving(mobj_t *corpse);
mobj_t *P_CheckOnMobj(mobj_t *thing);
void    P_BounceWall(mobj_t *mo);

// PUBLIC FUNCTION PROTOTYPES ----------------------------------------------

void    P_SpawnMapThing(thing_t * mthing);

// PRIVATE FUNCTION PROTOTYPES ---------------------------------------------

static void PlayerLandedOnThing(mobj_t *mo, mobj_t *onmobj);

// EXTERNAL DATA DECLARATIONS ----------------------------------------------

extern mobj_t LavaInflictor;

// PUBLIC DATA DEFINITIONS -------------------------------------------------

mobjtype_t PuffType;
mobj_t *MissileMobj;

fixed_t FloatBobOffsets[64] = {
    0, 51389, 102283, 152192,
    200636, 247147, 291278, 332604,
    370727, 405280, 435929, 462380,
    484378, 501712, 514213, 521763,
    524287, 521763, 514213, 501712,
    484378, 462380, 435929, 405280,
    370727, 332604, 291278, 247147,
    200636, 152192, 102283, 51389,
    -1, -51390, -102284, -152193,
    -200637, -247148, -291279, -332605,
    -370728, -405281, -435930, -462381,
    -484380, -501713, -514215, -521764,
    -524288, -521764, -514214, -501713,
    -484379, -462381, -435930, -405280,
    -370728, -332605, -291279, -247148,
    -200637, -152193, -102284, -51389
};

// PRIVATE DATA DEFINITIONS ------------------------------------------------

static int TIDList[MAX_TID_COUNT + 1];  // +1 for termination marker
static mobj_t *TIDMobj[MAX_TID_COUNT];

// CODE --------------------------------------------------------------------

/**
 * @return              <code>true</code> if the mobj is still present.
 */
boolean P_SetMobjState(mobj_t *mobj, statenum_t state)
{
    state_t    *st;

    if(state == S_NULL)
    {                           // Remove mobj
        mobj->state = (state_t *) S_NULL;
        P_RemoveMobj(mobj);
        return (false);
    }
    st = &states[state];

    P_SetState(mobj, state);
    mobj->turntime = false;     // $visangle-facetarget
    if(st->action)
    {                           // Call action function
        st->action(mobj);
    }

    // Return false if the action function removed the mobj.
    return mobj->thinker.function != (think_t) - 1;
}

/**
 * Same as P_SetMobjState, but does not call the state function.
 */
boolean P_SetMobjStateNF(mobj_t *mobj, statenum_t state)
{
    if(state == S_NULL)
    {   // Remove mobj
        mobj->state = (state_t *) S_NULL;
        P_RemoveMobj(mobj);
        return false;
    }

    mobj->turntime = false;     // $visangle-facetarget
    P_SetState(mobj, state);
    return true;
}

void P_ExplodeMissile(mobj_t *mo)
{
    mo->mom[MX] = mo->mom[MY] = mo->mom[MZ] = 0;
    P_SetMobjState(mo, mobjinfo[mo->type].deathstate);

    if(mo->flags & MF_MISSILE)
    {
        mo->flags &= ~MF_MISSILE;
        mo->flags |= MF_VIEWALIGN;
        if(mo->flags & MF_BRIGHTEXPLODE)
            mo->flags |= MF_BRIGHTSHADOW;
    }

    switch(mo->type)
    {
    case MT_SORCBALL1:
    case MT_SORCBALL2:
    case MT_SORCBALL3:
        S_StartSound(SFX_SORCERER_BIGBALLEXPLODE, NULL);
        break;

    case MT_SORCFX1:
        S_StartSound(SFX_SORCERER_HEADSCREAM, NULL);
        break;

    default:
        if(mo->info->deathsound)
            S_StartSound(mo->info->deathsound, mo);
        break;
    }
}

void P_FloorBounceMissile(mobj_t *mo)
{
    if(P_HitFloor(mo) >= FLOOR_LIQUID)
    {
        switch(mo->type)
        {
        case MT_SORCFX1:
        case MT_SORCBALL1:
        case MT_SORCBALL2:
        case MT_SORCBALL3:
            break;

        default:
            P_RemoveMobj(mo);
            return;
        }
    }

    switch(mo->type)
    {
    case MT_SORCFX1:
        mo->mom[MZ] = -mo->mom[MZ];   // no energy absorbed
        break;

    case MT_SGSHARD1:
    case MT_SGSHARD2:
    case MT_SGSHARD3:
    case MT_SGSHARD4:
    case MT_SGSHARD5:
    case MT_SGSHARD6:
    case MT_SGSHARD7:
    case MT_SGSHARD8:
    case MT_SGSHARD9:
    case MT_SGSHARD0:
        mo->mom[MZ] = FixedMul(mo->mom[MZ], -0.3 * FRACUNIT);
        if(abs(mo->mom[MZ]) < (FRACUNIT / 2))
        {
            P_SetMobjState(mo, S_NULL);
            return;
        }
        break;

    default:
        mo->mom[MZ] = FixedMul(mo->mom[MZ], -0.7 * FRACUNIT);
        break;
    }

    mo->mom[MX] = 2 * mo->mom[MX] / 3;
    mo->mom[MY] = 2 * mo->mom[MY] / 3;
    if(mo->info->seesound)
    {
        switch(mo->type)
        {
        case MT_SORCBALL1:
        case MT_SORCBALL2:
        case MT_SORCBALL3:
            if(!mo->args[0])
                S_StartSound(mo->info->seesound, mo);
            break;

        default:
            S_StartSound(mo->info->seesound, mo);
            break;
        }

        S_StartSound(mo->info->seesound, mo);
    }
}

void P_ThrustMobj(mobj_t *mo, angle_t angle, fixed_t move)
{
    angle >>= ANGLETOFINESHIFT;
    mo->mom[MX] += FixedMul(move, finecosine[angle]);
    mo->mom[MY] += FixedMul(move, finesine[angle]);
}

/**
 * @param delta         The amount 'source' needs to turn.
 *
 * @return              <code>1</code> if 'source' needs to turn
 *                      clockwise, or <code>0</code> if 'source' needs
 *                      to turn counter clockwise.
 */
int P_FaceMobj(mobj_t *source, mobj_t *target, angle_t *delta)
{
    angle_t     diff, angle1, angle2;

    angle1 = source->angle;
    angle2 = R_PointToAngle2(source->pos[VX], source->pos[VY],
                             target->pos[VX], target->pos[VY]);
    if(angle2 > angle1)
    {
        diff = angle2 - angle1;
        if(diff > ANGLE_180)
        {
            *delta = ANGLE_MAX - diff;
            return 0;
        }
        else
        {
            *delta = diff;
            return 1;
        }
    }
    else
    {
        diff = angle1 - angle2;
        if(diff > ANGLE_180)
        {
            *delta = ANGLE_MAX - diff;
            return 1;
        }
        else
        {
            *delta = diff;
            return 0;
        }
    }
}

/**
 * The missile tracer field must be mobj_t *target.
 *
 * @return              <code>true</code> if target was tracked.
 */
boolean P_SeekerMissile(mobj_t *actor, angle_t thresh, angle_t turnMax)
{
    int         dir, dist;
    angle_t     delta, angle;
    mobj_t     *target;

    target = actor->tracer;
    if(target == NULL)
        return false;

    if(!(target->flags & MF_SHOOTABLE))
    {   // Target died
        actor->tracer = NULL;
        return false;
    }

    dir = P_FaceMobj(actor, target, &delta);
    if(delta > thresh)
    {
        delta >>= 1;
        if(delta > turnMax)
        {
            delta = turnMax;
        }
    }

    if(dir)                       // Turn clockwise
        actor->angle += delta;
    else                          // Turn counter clockwise
        actor->angle -= delta;

    angle = actor->angle >> ANGLETOFINESHIFT;
    actor->mom[MX] = FixedMul(actor->info->speed, finecosine[angle]);
    actor->mom[MY] = FixedMul(actor->info->speed, finesine[angle]);

    if(actor->pos[VZ] + FLT2FIX(actor->height) < target->pos[VZ] ||
       target->pos[VZ] + FLT2FIX(target->height) < actor->pos[VZ])
    {   // Need to seek vertically
        dist = P_ApproxDistance(target->pos[VX] - actor->pos[VX],
                                target->pos[VY] - actor->pos[VY]);
        dist = dist / actor->info->speed;
        if(dist < 1)
            dist = 1;

        actor->mom[MZ] =
            (target->pos[VZ] + FLT2FIX(target->height /2) -
             (actor->pos[VZ] + FLT2FIX(actor->height /2))) / dist;
    }

    return true;
}

int P_GetMobjFriction(mobj_t *mo)
{
    if((mo->flags2 & MF2_FLY) &&
       !(mo->pos[VZ] <= FLT2FIX(mo->floorz)) &&
       !(mo->flags2 & MF2_ONMOBJ))
    {
        return FRICTION_FLY;
    }
    else if(P_GetThingFloorType(mo) == FLOOR_ICE)
    {
        return FRICTION_LOW;
    }

    return FRICTION_NORMAL;
}

void P_XYMovement(mobj_t *mo)
{
    fixed_t     ptryx, ptryy;
    player_t   *player;
    fixed_t     xmove, ymove;
    int         special;
    angle_t     angle;
    static int  windTab[3] = { 2048 * 5, 2048 * 10, 2048 * 25 };

    // $democam: cameramen have their own movement code
    if(P_CameraXYMovement(mo))
        return;

    if(!mo->mom[MX] && !mo->mom[MY])
    {
        if(mo->flags & MF_SKULLFLY)
        {   // A flying mobj slammed into something
            mo->flags &= ~MF_SKULLFLY;
            mo->mom[MX] = mo->mom[MY] = mo->mom[MZ] = 0;
            P_SetMobjState(mo, mo->info->seestate);
        }
        return;
    }

    special = P_XSectorOfSubsector(mo->subsector)->special;
    if(mo->flags2 & MF2_WINDTHRUST)
    {
        switch(special)
        {
        case 40:
        case 41:
        case 42: // Wind_East
            P_ThrustMobj(mo, 0, windTab[special - 40]);
            break;

        case 43:
        case 44:
        case 45: // Wind_North
            P_ThrustMobj(mo, ANG90, windTab[special - 43]);
            break;

        case 46:
        case 47:
        case 48: // Wind_South
            P_ThrustMobj(mo, ANG270, windTab[special - 46]);
            break;

        case 49:
        case 50:
        case 51: // Wind_West
            P_ThrustMobj(mo, ANG180, windTab[special - 49]);
            break;
        }
    }
    player = mo->player;

    if(mo->mom[MX] > MAXMOVE)
        mo->mom[MX] = MAXMOVE;
    else if(mo->mom[MX] < -MAXMOVE)
        mo->mom[MX] = -MAXMOVE;

    if(mo->mom[MY] > MAXMOVE)
        mo->mom[MY] = MAXMOVE;
    else if(mo->mom[MY] < -MAXMOVE)
        mo->mom[MY] = -MAXMOVE;

    xmove = mo->mom[MX];
    ymove = mo->mom[MY];
    do
    {
        if(xmove > MAXMOVE / 2 || ymove > MAXMOVE / 2)
        {
#if 1
            ptryx = mo->pos[VX] + xmove / 2;
            ptryy = mo->pos[VY] + ymove / 2;
            xmove >>= 1;
            ymove >>= 1;
#else
            xmove >>= 1;
            ymove >>= 1;
            ptryx = mo->pos[VX] + xmove;
            ptryy = mo->pos[VY] + ymove;
#endif
        }
        else
        {
            ptryx = mo->pos[VX] + xmove;
            ptryy = mo->pos[VY] + ymove;
            xmove = ymove = 0;
        }

        if(!P_TryMove(mo, ptryx, ptryy))
        {   // Blocked move
            if(mo->flags2 & MF2_SLIDE)
            {   // Try to slide along it
                if(BlockingMobj == NULL)
                {   // Slide against wall
                    P_SlideMove(mo);
                }
                else
                {   // Slide against mobj
                    if(P_TryMove(mo, mo->pos[VX], ptryy))
                    {
                        mo->mom[MX] = 0;
                    }
                    else if(P_TryMove(mo, ptryx, mo->pos[VY]))
                    {
                        mo->mom[MY] = 0;
                    }
                    else
                    {
                        mo->mom[MX] = mo->mom[MY] = 0;
                    }
                }
            }
            else if(mo->flags & MF_MISSILE)
            {
                if(mo->flags2 & MF2_FLOORBOUNCE)
                {
                    if(BlockingMobj)
                    {
                        if((BlockingMobj->flags2 & MF2_REFLECTIVE) ||
                           ((!BlockingMobj->player) &&
                            (!(BlockingMobj->flags & MF_COUNTKILL))))
                        {
                            fixed_t speed;

                            angle =
                                R_PointToAngle2(BlockingMobj->pos[VX],
                                                BlockingMobj->pos[VY],
                                                mo->pos[VX], mo->pos[VY]) +
                                ANGLE_1 * ((P_Random() % 16) - 8);

                            speed = P_ApproxDistance(mo->mom[MX], mo->mom[MY]);
                            speed = FixedMul(speed, 0.75 * FRACUNIT);
                            mo->angle = angle;
                            angle >>= ANGLETOFINESHIFT;
                            mo->mom[MX] = FixedMul(speed, finecosine[angle]);
                            mo->mom[MY] = FixedMul(speed, finesine[angle]);
                            if(mo->info->seesound)
                                S_StartSound(mo->info->seesound, mo);

                            return;
                        }
                        else
                        {   // Struck a player/creature
                            P_ExplodeMissile(mo);
                        }
                    }
                    else
                    {   // Struck a wall
                        P_BounceWall(mo);
                        switch(mo->type)
                        {
                        case MT_SORCBALL1:
                        case MT_SORCBALL2:
                        case MT_SORCBALL3:
                        case MT_SORCFX1:
                            break;

                        default:
                            if(mo->info->seesound)
                                S_StartSound(mo->info->seesound, mo);
                            break;
                        }

                        return;
                    }
                }
                if(BlockingMobj && (BlockingMobj->flags2 & MF2_REFLECTIVE))
                {
                    angle =
                        R_PointToAngle2(BlockingMobj->pos[VX], BlockingMobj->pos[VY],
                                        mo->pos[VX], mo->pos[VY]);

                    // Change angle for delflection/reflection
                    switch(BlockingMobj->type)
                    {
                    case MT_CENTAUR:
                    case MT_CENTAURLEADER:
                        if(abs(angle - BlockingMobj->angle) >> 24 > 45)
                            goto explode;
                        if(mo->type == MT_HOLY_FX)
                            goto explode;
                        // Drop through to sorcerer full reflection
                    case MT_SORCBOSS:
                        // Deflection
                        if(P_Random() < 128)
                            angle += ANGLE_45;
                        else
                            angle -= ANGLE_45;
                        break;

                    default:
                        // Reflection
                        angle += ANGLE_1 * ((P_Random() % 16) - 8);
                        break;
                    }

                    // Reflect the missile along angle
                    mo->angle = angle;
                    angle >>= ANGLETOFINESHIFT;
                    mo->mom[MX] =
                        FixedMul(mo->info->speed >> 1, finecosine[angle]);
                    mo->mom[MY] = FixedMul(mo->info->speed >> 1, finesine[angle]);

                    if(mo->flags2 & MF2_SEEKERMISSILE)
                    {
                        mo->tracer = mo->target;
                    }
                    mo->target = BlockingMobj;

                    return;
                }
              explode:
                // Explode a missile
                if(ceilingline && P_GetPtrp(ceilingline, DMU_BACK_SECTOR) &&
                   P_GetIntp(P_GetPtrp(ceilingline, DMU_BACK_SECTOR),
                             DMU_CEILING_TEXTURE) == skyflatnum)
                {   // Hack to prevent missiles exploding against the sky
                    if(mo->type == MT_BLOODYSKULL)
                    {
                        mo->mom[MX] = mo->mom[MY] = 0;
                        mo->mom[MZ] = -FRACUNIT;
                    }
                    else if(mo->type == MT_HOLY_FX)
                    {
                        P_ExplodeMissile(mo);
                    }
                    else
                    {
                        P_RemoveMobj(mo);
                    }
                    return;
                }

                P_ExplodeMissile(mo);
            }
            else
            {
                mo->mom[MX] = mo->mom[MY] = 0;
            }
        }
    } while(xmove || ymove);

    // Friction
    if(player && (P_GetPlayerCheats(player) & CF_NOMOMENTUM))
    {   // Debug option for no sliding at all
        mo->mom[MX] = mo->mom[MY] = 0;
        return;
    }
    if(mo->flags & (MF_MISSILE | MF_SKULLFLY))
        return; // No friction for missiles

    if(mo->pos[VZ] > FLT2FIX(mo->floorz) && !(mo->flags2 & MF2_FLY) &&
       !(mo->flags2 & MF2_ONMOBJ))
    {    // No friction when falling
        if(mo->type != MT_BLASTEFFECT)
            return;
    }

    if(mo->flags & MF_CORPSE)
    {   // Don't stop sliding if halfway off a step with some momentum
        if(mo->mom[MX] > FRACUNIT / 4 || mo->mom[MX] < -FRACUNIT / 4 ||
           mo->mom[MY] > FRACUNIT / 4 || mo->mom[MY] < -FRACUNIT / 4)
        {
            if(mo->floorz != P_GetFloatp(mo->subsector, DMU_FLOOR_HEIGHT))
            {
                return;
            }
        }
    }

    if(mo->mom[MX] > -STOPSPEED && mo->mom[MX] < STOPSPEED &&
       mo->mom[MY] > -STOPSPEED && mo->mom[MY] < STOPSPEED &&
       (!player || (player->plr->cmd.forwardMove == 0 &&
                    player->plr->cmd.sideMove == 0)))
    {   // If in a walking frame, stop moving
        if(player)
        {
            if((unsigned)
               ((player->plr->mo->state - states) - PCLASS_INFO(player->class)->runstate) <
               4)
            {
                P_SetMobjState(player->plr->mo, PCLASS_INFO(player->class)->normalstate);
            }
        }
        mo->mom[MX] = 0;
        mo->mom[MY] = 0;
    }
    else
    {
        int     friction = P_GetMobjFriction(mo);

        mo->mom[MX] = FixedMul(mo->mom[MX], friction);
        mo->mom[MY] = FixedMul(mo->mom[MY], friction);
    }
}

/**
 * \todo Move this to p_inter ***
 */
void P_MonsterFallingDamage(mobj_t *mo)
{
    int         damage;
    int         mom;

    mom = abs(mo->mom[MZ]);
    if(mom > 35 * FRACUNIT)
    {    // automatic death
        damage = 10000;
    }
    else
    {
        damage = ((mom - (23 * FRACUNIT)) * 6) >> FRACBITS;
    }
    damage = 10000; // always kill 'em.

    P_DamageMobj(mo, NULL, NULL, damage);
}

void P_ZMovement(mobj_t *mo)
{
    int         dist, delta;

    // $democam: cameramen get special z movement
    if(P_CameraZMovement(mo))
        return;

    // Check for smooth step up.
    if(mo->player && FIX2FLT(mo->pos[VZ]) < mo->floorz)
    {
        mo->player->plr->viewheight -= mo->floorz - FIX2FLT(mo->pos[VZ]);
        mo->player->plr->deltaviewheight =
            FIX2FLT(((cfg.plrViewHeight << FRACBITS) - FLT2FIX(mo->player->plr->viewheight)) >> 3);
    }

    // Adjust height.
    mo->pos[VZ] += mo->mom[MZ];
    if(mo->flags & MF_FLOAT && mo->target && !P_IsCamera(mo->target))
    {
        // Float down towards target if too close.
        if(!(mo->flags & MF_SKULLFLY) && !(mo->flags & MF_INFLOAT))
        {
            dist =
                P_ApproxDistance(mo->pos[VX] - mo->target->pos[VX],
                                 mo->pos[VY] - mo->target->pos[VY]);
            delta = (mo->target->pos[VZ] + FLT2FIX(mo->height /2)) - mo->pos[VZ];
            if(delta < 0 && dist < -(delta * 3))
            {
                mo->pos[VZ] -= FLOATSPEED;
                P_SetThingSRVOZ(mo, -FLOATSPEED);
            }
            else if(delta > 0 && dist < (delta * 3))
            {
                mo->pos[VZ] += FLOATSPEED;
                P_SetThingSRVOZ(mo, FLOATSPEED);
            }
        }
    }

    if(mo->player && mo->flags2 & MF2_FLY && !(mo->pos[VZ] <= FLT2FIX(mo->floorz)) &&
       leveltime & 2)
    {
        mo->pos[VZ] += finesine[(FINEANGLES / 20 * leveltime >> 2) & FINEMASK];
    }

    // Clip movement.
    if(mo->pos[VZ] <= FLT2FIX(mo->floorz))
    {   // Hit the floor
        if(mo->flags & MF_MISSILE)
        {
            mo->pos[VZ] = FLT2FIX(mo->floorz);
            if(mo->flags2 & MF2_FLOORBOUNCE)
            {
                P_FloorBounceMissile(mo);
                return;
            }
            else if(mo->type == MT_HOLY_FX)
            {   // The spirit struck the ground
                mo->mom[MZ] = 0;
                P_HitFloor(mo);
                return;
            }
            else if(mo->type == MT_MNTRFX2 || mo->type == MT_LIGHTNING_FLOOR)
            {   // Minotaur floor fire can go up steps
                return;
            }
            else
            {
                P_HitFloor(mo);
                P_ExplodeMissile(mo);
                return;
            }
        }

        if(mo->flags & MF_COUNTKILL)
        {
            // Blasted mobj falling
            if(mo->mom[MZ] < -(23 * FRACUNIT))
            {
                P_MonsterFallingDamage(mo);
            }
        }

        if(mo->pos[VZ] - mo->mom[MZ] > FLT2FIX(mo->floorz))
        {   // Spawn splashes, etc.
            P_HitFloor(mo);
        }

        mo->pos[VZ] = FLT2FIX(mo->floorz);
        if(mo->mom[MZ] < 0)
        {
            if(mo->flags2 & MF2_ICEDAMAGE && mo->mom[MZ] < -GRAVITY * 8)
            {
                mo->tics = 1;
                mo->mom[MX] = 0;
                mo->mom[MY] = 0;
                mo->mom[MZ] = 0;
                return;
            }

            if(mo->player)
            {
                mo->player->jumptics = 7; // delay any jumping for a short time
                if(mo->mom[MZ] < -GRAVITY * 8 && !(mo->flags2 & MF2_FLY))
                {   // Squat down.
                    mo->player->plr->deltaviewheight = FIX2FLT(mo->mom[MZ] >> 3);
                    if(mo->mom[MZ] < -23 * FRACUNIT)
                    {
                        P_FallingDamage(mo->player);
                        P_NoiseAlert(mo, mo);
                    }
                    else if(mo->mom[MZ] < -GRAVITY * 12 && !mo->player->morphTics)
                    {
                        S_StartSound(SFX_PLAYER_LAND, mo);

                        // Fix DOOM bug - dead players grunting when hitting the ground
                        // (e.g., after an archvile attack)
                        if(mo->player->health > 0)
                            switch(mo->player->class)
                            {
                            case PCLASS_FIGHTER:
                                S_StartSound(SFX_PLAYER_FIGHTER_GRUNT, mo);
                                break;

                            case PCLASS_CLERIC:
                                S_StartSound(SFX_PLAYER_CLERIC_GRUNT, mo);
                                break;

                            case PCLASS_MAGE:
                                S_StartSound(SFX_PLAYER_MAGE_GRUNT, mo);
                                break;

                            default:
                                break;
                            }
                    }
                    else if((P_GetThingFloorType(mo) < FLOOR_LIQUID) &&
                            (!mo->player->morphTics))
                    {
                        S_StartSound(SFX_PLAYER_LAND, mo);
                    }

                    if(!cfg.usemlook)
                        mo->player->centering = true;
                }
            }
            else if(mo->type >= MT_POTTERY1 && mo->type <= MT_POTTERY3)
            {
                P_DamageMobj(mo, NULL, NULL, 25);
            }
            else if(mo->flags & MF_COUNTKILL)
            {
                if(mo->mom[MZ] < -23 * FRACUNIT)
                {
                    // Doesn't get here
                }
            }
            mo->mom[MZ] = 0;
        }

        if(mo->flags & MF_SKULLFLY)
        {   // The skull slammed into something
            mo->mom[MZ] = -mo->mom[MZ];
        }

        if(mo->info->crashstate && (mo->flags & MF_CORPSE) &&
           !(mo->flags2 & MF2_ICEDAMAGE))
        {
            P_SetMobjState(mo, mo->info->crashstate);
            return;
        }
    }
    else if(mo->flags2 & MF2_LOGRAV)
    {
        if(mo->mom[MZ] == 0)
            mo->mom[MZ] = -(GRAVITY >> 3) * 2;
        else
            mo->mom[MZ] -= GRAVITY >> 3;
    }
    else if(!(mo->flags & MF_NOGRAVITY))
    {
        if(mo->mom[MZ] == 0)
            mo->mom[MZ] = -GRAVITY * 2;
        else
            mo->mom[MZ] -= GRAVITY;
    }

    if(FIX2FLT(mo->pos[VZ]) + mo->height > mo->ceilingz)
    {   // hit the ceiling
        if(mo->mom[MZ] > 0)
            mo->mom[MZ] = 0;

        mo->pos[VZ] = FLT2FIX(mo->ceilingz - mo->height);
        if(mo->flags2 & MF2_FLOORBOUNCE)
        {
            // Maybe reverse momentum here for ceiling bounce
            // Currently won't happen
            if(mo->info->seesound)
            {
                S_StartSound(mo->info->seesound, mo);
            }
            return;
        }

        if(mo->flags & MF_SKULLFLY)
        {   // the skull slammed into something
            mo->mom[MZ] = -mo->mom[MZ];
        }

        if(mo->flags & MF_MISSILE)
        {
            if(mo->type == MT_LIGHTNING_CEILING)
                return;

            if(P_GetIntp(mo->subsector, DMU_CEILING_TEXTURE) == skyflatnum)
            {
                if(mo->type == MT_BLOODYSKULL)
                {
                    mo->mom[MX] = mo->mom[MY] = 0;
                    mo->mom[MZ] = -FRACUNIT;
                }
                else if(mo->type == MT_HOLY_FX)
                {
                    P_ExplodeMissile(mo);
                }
                else
                {
                    P_RemoveMobj(mo);
                }

                return;
            }

            P_ExplodeMissile(mo);
            return;
        }
    }
}

void P_BlasterMobjThinker(mobj_t *mobj)
{
    int         i;
    fixed_t     z, frac[3];
    boolean     changexy;
    mobj_t     *mo;

#ifdef TIC_DEBUG
    FUNTAG("P_BlasterMobjThinker");

    if(rndDebugfile)
        fprintf(rndDebugfile, " -- z: %f, floorz: %f, mom: %f, %f, %f\n",
                FIX2FLT(mobj->pos[VZ]), FIX2FLT(mobj->floorz), FIX2FLT(mobj->mom[MX]),
                FIX2FLT(mobj->mom[MY]), FIX2FLT(mobj->mom[MZ]));
#endif

    // Handle movement
    if(mobj->mom[MX] || mobj->mom[MY] || (mobj->pos[VZ] != FLT2FIX(mobj->floorz)) || mobj->mom[MZ])
    {
        frac[VX] = mobj->mom[MX] >> 3;
        frac[VY] = mobj->mom[MY] >> 3;
        frac[VZ] = mobj->mom[MZ] >> 3;
        changexy = (frac[VX] || frac[VY]);

        for(i = 0; i < 8; ++i)
        {
            if(changexy)
            {
                if(!P_TryMove(mobj, mobj->pos[VX] + frac[VX],
                                    mobj->pos[VY] + frac[VY]))
                {   // Blocked move
                    P_ExplodeMissile(mobj);
#ifdef TIC_DEBUG
                    FUNTAG(" + missile exploded");
#endif
                    return;
                }
            }

            mobj->pos[VZ] += frac[VZ];
            if(mobj->pos[VZ] <= FLT2FIX(mobj->floorz))
            {   // Hit the floor
#ifdef TIC_DEBUG
                FUNTAG(" + floor hit");
#endif

                mobj->pos[VZ] = FLT2FIX(mobj->floorz);
                P_HitFloor(mobj);
                P_ExplodeMissile(mobj);
                return;
            }

            if(FIX2FLT(mobj->pos[VZ]) + mobj->height > mobj->ceilingz)
            {   // Hit the ceiling
                mobj->pos[VZ] = FLT2FIX(mobj->ceilingz - mobj->height);
                P_ExplodeMissile(mobj);
#ifdef TIC_DEBUG
                FUNTAG(" + ceiling hit");
#endif
                return;
            }

            if(changexy)
            {
                if(mobj->type == MT_MWAND_MISSILE && (P_Random() < 128))
                {
                    z = mobj->pos[VZ] - 8 * FRACUNIT;
                    if(z < FLT2FIX(mobj->floorz))
                    {
                        z = FLT2FIX(mobj->floorz);
                    }
                    P_SpawnMobj(mobj->pos[VX], mobj->pos[VY], z, MT_MWANDSMOKE);
                }
                else if(!--mobj->special1)
                {
                    mobj->special1 = 4;
                    z = mobj->pos[VZ] - 12 * FRACUNIT;
                    if(z < FLT2FIX(mobj->floorz))
                    {
                        z = FLT2FIX(mobj->floorz);
                    }

                    mo = P_SpawnMobj(mobj->pos[VX], mobj->pos[VY], z, MT_CFLAMEFLOOR);
                    if(mo)
                    {
                        mo->angle = mobj->angle;
                    }
                }
            }
        }
    }

    // Advance the state
    if(mobj->tics != -1)
    {
        mobj->tics--;
        while(!mobj->tics)
        {
            if(!P_SetMobjState(mobj, mobj->state->nextstate))
            {                   // mobj was removed
#ifdef TIC_DEBUG
                FUNTAG(" + mobj removed");
#endif
                return;
            }
        }
    }
#ifdef TIC_DEBUG
    FUNTAG(" + end");
#endif
}

static void PlayerLandedOnThing(mobj_t *mo, mobj_t *onmobj)
{
    mo->player->plr->deltaviewheight = FIX2FLT(mo->mom[MZ] >> 3);
    if(mo->mom[MZ] < -23 * FRACUNIT)
    {
        P_FallingDamage(mo->player);
        P_NoiseAlert(mo, mo);
    }
    else if(mo->mom[MZ] < -GRAVITY * 12 && !mo->player->morphTics)
    {
        S_StartSound(SFX_PLAYER_LAND, mo);
        switch(mo->player->class)
        {
        case PCLASS_FIGHTER:
            S_StartSound(SFX_PLAYER_FIGHTER_GRUNT, mo);
            break;

        case PCLASS_CLERIC:
            S_StartSound(SFX_PLAYER_CLERIC_GRUNT, mo);
            break;

        case PCLASS_MAGE:
            S_StartSound(SFX_PLAYER_MAGE_GRUNT, mo);
            break;

        default:
            break;
        }
    }
    else if(!mo->player->morphTics)
    {
        S_StartSound(SFX_PLAYER_LAND, mo);
    }

    // Lookspring is stupid when mouselook is on (and not in demo).
    if(!cfg.usemlook) // || demorecording || demoplayback)
        mo->player->centering = true;
}

void P_MobjThinker(mobj_t *mobj)
{
    mobj_t *onmo;

    if(mobj->ddflags & DDMF_REMOTE) // Remote mobjs are handled separately.
        return;

    // The first three bits of the selector special byte contain a
    // relative health level.
    P_UpdateHealthBits(mobj);

    // Handle X and Y momentums
    BlockingMobj = NULL;
    if(mobj->mom[MX] || mobj->mom[MY] || (mobj->flags & MF_SKULLFLY))
    {
        P_XYMovement(mobj);
        if(mobj->thinker.function == (think_t) - 1)
        {   // mobj was removed
            return;
        }
    }
    else if(mobj->flags2 & MF2_BLASTED)
    {   // Reset to not blasted when momentums are gone
        ResetBlasted(mobj);
    }

    if(mobj->flags2 & MF2_FLOATBOB)
    {
        // Floating item bobbing motion (special1 is height)
        /*mobj->z = mobj->floorz +
           mobj->special1 +
           FloatBobOffsets[(mobj->health++)&63]; */

        // Keep it on the floor.
        mobj->pos[VZ] = FLT2FIX(mobj->floorz);

        // Negative floorclip raises the mobj off the floor.
        mobj->floorclip = FIX2FLT(-mobj->special1);
        if(mobj->floorclip < FIX2FLT(-MAX_BOB_OFFSET))
        {
            // We don't want it going through the floor.
            mobj->floorclip = FIX2FLT(-MAX_BOB_OFFSET);
        }

        // Old floatbob used health as index, let's still increase it
        // as before (in case somebody wants to use it).
        mobj->health++;

        // HACK: If we are bobbing into the floor, raise special1 height.
        // Only affects Weapon Pieces.
        /*if(mobj->type >= MT_FW_SWORD1
           && mobj->type <= MT_MW_STAFF3
           && mobj->z < mobj->floorz)
           {
           mobj->special1 += mobj->floorz - mobj->z;
           } */
    }
    else if((mobj->pos[VZ] != FLT2FIX(mobj->floorz)) || mobj->mom[MZ] || BlockingMobj)
    {   // Handle Z momentum and gravity
        if(mobj->flags2 & MF2_PASSMOBJ)
        {
            onmo = P_CheckOnMobj(mobj);
            if(!onmo)
            {
                P_ZMovement(mobj);
                if(mobj->player && mobj->flags & MF2_ONMOBJ)
                {
                    mobj->flags2 &= ~MF2_ONMOBJ;
                }
            }
            else
            {
                if(mobj->player)
                {
                    if(mobj->mom[MZ] < -GRAVITY * 8 && !(mobj->flags2 & MF2_FLY))
                    {
                        PlayerLandedOnThing(mobj, onmo);
                    }

                    if(onmo->pos[VZ] + FLT2FIX(onmo->height) - mobj->pos[VZ] <= 24 * FRACUNIT)
                    {
                        mobj->player->plr->viewheight -=
                            FIX2FLT(onmo->pos[VZ]) + onmo->height - FIX2FLT(mobj->pos[VZ]);
                        mobj->player->plr->deltaviewheight =
                            FIX2FLT(((cfg.plrViewHeight << FRACBITS) - FLT2FIX(mobj->player->plr->viewheight)) >> 3);
                        mobj->pos[VZ] = onmo->pos[VZ] + FLT2FIX(onmo->height);
                        mobj->flags2 |= MF2_ONMOBJ;
                        mobj->mom[MZ] = 0;
                    }
                    else
                    {   // hit the bottom of the blocking mobj
                        mobj->mom[MZ] = 0;
                    }
                }

                /* Landing on another player, and mimicking his movements
                   if(mobj->player && onmo->player)
                   {
                   mobj->mom[MX] = onmo->mom[MX];
                   mobj->mom[MY] = onmo->mom[MY];
                   if(onmo->z < onmo->floorz)
                   {
                   mobj->z += onmo->floorz-onmo->z;
                   if(onmo->player)
                   {
                   onmo->player->plr->viewheight -= onmo->floorz-onmo->z;
                   onmo->player->plr->deltaviewheight = (VIEWHEIGHT-
                   onmo->player->plr->viewheight)>>3;
                   }
                   onmo->z = onmo->floorz;
                   }
                   }
                 */
            }
        }
        else
        {
            P_ZMovement(mobj);
        }

        if(mobj->thinker.function == (think_t) - 1)
        {   // mobj was removed
            return;
        }
    }

    // Cycle through states, calling action functions at transitions
    if(mobj->tics != -1)
    {
        //int     oldsprite = mobj->state->sprite;

        mobj->tics--;
        P_SRVOAngleTicker(mobj);
        // You can cycle through multiple states in a tic.
        while(!mobj->tics)
        {
            P_ClearThingSRVO(mobj);
            if(!P_SetMobjState(mobj, mobj->state->nextstate))
            {                   // mobj was removed
                return;
            }
        }

        // See what the next frame is.
        /*if(mobj->state->nextstate != S_NULL)
           {
           state_t *st = &states[mobj->state->nextstate];
           if(oldsprite == st->sprite)
           {
           mobj->nextframe = st->frame;
           }
           else
           {
           mobj->nextframe = -1;
           }
           mobj->nexttime = mobj->tics;
           } */
    }

    // Ice corpses aren't going anywhere.
    if(mobj->flags & MF_ICECORPSE)
        P_SetThingSRVO(mobj, 0, 0);
    /*else
       {
       // There definitely is no next frame in this case (tics==-1).
       mobj->nextframe = -1;
       } */
}

mobj_t *P_SpawnMobj(fixed_t x, fixed_t y, fixed_t z, mobjtype_t type)
{
    mobj_t     *mobj;
    mobjinfo_t *info;
    fixed_t     space;

    mobj = Z_Malloc(sizeof(*mobj), PU_LEVEL, NULL);
    memset(mobj, 0, sizeof(*mobj));

    info = &mobjinfo[type];
    mobj->type = type;
    mobj->info = info;
    mobj->pos[VX] = x;
    mobj->pos[VY] = y;
    mobj->radius = info->radius;
    mobj->height = FIX2FLT(info->height);
    mobj->flags = info->flags;
    mobj->flags2 = info->flags2;
    mobj->flags3 = info->flags3;
    mobj->damage = info->damage; // this doesn't appear to actually be used
    // see P_DamageMobj in P_inter.c

    // Let the engine know about solid objects.
    if(mobj->flags & MF_SOLID)
        mobj->ddflags |= DDMF_SOLID;
    if(mobj->flags2 & MF2_DONTDRAW)
        mobj->ddflags |= DDMF_DONTDRAW;

    mobj->health =
        info->spawnhealth * (IS_NETGAME ? cfg.netMobHealthModifier : 1);
    if(gameskill != SM_NIGHTMARE)
    {
        mobj->reactiontime = info->reactiontime;
    }
    mobj->lastlook = P_Random() % MAXPLAYERS;

    // Must link before setting state.
    mobj->thinker.function = P_MobjThinker;
    P_AddThinker(&mobj->thinker);

    P_SetState(mobj, info->spawnstate);

    // Set subsector and/or block links.
    P_SetThingPosition(mobj);
    mobj->floorz = P_GetFloatp(mobj->subsector, DMU_FLOOR_HEIGHT);
    mobj->ceilingz = P_GetFloatp(mobj->subsector, DMU_CEILING_HEIGHT);
    if(z == ONFLOORZ)
    {
        mobj->pos[VZ] = FLT2FIX(mobj->floorz);
    }
    else if(z == ONCEILINGZ)
    {
        mobj->pos[VZ] = FLT2FIX(mobj->ceilingz) - mobj->info->height;
    }
    else if(z == FLOATRANDZ)
    {
        space = FLT2FIX(mobj->ceilingz) - mobj->info->height - FLT2FIX(mobj->floorz);
        if(space > 48 * FRACUNIT)
        {
            space -= 40 * FRACUNIT;
            mobj->pos[VZ] =
                ((space * P_Random()) >> 8) + FLT2FIX(mobj->floorz) + 40 * FRACUNIT;
        }
        else
        {
            mobj->pos[VZ] = FLT2FIX(mobj->floorz);
        }
    }
    else if(mobj->flags2 & MF2_FLOATBOB)
    {
        mobj->pos[VZ] = FLT2FIX(mobj->floorz) + z; // artifact z passed in as height
    }
    else
    {
        mobj->pos[VZ] = z;
    }

    if(mobj->flags2 & MF2_FLOORCLIP &&
       P_GetThingFloorType(mobj) >= FLOOR_LIQUID &&
       mobj->pos[VZ] == P_GetFixedp(mobj->subsector, DMU_FLOOR_HEIGHT))
    {
        mobj->floorclip = 10;
    }
    else
    {
        mobj->floorclip = 0;
    }

    return mobj;
}

void P_RemoveMobj(mobj_t *mobj)
{
    // Remove from creature queue
    if(mobj->flags & MF_COUNTKILL && mobj->flags & MF_CORPSE)
    {
        A_DeQueueCorpse(mobj);
    }

    if(mobj->tid)
    {                           // Remove from TID list
        P_RemoveMobjFromTIDList(mobj);
    }

    // Unlink from sector and block lists
    P_UnsetThingPosition(mobj);

    // Stop any playing sound
    S_StopSound(0, mobj);

    // Free block
    P_RemoveThinker((thinker_t *) mobj);
}

/**
 * Called when a player is spawned on the level. Most of the player
 * structure stays unchanged between levels.
 */
void P_SpawnPlayer(thing_t *mthing, int playernum)
{
    player_t   *p;
    fixed_t     pos[3];
    mobj_t     *mobj = 0;

    if(!players[playernum].plr->ingame)
        return;

    p = &players[playernum];
    if(p->playerstate == PST_REBORN)
    {
        G_PlayerReborn(playernum);
    }

    pos[VX] = mthing->x << FRACBITS;
    pos[VY] = mthing->y << FRACBITS;
    pos[VZ] = ONFLOORZ;
    if(randomclass && deathmatch)
    {
        p->class = P_Random() % 3;
        if(p->class == cfg.playerClass[playernum])
        {
            p->class = (p->class + 1) % 3;
        }
        cfg.playerClass[playernum] = p->class;
        SB_SetClassData();
        NetSv_SendPlayerInfo(playernum, DDSP_ALL_PLAYERS);
    }
    else
    {
        p->class = cfg.playerClass[playernum];
    }

    mobj = P_SpawnMobj(pos[VX], pos[VY], pos[VZ], PCLASS_INFO(p->class)->mobjtype);

    // With clients all player mobjs are remote, even the consoleplayer.
    if(IS_CLIENT)
    {
        mobj->flags &= ~MF_SOLID;
        mobj->ddflags = DDMF_REMOTE | DDMF_DONTDRAW;
        // The real flags are received from the server later on.
    }

    // Set translation table data.
    if(p->class == PCLASS_FIGHTER && (p->colormap == 0 || p->colormap == 2))
    {
        // The first type should be blue, and the third should be the
        // Fighter's original gold color
        //if(mthing->type == 1)
        if(p->colormap == 0)
        {
            mobj->flags |= 2 << MF_TRANSSHIFT;
        }
    }
    else if(p->colormap > 0 && p->colormap < 8)
    {  // Set color translation bits for player sprites
        //mobj->flags |= (mthing->type-1)<<MF_TRANSSHIFT;
        mobj->flags |= p->colormap << MF_TRANSSHIFT;
    }

    mobj->angle = ANG45 * (mthing->angle / 45); /* $unifiedangles */
    p->plr->lookdir = 0;/* $unifiedangles */
    p->plr->flags |= DDPF_FIXANGLES | DDPF_FIXPOS | DDPF_FIXMOM;
    mobj->player = p;
    mobj->dplayer = p->plr;
    mobj->health = p->health;
    p->plr->mo = mobj;
    p->playerstate = PST_LIVE;
    p->refire = 0;
    p->damagecount = 0;
    p->bonuscount = 0;
    p->poisoncount = 0;
    p->morphTics = 0;
    p->plr->extralight = 0;
    p->plr->fixedcolormap = 0;

    if(p->plr->flags & DDPF_CAMERA)
    {
        p->plr->mo->pos[VZ] += cfg.plrViewHeight << FRACBITS;
        p->plr->viewheight = 0;
    }
    else
        p->plr->viewheight = (int) cfg.plrViewHeight;

    p->plr->lookdir = 0;
    P_SetupPsprites(p);
    if(deathmatch)
    {   // Give all keys in death match mode
        p->keys = 2047;
    }

    if(playernum == consoleplayer)
    {
        // wake up the status bar
        ST_Start();

        // wake up the heads up text
        HU_Start();
    }
}

/**
 * The fields of the mapthing should already be in host byte order.
 */
void P_SpawnMapThing(thing_t *mthing)
{
    int         i;
    unsigned int spawnMask;
    mobj_t     *mobj;
    fixed_t     pos[3];
    static unsigned int classFlags[] = {
        MTF_FIGHTER,
        MTF_CLERIC,
        MTF_MAGE
    };

    // Count deathmatch start positions.
    if(mthing->type == 11)
    {
        if(deathmatch_p < &deathmatchstarts[MAX_DM_STARTS])
        {
            memcpy(deathmatch_p, mthing, sizeof(*mthing));
            deathmatch_p++;
        }
        return;
    }

    if(mthing->type == PO_ANCHOR_TYPE)
    {   // Polyobj Anchor Pt.
        return;
    }
    else if(mthing->type == PO_SPAWN_TYPE ||
            mthing->type == PO_SPAWNCRUSH_TYPE)
    {   // Polyobj Anchor Pt.
        DD_SetInteger(DD_POLYOBJ_COUNT, DD_GetInteger(DD_POLYOBJ_COUNT) + 1);
        return;
    }

    // Check for player starts 1 to 4.
    if(mthing->type <= 4)
    {
        /*
           if(mthing->arg1 < 0 || mthing->arg1 > MAX_PLAYER_STARTS)
           {
           Con_Message("P_SpawnMapThing: mthing->arg1 is really weird!"
           " (%d)\n", mthing->arg1);
           }
         */
        P_RegisterPlayerStart(mthing);
        /*playerstarts[mthing->arg1][mthing->type-1] = *mthing;
           if(!deathmatch && !mthing->arg1)
           {
           P_SpawnPlayer(mthing, mthing->type-1);
           } */
        return;
    }

    // Check for player starts 5 to 8.
    if(mthing->type >= 9100 && mthing->type <= 9103)
    {
        mthing->type = 5 + mthing->type - 9100; // Translate to 5 - 8.
        P_RegisterPlayerStart(mthing);
        /*playerstarts[mthing->arg1][mthing->type-1] = *mthing;
           if(!deathmatch && !mthing->arg1)
           {
           P_SpawnPlayer(mthing, mthing->type-1);
           } */
        return;
    }

    if(mthing->type >= 1400 && mthing->type < 1410)
    {
        sector_t* sector = P_GetPtrp(
            R_PointInSubsector(mthing->x << FRACBITS, mthing->y << FRACBITS),
            DMU_SECTOR);
        P_XSector(sector)->seqType = mthing->type - 1400;
        return;
    }

    // Check current game type with spawn flags.
    if(IS_NETGAME == false)
    {
        spawnMask = MTF_GSINGLE;
    }
    else if(deathmatch)
    {
        spawnMask = MTF_GDEATHMATCH;
    }
    else
    {
        spawnMask = MTF_GCOOP;
    }

    if(!(mthing->options & spawnMask))
        return;

    // Check current skill with spawn flags.
    if(gameskill == SM_BABY || gameskill == SM_EASY)
    {
        spawnMask = MTF_EASY;
    }
    else if(gameskill == SM_HARD || gameskill == SM_NIGHTMARE)
    {
        spawnMask = MTF_HARD;
    }
    else
    {
        spawnMask = MTF_NORMAL;
    }

    if(!(mthing->options & spawnMask))
        return;

    // Check current character classes with spawn flags.
    if(IS_NETGAME == false)
    {   // Single player.
        if((mthing->options & classFlags[cfg.playerClass[0]]) == 0)
        {   // Not for current class.
            return;
        }
    }
    else if(deathmatch == false)
    {   // Cooperative.
        spawnMask = 0;
        for(i = 0; i < MAXPLAYERS; ++i)
        {
            if(players[i].plr->ingame)
            {
                spawnMask |= classFlags[cfg.playerClass[i]];
            }
        }

        // No players are in the game when a dedicated server is
        // started. In this case, we'll be generous and spawn stuff
        // for all the classes.
        if(!spawnMask)
        {
            spawnMask |= MTF_FIGHTER | MTF_CLERIC | MTF_MAGE;
        }

        if((mthing->options & spawnMask) == 0)
        {
            return;
        }
    }

    // Find which type to spawn.
    for(i = 0; i < Get(DD_NUMMOBJTYPES); ++i)
    {
        if(mthing->type == mobjinfo[i].doomednum)
        {
            break;
        }
    }

    if(i == Get(DD_NUMMOBJTYPES))
    {   // Can't find thing type.
        Con_Error("P_SpawnMapThing: Unknown type %i at (%i, %i)",
                  mthing->type, mthing->x, mthing->y);
    }

    // Clients only spawn local objects.
    if(IS_CLIENT)
    {
        if(!(mobjinfo[i].flags & MF_LOCAL))
            return;
    }

    // Don't spawn keys and players in deathmatch.
    if(deathmatch && mobjinfo[i].flags & MF_NOTDMATCH)
    {
        return;
    }

    // Don't spawn monsters if -nomonsters.
    if(nomonsters && (mobjinfo[i].flags & MF_COUNTKILL))
    {
        return;
    }

    pos[VX] = mthing->x << FRACBITS;
    pos[VY] = mthing->y << FRACBITS;
    if(mobjinfo[i].flags & MF_SPAWNCEILING)
    {
        pos[VZ] = ONCEILINGZ;
    }
    else if(mobjinfo[i].flags2 & MF2_SPAWNFLOAT)
    {
        pos[VZ] = FLOATRANDZ;
    }
    else if(mobjinfo[i].flags2 & MF2_FLOATBOB)
    {
        pos[VZ] = mthing->height << FRACBITS;
    }
    else
    {
        pos[VZ] = ONFLOORZ;
    }

    switch(i)
    {   // Special stuff.
    case MT_ZLYNCHED_NOHEART:
        P_SpawnMobj(pos[VX], pos[VY], ONFLOORZ, MT_BLOODPOOL);
        break;

    default:
        break;
    }

    mobj = P_SpawnMobj(pos[VX], pos[VY], pos[VZ], i);
    if(pos[VZ] == ONFLOORZ)
    {
        mobj->pos[VZ] += mthing->height << FRACBITS;
    }
    else if(pos[VZ] == ONCEILINGZ)
    {
        mobj->pos[VZ] -= mthing->height << FRACBITS;
    }

    mobj->tid = mthing->tid;
    mobj->special = mthing->special;
    mobj->args[0] = mthing->arg1;
    mobj->args[1] = mthing->arg2;
    mobj->args[2] = mthing->arg3;
    mobj->args[3] = mthing->arg4;
    mobj->args[4] = mthing->arg5;
    if(mobj->flags2 & MF2_FLOATBOB)
    {   // Seed random starting index for bobbing motion.
        mobj->health = P_Random();
        mobj->special1 = mthing->height << FRACBITS;
    }

    if(mobj->tics > 0)
    {
        mobj->tics = 1 + (P_Random() % mobj->tics);
    }

    if(mobj->flags & MF_COUNTKILL)
    {
        // Quantize angle to 45 degree increments.
        mobj->angle = ANG45 * (mthing->angle / 45);
    }
    else
    {
        // Scale angle correctly (source is 0..359).
        mobj->angle = ((mthing->angle << 8) / 360) << 24;
    }

    mobj->visangle = mobj->angle >> 16; // "angle-servo"; smooth actor turning
    if(mthing->options & MTF_AMBUSH)
    {
        mobj->flags |= MF_AMBUSH;
    }

    if(mthing->options & MTF_DORMANT)
    {
        mobj->flags2 |= MF2_DORMANT;
        if(mobj->type == MT_ICEGUY)
        {
            P_SetMobjState(mobj, S_ICEGUY_DORMANT);
        }
        mobj->tics = -1;
    }
}

void P_CreateTIDList(void)
{
    int         i;
    mobj_t     *mobj;
    thinker_t  *t;

    i = 0;
    for(t = thinkercap.next; t != &thinkercap && t; t = t->next)
    {
        if(t->function != P_MobjThinker)
            continue; 

        mobj = (mobj_t *) t;
        if(mobj->tid != 0)
        {   // Add to list
            if(i == MAX_TID_COUNT)
            {
                Con_Error("P_CreateTIDList: MAX_TID_COUNT (%d) exceeded.",
                          MAX_TID_COUNT);
            }
            TIDList[i] = mobj->tid;
            TIDMobj[i++] = mobj;
        }
    }

    // Add termination marker
    TIDList[i] = 0;
}

void P_InsertMobjIntoTIDList(mobj_t *mobj, int tid)
{
    int         i, index;

    index = -1;
    for(i = 0; TIDList[i] != 0; ++i)
    {
        if(TIDList[i] == -1)
        {   // Found empty slot
            index = i;
            break;
        }
    }

    if(index == -1)
    {   // Append required
        if(i == MAX_TID_COUNT)
        {
            Con_Error("P_InsertMobjIntoTIDList: MAX_TID_COUNT (%d)"
                      "exceeded.", MAX_TID_COUNT);
        }
        index = i;
        TIDList[index + 1] = 0;
    }

    mobj->tid = tid;
    TIDList[index] = tid;
    TIDMobj[index] = mobj;
}

void P_RemoveMobjFromTIDList(mobj_t *mobj)
{
    int         i;

    for(i = 0; TIDList[i] != 0; ++i)
    {
        if(TIDMobj[i] == mobj)
        {
            TIDList[i] = -1;
            TIDMobj[i] = NULL;
            mobj->tid = 0;
            return;
        }
    }

    mobj->tid = 0;
}

mobj_t *P_FindMobjFromTID(int tid, int *searchPosition)
{
    int         i;

    for(i = *searchPosition + 1; TIDList[i] != 0; ++i)
    {
        if(TIDList[i] == tid)
        {
            *searchPosition = i;
            return TIDMobj[i];
        }
    }

    *searchPosition = -1;
    return NULL;
}

void P_SpawnPuff(fixed_t x, fixed_t y, fixed_t z)
{
    mobj_t     *puff;

    z += ((P_Random() - P_Random()) << 10);
    puff = P_SpawnMobj(x, y, z, PuffType);
    if(linetarget && puff->info->seesound)
    {   // Hit thing sound
        S_StartSound(puff->info->seesound, puff);
    }
    else if(puff->info->attacksound)
    {
        S_StartSound(puff->info->attacksound, puff);
    }

    switch(PuffType)
    {
    case MT_PUNCHPUFF:
        puff->mom[MZ] = FRACUNIT;
        break;

    case MT_HAMMERPUFF:
        puff->mom[MZ] = .8 * FRACUNIT;
        break;

    default:
        break;
    }

    PuffSpawned = puff;
}

void P_BloodSplatter(fixed_t x, fixed_t y, fixed_t z, mobj_t *originator)
{
    mobj_t *mo;

    mo = P_SpawnMobj(x, y, z, MT_BLOODSPLATTER);
    mo->target = originator;
    mo->mom[MX] = (P_Random() - P_Random()) << 10;
    mo->mom[MY] = (P_Random() - P_Random()) << 10;
    mo->mom[MZ] = 3 * FRACUNIT;
}

void P_BloodSplatter2(fixed_t x, fixed_t y, fixed_t z, mobj_t *originator)
{
    mobj_t *mo;

    mo = P_SpawnMobj(x + ((P_Random() - 128) << 11),
                     y + ((P_Random() - 128) << 11), z, MT_AXEBLOOD);
    mo->target = originator;
}

void P_RipperBlood(mobj_t *mo)
{
    mobj_t *th;
    fixed_t pos[3];

    memcpy(pos, mo->pos, sizeof(pos));
    pos[VX] += ((P_Random() - P_Random()) << 12);
    pos[VY] += ((P_Random() - P_Random()) << 12);
    pos[VZ] += ((P_Random() - P_Random()) << 12);
    th = P_SpawnMobj(pos[VX], pos[VY], pos[VZ], MT_BLOOD);
    //  th->flags |= MF_NOGRAVITY;
    th->mom[MX] = mo->mom[MX] >> 1;
    th->mom[MY] = mo->mom[MY] >> 1;
    th->tics += P_Random() & 3;
}

int P_GetThingFloorType(mobj_t *thing)
{
    if(thing->floorpic && !IS_CLIENT)
    {
        return P_FlatToTerrainType(thing->floorpic);
    }
    else
    {
        return P_GetTerrainType(P_GetPtrp(thing->subsector, DMU_SECTOR), PLN_FLOOR);
    }
}

int P_HitFloor(mobj_t *thing)
{
    mobj_t     *mo;
    int         smallsplash = false;

    if(thing->floorz != P_GetFloatp(thing->subsector, DMU_FLOOR_HEIGHT))
    {   // Don't splash if landing on the edge above water/lava/etc....
        return (FLOOR_SOLID);
    }

    // Things that don't splash go here
    switch(thing->type)
    {
    case MT_LEAF1:
    case MT_LEAF2:
        //      case MT_BLOOD:          // I set these to low mass -- pm
        //      case MT_BLOODSPLATTER:
    case MT_SPLASH:
    case MT_SLUDGECHUNK:
        return (FLOOR_SOLID);

    default:
        break;
    }

    // Small splash for small masses
    if(thing->info->mass < 10)
        smallsplash = true;

    switch(P_GetThingFloorType(thing))
    {
    case FLOOR_WATER:
        if(smallsplash)
        {
            mo = P_SpawnMobj(thing->pos[VX], thing->pos[VY], ONFLOORZ, MT_SPLASHBASE);
            if(mo)
                mo->floorclip += SMALLSPLASHCLIP;
            S_StartSound(SFX_AMBIENT10, mo);    // small drip
        }
        else
        {
            mo = P_SpawnMobj(thing->pos[VX], thing->pos[VY], ONFLOORZ, MT_SPLASH);
            mo->target = thing;
            mo->mom[MX] = (P_Random() - P_Random()) << 8;
            mo->mom[MY] = (P_Random() - P_Random()) << 8;
            mo->mom[MZ] = 2 * FRACUNIT + (P_Random() << 8);
            mo = P_SpawnMobj(thing->pos[VX], thing->pos[VY], ONFLOORZ, MT_SPLASHBASE);
            if(thing->player)
                P_NoiseAlert(thing, thing);
            S_StartSound(SFX_WATER_SPLASH, mo);
        }
        return (FLOOR_WATER);

    case FLOOR_LAVA:
        if(smallsplash)
        {
            mo = P_SpawnMobj(thing->pos[VX], thing->pos[VY], ONFLOORZ, MT_LAVASPLASH);
            if(mo)
                mo->floorclip += SMALLSPLASHCLIP;
        }
        else
        {
            mo = P_SpawnMobj(thing->pos[VX], thing->pos[VY], ONFLOORZ, MT_LAVASMOKE);
            mo->mom[MZ] = FRACUNIT + (P_Random() << 7);
            mo = P_SpawnMobj(thing->pos[VX], thing->pos[VY], ONFLOORZ, MT_LAVASPLASH);
            if(thing->player)
                P_NoiseAlert(thing, thing);
        }

        S_StartSound(SFX_LAVA_SIZZLE, mo);
        if(thing->player && leveltime & 31)
        {
            P_DamageMobj(thing, &LavaInflictor, NULL, 5);
        }
        return (FLOOR_LAVA);

    case FLOOR_SLUDGE:
        if(smallsplash)
        {
            mo = P_SpawnMobj(thing->pos[VX], thing->pos[VY], ONFLOORZ, MT_SLUDGESPLASH);
            if(mo)
                mo->floorclip += SMALLSPLASHCLIP;
        }
        else
        {
            mo = P_SpawnMobj(thing->pos[VX], thing->pos[VY], ONFLOORZ, MT_SLUDGECHUNK);
            mo->target = thing;
            mo->mom[MX] = (P_Random() - P_Random()) << 8;
            mo->mom[MY] = (P_Random() - P_Random()) << 8;
            mo->mom[MZ] = FRACUNIT + (P_Random() << 8);
            mo = P_SpawnMobj(thing->pos[VX], thing->pos[VY], ONFLOORZ, MT_SLUDGESPLASH);
            if(thing->player)
                P_NoiseAlert(thing, thing);
        }

        S_StartSound(SFX_SLUDGE_GLOOP, mo);
        return (FLOOR_SLUDGE);
    }

    return (FLOOR_SOLID);
}

void ResetBlasted(mobj_t *mo)
{
    mo->flags2 &= ~MF2_BLASTED;
    if(!(mo->flags & MF_ICECORPSE))
    {
        mo->flags2 &= ~MF2_SLIDE;
    }
}

void P_BlastMobj(mobj_t *source, mobj_t *victim, fixed_t strength)
{
    angle_t angle, ang;
    mobj_t *mo;
    fixed_t pos[3];

    angle = R_PointToAngle2(source->pos[VX], source->pos[VY],
                            victim->pos[VX], victim->pos[VY]);
    angle >>= ANGLETOFINESHIFT;
    if(strength < BLAST_FULLSTRENGTH)
    {
        victim->mom[MX] = FixedMul(strength, finecosine[angle]);
        victim->mom[MY] = FixedMul(strength, finesine[angle]);
        if(victim->player)
        {
            // Players handled automatically
        }
        else
        {
            victim->flags2 |= MF2_SLIDE;
            victim->flags2 |= MF2_BLASTED;
        }
    }
    else // full strength blast from artifact
    {
        if(victim->flags & MF_MISSILE)
        {
            switch(victim->type)
            {
            case MT_SORCBALL1: // don't blast sorcerer balls
            case MT_SORCBALL2:
            case MT_SORCBALL3:
                return;
                break;

            case MT_MSTAFF_FX2: // Reflect to originator
                victim->tracer = victim->target;
                victim->target = source;
                break;

            default:
                break;
            }
        }

        if(victim->type == MT_HOLY_FX)
        {
            if(victim->tracer == source)
            {
                victim->tracer = victim->target;
                victim->target = source;
            }
        }
        victim->mom[MX] = FixedMul(BLAST_SPEED, finecosine[angle]);
        victim->mom[MY] = FixedMul(BLAST_SPEED, finesine[angle]);

        // Spawn blast puff
        ang = R_PointToAngle2(victim->pos[VX], victim->pos[VY],
                              source->pos[VX], source->pos[VY]);
        ang >>= ANGLETOFINESHIFT;

        memcpy(pos, victim->pos, sizeof(pos));
        pos[VX] += FixedMul(victim->radius + FRACUNIT, finecosine[ang]);
        pos[VY] += FixedMul(victim->radius + FRACUNIT, finesine[ang]);
        pos[VZ] -= FLT2FIX(victim->floorclip + victim->height /2);

        mo = P_SpawnMobj(pos[VX], pos[VY], pos[VZ], MT_BLASTEFFECT);
        if(mo)
        {
            mo->mom[MX] = victim->mom[MX];
            mo->mom[MY] = victim->mom[MY];
        }

        if(victim->flags & MF_MISSILE)
        {
            victim->mom[MZ] = 8 * FRACUNIT;
            mo->mom[MZ] = victim->mom[MZ];
        }
        else
        {
            victim->mom[MZ] = (1000 / victim->info->mass) << FRACBITS;
        }

        if(victim->player)
        {
            // Players handled automatically
        }
        else
        {
            victim->flags2 |= MF2_SLIDE;
            victim->flags2 |= MF2_BLASTED;
        }
    }
}

/**
 * Blast all mobj things away
 */
void P_BlastRadius(player_t *player)
{
    mobj_t     *mo;
    mobj_t     *pmo = player->plr->mo;
    thinker_t  *think;
    fixed_t     dist;

    S_StartSound(SFX_ARTIFACT_BLAST, pmo);
    P_NoiseAlert(player->plr->mo, player->plr->mo);

    for(think = thinkercap.next; think != &thinkercap && think;
        think = think->next)
    {
        if(think->function != P_MobjThinker)
            continue; // Not a mobj thinker

        mo = (mobj_t *) think;
        if((mo == pmo) || (mo->flags2 & MF2_BOSS))
            continue; // Not a valid monster

        if((mo->type == MT_POISONCLOUD) ||  // poison cloud
           (mo->type == MT_HOLY_FX) ||  // holy fx
           (mo->flags & MF_ICECORPSE))  // frozen corpse
        {
            // Let these special cases go
        }
        else if((mo->flags & MF_COUNTKILL) && (mo->health <= 0))
        {
            continue;
        }
        else if(!(mo->flags & MF_COUNTKILL) && !(mo->player) &&
                !(mo->flags & MF_MISSILE))
        {   // Must be monster, player, or missile
            continue;
        }

        if(mo->flags2 & MF2_DORMANT)
            continue;  // no dormant creatures

        if((mo->type == MT_WRAITHB) && (mo->flags2 & MF2_DONTDRAW))
            continue;  // no underground wraiths

        if((mo->type == MT_SPLASHBASE) || (mo->type == MT_SPLASH))
            continue;

        if(mo->type == MT_SERPENT || mo->type == MT_SERPENTLEADER)
            continue;

        dist = P_ApproxDistance(pmo->pos[VX] - mo->pos[VX],
                                pmo->pos[VY] - mo->pos[VY]);
        if(dist > BLAST_RADIUS_DIST)
            continue; // Out of range

        P_BlastMobj(pmo, mo, BLAST_FULLSTRENGTH);
    }
}

/**
 * Do class specific effect for everyone in radius
 */
boolean P_HealRadius(player_t *player)
{
    mobj_t     *mo;
    mobj_t     *pmo = player->plr->mo;
    thinker_t  *think;
    fixed_t     dist;
    int         effective = false;
    int         amount;

    for(think = thinkercap.next; think != &thinkercap && think;
        think = think->next)
    {
        if(think->function != P_MobjThinker)
            continue; // Not a mobj thinker

        mo = (mobj_t *) think;
        if(!mo->player || mo->health <= 0)
            continue;

        dist = P_ApproxDistance(pmo->pos[VX] - mo->pos[VX],
                                pmo->pos[VY] - mo->pos[VY]);
        if(dist > HEAL_RADIUS_DIST)
            continue; // Out of range

        switch(player->class)
        {
        case PCLASS_FIGHTER: // Radius armor boost
            if((P_GiveArmor(mo->player, ARMOR_ARMOR, 1)) ||
               (P_GiveArmor(mo->player, ARMOR_SHIELD, 1)) ||
               (P_GiveArmor(mo->player, ARMOR_HELMET, 1)) ||
               (P_GiveArmor(mo->player, ARMOR_AMULET, 1)))
            {
                effective = true;
                S_StartSound(SFX_MYSTICINCANT, mo);
            }
            break;

        case PCLASS_CLERIC: // Radius heal
            amount = 50 + (P_Random() % 50);
            if(P_GiveBody(mo->player, amount))
            {
                effective = true;
                S_StartSound(SFX_MYSTICINCANT, mo);
            }
            break;

        case PCLASS_MAGE: // Radius mana boost
            amount = 50 + (P_Random() % 50);
            if((P_GiveMana(mo->player, AT_BLUEMANA, amount)) ||
               (P_GiveMana(mo->player, AT_GREENMANA, amount)))
            {
                effective = true;
                S_StartSound(SFX_MYSTICINCANT, mo);
            }
            break;

        case PCLASS_PIG:
        default:
            break;
        }
    }

    return effective;
}

/**
 * @return              <code>true</code> if the missile is at a valid
 *                      spawn point, otherwise explodes it and return
 *                      <code>false</code>.
 */
boolean P_CheckMissileSpawn(mobj_t *missile)
{
    //missile->tics -= P_Random()&3;

    // move a little forward so an angle can be computed if it
    // immediately explodes
    missile->pos[VX] += (missile->mom[MX] >> 1);
    missile->pos[VY] += (missile->mom[MY] >> 1);
    missile->pos[VZ] += (missile->mom[MZ] >> 1);
    if(!P_TryMove(missile, missile->pos[VX], missile->pos[VY]))
    {
        P_ExplodeMissile(missile);
        return (false);
    }
    return (true);
}

/**
 * @return             <code>NULL</code> if the missile exploded
 *                     immediately, otherwise returns a mobj_t pointer to
 *                     the spawned missile.
 */
mobj_t *P_SpawnMissile(mobj_t *source, mobj_t *dest, mobjtype_t type)
{
    fixed_t     z;
    mobj_t     *th;
    angle_t     an;
    int         dist, origdist;
    float       aim;

    switch(type)
    {
    case MT_MNTRFX1: // Minotaur swing attack missile
        z = source->pos[VZ] + 40 * FRACUNIT;
        break;

    case MT_MNTRFX2: // Minotaur floor fire missile
        z = ONFLOORZ + FLT2FIX(source->floorclip);
        break;

    case MT_CENTAUR_FX:
        z = source->pos[VZ] + 45 * FRACUNIT;
        break;

    case MT_ICEGUY_FX:
        z = source->pos[VZ] + 40 * FRACUNIT;
        break;

    case MT_HOLY_MISSILE:
        z = source->pos[VZ] + 40 * FRACUNIT;
        break;

    default:
        z = source->pos[VZ] + 32 * FRACUNIT;
        break;
    }
    z -= FLT2FIX(source->floorclip);
    th = P_SpawnMobj(source->pos[VX], source->pos[VY], z, type);
    if(th->info->seesound)
        S_StartSound(th->info->seesound, th);

    th->target = source; // Originator
    an = R_PointToAngle2(source->pos[VX], source->pos[VY],
                         dest->pos[VX], dest->pos[VY]);
    if(dest->flags & MF_SHADOW)
    {   // Invisible target
        an += (P_Random() - P_Random()) << 21;
    }

    th->angle = an;
    an >>= ANGLETOFINESHIFT;
    th->mom[MX] = FixedMul(th->info->speed, finecosine[an]);
    th->mom[MY] = FixedMul(th->info->speed, finesine[an]);
    origdist = P_ApproxDistance(dest->pos[VX] - source->pos[VX],
                                dest->pos[VY] - source->pos[VY]);
    dist = origdist / th->info->speed;
    if(dist < 1)
    {
        dist = 1;
    }
    th->mom[MZ] = (dest->pos[VZ] - source->pos[VZ]) / dist;

    // Use a more three-dimensional method.
    aim =
        BANG2RAD(bamsAtan2
                 ((dest->pos[VZ] - source->pos[VZ]) >> FRACBITS, origdist >> FRACBITS));
    th->mom[MX] *= cos(aim);
    th->mom[MY] *= cos(aim);
    th->mom[MZ] = sin(aim) * th->info->speed;

    return (P_CheckMissileSpawn(th) ? th : NULL);
}

/**
 * @return              <code>NULL</code> if the missile exploded
 *                      immediately, otherwise returns a mobj_t pointer
 *                      to the spawned missile.
 */
mobj_t *P_SpawnMissileXYZ(fixed_t x, fixed_t y, fixed_t z, mobj_t *source,
                          mobj_t *dest, mobjtype_t type)
{
    mobj_t     *th;
    angle_t     an;
    int         dist;

    z -= FLT2FIX(source->floorclip);
    th = P_SpawnMobj(x, y, z, type);

    if(th->info->seesound)
        S_StartSound(th->info->seesound, th);

    th->target = source; // Originator
    an = R_PointToAngle2(source->pos[VX], source->pos[VY],
                         dest->pos[VX], dest->pos[VY]);
    if(dest->flags & MF_SHADOW)
    {   // Invisible target
        an += (P_Random() - P_Random()) << 21;
    }

    th->angle = an;
    an >>= ANGLETOFINESHIFT;
    th->mom[MX] = FixedMul(th->info->speed, finecosine[an]);
    th->mom[MY] = FixedMul(th->info->speed, finesine[an]);
    dist = P_ApproxDistance(dest->pos[VX] - source->pos[VX],
                            dest->pos[VY] - source->pos[VY]);
    dist = dist / th->info->speed;
    if(dist < 1)
    {
        dist = 1;
    }
    th->mom[MZ] = (dest->pos[VZ] - source->pos[VZ]) / dist;
    return (P_CheckMissileSpawn(th) ? th : NULL);
}

/**
 * @return              <code>NULL</code> if the missile exploded
 *                      immediately, otherwise returns a mobj_t pointer
 *                      to the spawned missile.
 */
mobj_t *P_SpawnMissileAngle(mobj_t *source, mobjtype_t type, angle_t angle,
                            fixed_t momz)
{
    fixed_t     z;
    mobj_t     *mo;

    switch(type)
    {
    case MT_MNTRFX1: // Minotaur swing attack missile
        z = source->pos[VZ] + 40 * FRACUNIT;
        break;

    case MT_MNTRFX2: // Minotaur floor fire missile
        z = ONFLOORZ + FLT2FIX(source->floorclip);
        break;

    case MT_ICEGUY_FX2: // Secondary Projectiles of the Ice Guy
        z = source->pos[VZ] + 3 * FRACUNIT;
        break;

    case MT_MSTAFF_FX2:
        z = source->pos[VZ] + 40 * FRACUNIT;
        break;

    default:
        z = source->pos[VZ] + 32 * FRACUNIT;
        break;
    }

    z -= FLT2FIX(source->floorclip);
    mo = P_SpawnMobj(source->pos[VX], source->pos[VY], z, type);

    if(mo->info->seesound)
        S_StartSound(mo->info->seesound, mo);

    mo->target = source; // Originator
    mo->angle = angle;
    angle >>= ANGLETOFINESHIFT;
    mo->mom[MX] = FixedMul(mo->info->speed, finecosine[angle]);
    mo->mom[MY] = FixedMul(mo->info->speed, finesine[angle]);
    mo->mom[MZ] = momz;
    return (P_CheckMissileSpawn(mo) ? mo : NULL);
}

/**
 * @return              <code>NULL</code> if the missile exploded
 *                      immediately, otherwise returns a mobj_t pointer
 *                      to the spawned missile.
 */
mobj_t *P_SpawnMissileAngleSpeed(mobj_t *source, mobjtype_t type,
                                 angle_t angle, fixed_t momz, fixed_t speed)
{
    fixed_t     z;
    mobj_t     *mo;

    z = source->pos[VZ];
    z -= FLT2FIX(source->floorclip);
    mo = P_SpawnMobj(source->pos[VX], source->pos[VY], z, type);

    mo->target = source; // Originator
    mo->angle = angle;
    angle >>= ANGLETOFINESHIFT;
    mo->mom[MX] = FixedMul(speed, finecosine[angle]);
    mo->mom[MY] = FixedMul(speed, finesine[angle]);
    mo->mom[MZ] = momz;
    return (P_CheckMissileSpawn(mo) ? mo : NULL);
}

/**
 * Tries to aim at a nearby monster.
 */
mobj_t *P_SpawnPlayerMissile(mobj_t *source, mobjtype_t type)
{
    angle_t     an;
    fixed_t     pos[3], slope;

    //float     fangle = source->player->plr->lookdir * 85.0/110.0 /180*PI;
    float       fangle = LOOKDIR2RAD(source->player->plr->lookdir);
    float       movfac = 1;
    boolean     dontAim = cfg.noAutoAim;

    // Try to find a target
    an = source->angle;
    slope = P_AimLineAttack(source, an, 16 * 64 * FRACUNIT);
    if(!linetarget || dontAim)
    {
        an += 1 << 26;
        slope = P_AimLineAttack(source, an, 16 * 64 * FRACUNIT);
        if(!linetarget)
        {
            an -= 2 << 26;
            slope = P_AimLineAttack(source, an, 16 * 64 * FRACUNIT);
        }
        if(!linetarget || dontAim)
        {
            an = source->angle;

            slope = FRACUNIT * sin(fangle) / 1.2;
            movfac = cos(fangle);
        }
    }

    memcpy(pos, source->pos, sizeof(pos));

    if(type == MT_LIGHTNING_FLOOR)
    {
        pos[VZ] = ONFLOORZ;
        slope = 0;
    }
    else if(type == MT_LIGHTNING_CEILING)
    {
        pos[VZ] = ONCEILINGZ;
        slope = 0;
    }
    else
    {
        pos[VZ] += (cfg.plrViewHeight - 9) * FRACUNIT +
                   (((int) source->player->plr->lookdir) << FRACBITS) / 173;
        pos[VZ] -= FLT2FIX(source->floorclip);
    }

#ifdef TIC_DEBUG
    FUNTAG("P_SpawnPlayerMissile");
    if(rndDebugfile)
        fprintf(rndDebugfile,
                " -- srcZ:%f, plr:%d, lookdir:%f, z:%f, slope:%f\n",
                FIX2FLT(source->pos[VZ]), source->player - players,
                source->player->plr->lookdir, FIX2FLT(pos[VZ]), FIX2FLT(slope));
#endif

    MissileMobj = P_SpawnMobj(pos[VX], pos[VY], pos[VZ], type);

    MissileMobj->target = source;
    MissileMobj->angle = an;
    MissileMobj->mom[MX] =
        movfac * FixedMul(MissileMobj->info->speed,
                          finecosine[an >> ANGLETOFINESHIFT]);
    MissileMobj->mom[MY] =
        movfac * FixedMul(MissileMobj->info->speed,
                          finesine[an >> ANGLETOFINESHIFT]);
    MissileMobj->mom[MZ] = FixedMul(MissileMobj->info->speed, slope);
    if(MissileMobj->type == MT_MWAND_MISSILE ||
       MissileMobj->type == MT_CFLAME_MISSILE)
    {   // Ultra-fast ripper spawning missile
        MissileMobj->pos[VX] += (MissileMobj->mom[MX] >> 3);
        MissileMobj->pos[VY] += (MissileMobj->mom[MY] >> 3);
        MissileMobj->pos[VZ] += (MissileMobj->mom[MZ] >> 3);
    }
    else
    {   // Normal missile
        MissileMobj->pos[VX] += (MissileMobj->mom[MX] >> 1);
        MissileMobj->pos[VY] += (MissileMobj->mom[MY] >> 1);
        MissileMobj->pos[VZ] += (MissileMobj->mom[MZ] >> 1);
    }

    if(!P_TryMove(MissileMobj, MissileMobj->pos[VX], MissileMobj->pos[VY]))
    {   // Exploded immediately
        P_ExplodeMissile(MissileMobj);
        return (NULL);
    }
    return (MissileMobj);
}

mobj_t *P_SPMAngle(mobj_t *source, mobjtype_t type, angle_t angle)
{
    mobj_t     *th;
    angle_t     an;
    fixed_t     pos[3], slope;
    float       fangle = LOOKDIR2RAD(source->player->plr->lookdir);
    float       movfac = 1;
    boolean     dontAim = cfg.noAutoAim;

    // See which target is to be aimed at.
    an = angle;
    slope = P_AimLineAttack(source, an, 16 * 64 * FRACUNIT);
    if(!linetarget || dontAim)
    {
        an += 1 << 26;
        slope = P_AimLineAttack(source, an, 16 * 64 * FRACUNIT);
        if(!linetarget)
        {
            an -= 2 << 26;
            slope = P_AimLineAttack(source, an, 16 * 64 * FRACUNIT);
        }
        if(!linetarget || dontAim)
        {
            an = angle;

            slope = FRACUNIT * sin(fangle) / 1.2;
            movfac = cos(fangle);
        }
    }

    memcpy(pos, source->pos, sizeof(pos));
    pos[VZ] += (cfg.plrViewHeight - 9) * FRACUNIT +
               (((int) source->player->plr->lookdir) << FRACBITS) / 173;
    pos[VZ] -= FLT2FIX(source->floorclip);

    th = P_SpawnMobj(pos[VX], pos[VY], pos[VZ], type);
    th->target = source;
    th->angle = an;
    th->mom[MX] =
        movfac * FixedMul(th->info->speed, finecosine[an >> ANGLETOFINESHIFT]);
    th->mom[MY] =
        movfac * FixedMul(th->info->speed, finesine[an >> ANGLETOFINESHIFT]);
    th->mom[MZ] = FixedMul(th->info->speed, slope);

    return (P_CheckMissileSpawn(th) ? th : NULL);
}

mobj_t *P_SPMAngleXYZ(mobj_t *source, fixed_t x, fixed_t y, fixed_t z,
                      mobjtype_t type, angle_t angle)
{
    mobj_t     *th;
    angle_t     an;
    fixed_t     slope;
    float       fangle = LOOKDIR2RAD(source->player->plr->lookdir);
    float       movfac = 1;
    boolean dontAim = cfg.noAutoAim;

    // See which target is to be aimed at.
    an = angle;
    slope = P_AimLineAttack(source, an, 16 * 64 * FRACUNIT);
    if(!linetarget || dontAim)
    {
        an += 1 << 26;
        slope = P_AimLineAttack(source, an, 16 * 64 * FRACUNIT);
        if(!linetarget)
        {
            an -= 2 << 26;
            slope = P_AimLineAttack(source, an, 16 * 64 * FRACUNIT);
        }

        if(!linetarget || dontAim)
        {
            an = angle;
            slope = FRACUNIT * sin(fangle) / 1.2;
            movfac = cos(fangle);

        }
    }

    z += 4 * 8 * FRACUNIT +
        (((int) source->player->plr->lookdir) << FRACBITS) / 173;
    z -= FLT2FIX(source->floorclip);
    th = P_SpawnMobj(x, y, z, type);

    th->target = source;
    th->angle = an;
    th->mom[MX] =
        movfac * FixedMul(th->info->speed, finecosine[an >> ANGLETOFINESHIFT]);
    th->mom[MY] =
        movfac * FixedMul(th->info->speed, finesine[an >> ANGLETOFINESHIFT]);
    th->mom[MZ] = FixedMul(th->info->speed, slope);

    return (P_CheckMissileSpawn(th) ? th : NULL);
}

mobj_t *P_SpawnKoraxMissile(fixed_t x, fixed_t y, fixed_t z,
                            mobj_t *source, mobj_t *dest, mobjtype_t type)
{
    mobj_t     *th;
    angle_t     an;
    int         dist;

    z -= FLT2FIX(source->floorclip);
    th = P_SpawnMobj(x, y, z, type);
    if(th->info->seesound)
    {
        S_StartSound(th->info->seesound, th);
    }

    th->target = source; // Originator
    an = R_PointToAngle2(x, y, dest->pos[VX], dest->pos[VY]);
    if(dest->flags & MF_SHADOW)
    {   // Invisible target
        an += (P_Random() - P_Random()) << 21;
    }

    th->angle = an;
    an >>= ANGLETOFINESHIFT;
    th->mom[MX] = FixedMul(th->info->speed, finecosine[an]);
    th->mom[MY] = FixedMul(th->info->speed, finesine[an]);

    dist = P_ApproxDistance(dest->pos[VX] - x, dest->pos[VY] - y);
    dist = dist / th->info->speed;
    if(dist < 1)
        dist = 1;

    th->mom[MZ] = (dest->pos[VZ] - z + (30 * FRACUNIT)) / dist;
    return (P_CheckMissileSpawn(th) ? th : NULL);
}
