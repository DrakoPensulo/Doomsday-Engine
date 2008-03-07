/**\file
 *\section License
 * License: GPL
 * Online License Link: http://www.gnu.org/licenses/gpl.html
 *
 *\author Copyright © 2003-2007 Jaakko Keränen <jaakko.keranen@iki.fi>
 *\author Copyright © 2005-2008 Daniel Swanson <danij@dengine.net>
 *\author Copyright © 2003-2005 Samuel Villarreal <svkaiser@gmail.com>
 *\author Copyright © 1993-1996 by id Software, Inc.
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
 * d_englsh.h: Printed strings for translation (native is English).
 */

#ifndef __D_ENGLSH_H__
#define __D_ENGLSH_H__

#ifndef __DOOM64TC__
#  error "Using Doom64TC headers without __DOOM64TC__"
#endif

#define GET_TXT(x)          ((*gi.text)[x].text)

#define D_DEVSTR            GET_TXT(TXT_D_DEVSTR)
#define D_CDROM             GET_TXT(TXT_D_CDROM)
#define PRESSKEY            GET_TXT(TXT_PRESSKEY)
#define PRESSYN             GET_TXT(TXT_PRESSYN)
#define QUITMSG             GET_TXT(TXT_QUITMSG)
#define LOADNET             GET_TXT(TXT_LOADNET)
#define QLOADNET            GET_TXT(TXT_QLOADNET)
#define QSAVESPOT           GET_TXT(TXT_QSAVESPOT)
#define SAVEDEAD            GET_TXT(TXT_SAVEDEAD)
#define QSPROMPT            GET_TXT(TXT_QSPROMPT)
#define QLPROMPT            GET_TXT(TXT_QLPROMPT)

#define NEWGAME             GET_TXT(TXT_NEWGAME)
#define NIGHTMARE           GET_TXT(TXT_NIGHTMARE)
#define SWSTRING            GET_TXT(TXT_SWSTRING)
#define MSGOFF              GET_TXT(TXT_MSGOFF)
#define MSGON               GET_TXT(TXT_MSGON)
#define NETEND              GET_TXT(TXT_NETEND)
#define ENDGAME             GET_TXT(TXT_ENDGAME)

#define DOSY                GET_TXT(TXT_DOSY)

#define DETAILHI            GET_TXT(TXT_DETAILHI)
#define DETAILLO            GET_TXT(TXT_DETAILLO)
#define GAMMALVL0           GET_TXT(TXT_GAMMALVL0)
#define GAMMALVL1           GET_TXT(TXT_GAMMALVL1)
#define GAMMALVL2           GET_TXT(TXT_GAMMALVL2)
#define GAMMALVL3           GET_TXT(TXT_GAMMALVL3)
#define GAMMALVL4           GET_TXT(TXT_GAMMALVL4)
#define EMPTYSTRING         GET_TXT(TXT_EMPTYSTRING)

#define GOTARMOR            GET_TXT(TXT_GOTARMOR)
#define GOTMEGA             GET_TXT(TXT_GOTMEGA)
#define GOTHTHBONUS         GET_TXT(TXT_GOTHTHBONUS)
#define GOTARMBONUS         GET_TXT(TXT_GOTARMBONUS)
#define GOTHELLBONUS        GET_TXT(TXT_GOTHELLBONUS) // d64tc
#define GOTSTIM             GET_TXT(TXT_GOTSTIM)
#define GOTMEDINEED         GET_TXT(TXT_GOTMEDINEED)
#define GOTMEDIKIT          GET_TXT(TXT_GOTMEDIKIT)
#define GOTSUPER            GET_TXT(TXT_GOTSUPER)
#define GOTBLUECARD         GET_TXT(TXT_GOTBLUECARD)
#define GOTYELWCARD         GET_TXT(TXT_GOTYELWCARD)
#define GOTREDCARD          GET_TXT(TXT_GOTREDCARD)
#define GOTBLUESKUL         GET_TXT(TXT_GOTBLUESKUL)
#define GOTYELWSKUL         GET_TXT(TXT_GOTYELWSKUL)
#define GOTREDSKULL         GET_TXT(TXT_GOTREDSKULL)
#define GOTINVUL            GET_TXT(TXT_GOTINVUL)
#define GOTBERSERK          GET_TXT(TXT_GOTBERSERK)
#define GOTINVIS            GET_TXT(TXT_GOTINVIS)
#define GOTSUIT             GET_TXT(TXT_GOTSUIT)
#define GOTMAP              GET_TXT(TXT_GOTMAP)
#define GOTVISOR            GET_TXT(TXT_GOTVISOR)
#define GOTMSPHERE          GET_TXT(TXT_GOTMSPHERE)
#define GOTCLIP             GET_TXT(TXT_GOTCLIP)
#define GOTCLIPBOX          GET_TXT(TXT_GOTCLIPBOX)
#define GOTROCKET           GET_TXT(TXT_GOTROCKET)
#define GOTROCKBOX          GET_TXT(TXT_GOTROCKBOX)
#define GOTCELL             GET_TXT(TXT_GOTCELL)
#define GOTCELLBOX          GET_TXT(TXT_GOTCELLBOX)
#define GOTSHELLS           GET_TXT(TXT_GOTSHELLS)
#define GOTSHELLBOX         GET_TXT(TXT_GOTSHELLBOX)
#define GOTBACKPACK         GET_TXT(TXT_GOTBACKPACK)
#define GOTBFG9000          GET_TXT(TXT_GOTBFG9000)
#define GOTCHAINGUN         GET_TXT(TXT_GOTCHAINGUN)
#define GOTCHAINSAW         GET_TXT(TXT_GOTCHAINSAW)
#define GOTLAUNCHER         GET_TXT(TXT_GOTLAUNCHER)
#define GOTPLASMA           GET_TXT(TXT_GOTPLASMA)
#define GOTSHOTGUN          GET_TXT(TXT_GOTSHOTGUN)
#define GOTSHOTGUN2         GET_TXT(TXT_GOTSHOTGUN2)
#define GOTUNMAKER          GET_TXT(TXT_GOTUNMAKER) // d64tc
#define NGOTUNMAKER         GET_TXT(TXT_NGOTUNMAKER) // d64tc
#define UNMAKERCHARGE       GET_TXT(TXT_UNMAKERCHARGE) // d64tc
#define GOTPOWERUP1         GET_TXT(TXT_GOTPOWERUP1) // d64TC
#define NGOTPOWERUP1        GET_TXT(TXT_NGOTPOWERUP1) // d64TC
#define GOTPOWERUP2         GET_TXT(TXT_GOTPOWERUP2) // d64TC
#define NGOTPOWERUP2        GET_TXT(TXT_NGOTPOWERUP2) // d64TC
#define GOTPOWERUP3         GET_TXT(TXT_GOTPOWERUP3) // d64TC
#define NGOTPOWERUP3        GET_TXT(TXT_NGOTPOWERUP3) // d64TC
#define GOTFLOATER          GET_TXT(TXT_GOTFLOATER) // d64tc
#define GOTHELLTIME         GET_TXT(TXT_GOTHELLTIME) // d64tc
#define HELLTIMEON          GET_TXT(TXT_HELLTIMEON) // d64tc
#define HELLTIMEWEAK        GET_TXT(TXT_HELLTIMEWEAK) // d64tc

#define PD_OPNPOWERUP       GET_TXT(TXT_PD_OPNPOWERUP) // d64TC
#define PD_BLUEO            GET_TXT(TXT_PD_BLUEO)
#define PD_REDO             GET_TXT(TXT_PD_REDO)
#define PD_YELLOWO          GET_TXT(TXT_PD_YELLOWO)
#define PD_BLUEK            GET_TXT(TXT_PD_BLUEK)
#define PD_REDK             GET_TXT(TXT_PD_REDK)
#define PD_YELLOWK          GET_TXT(TXT_PD_YELLOWK)

#define ALLALONE            GET_TXT(TXT_ALLALONE) // d64tc
#define TXT_GAMESAVED       GET_TXT(TXT_GGSAVED)

#define HUSTR_MSGU          GET_TXT(TXT_HUSTR_MSGU)
#define HUSTR_E1M01         GET_TXT(TXT_HUSTR_E1M01)
#define HUSTR_E1M02         GET_TXT(TXT_HUSTR_E1M02)
#define HUSTR_E1M03         GET_TXT(TXT_HUSTR_E1M03)
#define HUSTR_E1M04         GET_TXT(TXT_HUSTR_E1M04)
#define HUSTR_E1M05         GET_TXT(TXT_HUSTR_E1M05)
#define HUSTR_E1M06         GET_TXT(TXT_HUSTR_E1M06)
#define HUSTR_E1M07         GET_TXT(TXT_HUSTR_E1M07)
#define HUSTR_E1M08         GET_TXT(TXT_HUSTR_E1M08)
#define HUSTR_E1M09         GET_TXT(TXT_HUSTR_E1M09)
#define HUSTR_E1M10         GET_TXT(TXT_HUSTR_E1M10)
#define HUSTR_E1M11         GET_TXT(TXT_HUSTR_E1M11)
#define HUSTR_E1M12         GET_TXT(TXT_HUSTR_E1M12)
#define HUSTR_E1M13         GET_TXT(TXT_HUSTR_E1M13)
#define HUSTR_E1M14         GET_TXT(TXT_HUSTR_E1M14)
#define HUSTR_E1M15         GET_TXT(TXT_HUSTR_E1M15)
#define HUSTR_E1M16         GET_TXT(TXT_HUSTR_E1M16)
#define HUSTR_E1M17         GET_TXT(TXT_HUSTR_E1M17)
#define HUSTR_E1M18         GET_TXT(TXT_HUSTR_E1M18)
#define HUSTR_E1M19         GET_TXT(TXT_HUSTR_E1M19)
#define HUSTR_E1M20         GET_TXT(TXT_HUSTR_E1M20)
#define HUSTR_E1M21         GET_TXT(TXT_HUSTR_E1M21)
#define HUSTR_E1M22         GET_TXT(TXT_HUSTR_E1M22)
#define HUSTR_E1M23         GET_TXT(TXT_HUSTR_E1M23)
#define HUSTR_E1M24         GET_TXT(TXT_HUSTR_E1M24)
#define HUSTR_E1M25         GET_TXT(TXT_HUSTR_E1M25)
#define HUSTR_E1M26         GET_TXT(TXT_HUSTR_E1M26)
#define HUSTR_E1M27         GET_TXT(TXT_HUSTR_E1M27)
#define HUSTR_E1M28         GET_TXT(TXT_HUSTR_E1M28)
#define HUSTR_E1M29         GET_TXT(TXT_HUSTR_E1M29)
#define HUSTR_E1M30         GET_TXT(TXT_HUSTR_E1M30)
#define HUSTR_E1M31         GET_TXT(TXT_HUSTR_E1M31)
#define HUSTR_E1M32         GET_TXT(TXT_HUSTR_E1M32)
#define HUSTR_E1M33         GET_TXT(TXT_HUSTR_E1M33)
#define HUSTR_E1M34         GET_TXT(TXT_HUSTR_E1M34)
#define HUSTR_E1M35         GET_TXT(TXT_HUSTR_E1M35)
#define HUSTR_E1M36         GET_TXT(TXT_HUSTR_E1M36)
#define HUSTR_E1M37         GET_TXT(TXT_HUSTR_E1M37)
#define HUSTR_E1M38         GET_TXT(TXT_HUSTR_E1M38)
#define HUSTR_E1M39         GET_TXT(TXT_HUSTR_E1M39)
#define HUSTR_E2M01         GET_TXT(TXT_HUSTR_E2M01)
#define HUSTR_E2M02         GET_TXT(TXT_HUSTR_E2M02)
#define HUSTR_E2M03         GET_TXT(TXT_HUSTR_E2M03)
#define HUSTR_E2M04         GET_TXT(TXT_HUSTR_E2M04)
#define HUSTR_E2M05         GET_TXT(TXT_HUSTR_E2M05)
#define HUSTR_E2M06         GET_TXT(TXT_HUSTR_E2M06)
#define HUSTR_E2M07         GET_TXT(TXT_HUSTR_E2M07)
#define HUSTR_CHATMACRO1    GET_TXT(TXT_HUSTR_CHATMACRO1)
#define HUSTR_CHATMACRO2    GET_TXT(TXT_HUSTR_CHATMACRO2)
#define HUSTR_CHATMACRO3    GET_TXT(TXT_HUSTR_CHATMACRO3)
#define HUSTR_CHATMACRO4    GET_TXT(TXT_HUSTR_CHATMACRO4)
#define HUSTR_CHATMACRO5    GET_TXT(TXT_HUSTR_CHATMACRO5)
#define HUSTR_CHATMACRO6    GET_TXT(TXT_HUSTR_CHATMACRO6)
#define HUSTR_CHATMACRO7    GET_TXT(TXT_HUSTR_CHATMACRO7)
#define HUSTR_CHATMACRO8    GET_TXT(TXT_HUSTR_CHATMACRO8)
#define HUSTR_CHATMACRO9    GET_TXT(TXT_HUSTR_CHATMACRO9)
#define HUSTR_CHATMACRO0    GET_TXT(TXT_HUSTR_CHATMACRO0)
#define HUSTR_TALKTOSELF1   GET_TXT(TXT_HUSTR_TALKTOSELF1)
#define HUSTR_TALKTOSELF2   GET_TXT(TXT_HUSTR_TALKTOSELF2)
#define HUSTR_TALKTOSELF3   GET_TXT(TXT_HUSTR_TALKTOSELF3)
#define HUSTR_TALKTOSELF4   GET_TXT(TXT_HUSTR_TALKTOSELF4)
#define HUSTR_TALKTOSELF5   GET_TXT(TXT_HUSTR_TALKTOSELF5)
#define HUSTR_MESSAGESENT   GET_TXT(TXT_HUSTR_MESSAGESENT)

/**
 * The following should NOT be changed unless it seems just AWFULLY
 * necessary!
 */
#define HUSTR_PLRGREEN      GET_TXT(TXT_HUSTR_PLRGREEN)
#define HUSTR_PLRINDIGO     GET_TXT(TXT_HUSTR_PLRINDIGO)
#define HUSTR_PLRBROWN      GET_TXT(TXT_HUSTR_PLRBROWN)
#define HUSTR_PLRRED        GET_TXT(TXT_HUSTR_PLRRED)

//// \fixme What are these doing here??
#define HUSTR_KEYGREEN      'g'
#define HUSTR_KEYINDIGO     'i'
#define HUSTR_KEYBROWN      'b'
#define HUSTR_KEYRED        'r'

#define AMSTR_FOLLOWON      GET_TXT(TXT_AMSTR_FOLLOWON)
#define AMSTR_FOLLOWOFF     GET_TXT(TXT_AMSTR_FOLLOWOFF)
#define AMSTR_ROTATEON      GET_TXT(TXT_AMSTR_ROTATEON)
#define AMSTR_ROTATEOFF     GET_TXT(TXT_AMSTR_ROTATEOFF)
#define AMSTR_GRIDON        GET_TXT(TXT_AMSTR_GRIDON)
#define AMSTR_GRIDOFF       GET_TXT(TXT_AMSTR_GRIDOFF)
#define AMSTR_MARKEDSPOT    GET_TXT(TXT_AMSTR_MARKEDSPOT)
#define AMSTR_MARKSCLEARED  GET_TXT(TXT_AMSTR_MARKSCLEARED)

// Weapon names:
#define WEAPON0             GET_TXT(TXT_WEAPON0)
#define WEAPON1             GET_TXT(TXT_WEAPON1)
#define WEAPON2             GET_TXT(TXT_WEAPON2)
#define WEAPON3             GET_TXT(TXT_WEAPON3)
#define WEAPON4             GET_TXT(TXT_WEAPON4)
#define WEAPON5             GET_TXT(TXT_WEAPON5)
#define WEAPON6             GET_TXT(TXT_WEAPON6)
#define WEAPON7             GET_TXT(TXT_WEAPON7)
#define WEAPON8             GET_TXT(TXT_WEAPON8)
#define WEAPON9             GET_TXT(TXT_WEAPON9) // d64tc

#define STSTR_MUS           GET_TXT(TXT_STSTR_MUS)
#define STSTR_NOMUS         GET_TXT(TXT_STSTR_NOMUS)
#define STSTR_DQDON         GET_TXT(TXT_STSTR_DQDON)
#define STSTR_DQDOFF        GET_TXT(TXT_STSTR_DQDOFF)
#define STSTR_KFAADDED      GET_TXT(TXT_STSTR_KFAADDED)
#define STSTR_FAADDED       GET_TXT(TXT_STSTR_FAADDED)
#define STSTR_NCON          GET_TXT(TXT_STSTR_NCON)
#define STSTR_NCOFF         GET_TXT(TXT_STSTR_NCOFF)
#define STSTR_BEHOLD        GET_TXT(TXT_STSTR_BEHOLD)
#define STSTR_BEHOLDX       GET_TXT(TXT_STSTR_BEHOLDX)
#define STSTR_CHOPPERS      GET_TXT(TXT_STSTR_CHOPPERS)
#define STSTR_CLEV          GET_TXT(TXT_STSTR_CLEV)

#define C1TEXT              GET_TXT(TXT_C1TEXT)
#define C2TEXT              GET_TXT(TXT_C2TEXT)
#define C3TEXT              GET_TXT(TXT_C3TEXT)
#define C4TEXT              GET_TXT(TXT_C4TEXT)
#define C5TEXT              GET_TXT(TXT_C5TEXT)
#define C6TEXT              GET_TXT(TXT_C6TEXT)
#define C7TEXT              GET_TXT(TXT_C7TEXT) // d64tc
#define C8TEXT              GET_TXT(TXT_C8TEXT) // d64tc
#define C9TEXT              GET_TXT(TXT_C9TEXT) // d64tc

#define OUCAST1TEXT         GET_TXT(TXT_OUCAST1TEXT) // d64tc
#define OUCAST2TEXT         GET_TXT(TXT_OUCAST2TEXT) // d64tc

#define CC_ZOMBIE           GET_TXT(TXT_CC_ZOMBIE)
#define CC_SHOTGUN          GET_TXT(TXT_CC_SHOTGUN)
#define CC_HEAVY            GET_TXT(TXT_CC_HEAVY)
#define CC_IMP              GET_TXT(TXT_CC_IMP)
#define CC_DEMON            GET_TXT(TXT_CC_DEMON)
#define CC_LOST             GET_TXT(TXT_CC_LOST)
#define CC_CACO             GET_TXT(TXT_CC_CACO)
#define CC_HELL             GET_TXT(TXT_CC_HELL)
#define CC_BARON            GET_TXT(TXT_CC_BARON)
#define CC_ARACH            GET_TXT(TXT_CC_ARACH)
#define CC_PAIN             GET_TXT(TXT_CC_PAIN)
#define CC_REVEN            GET_TXT(TXT_CC_REVEN)
#define CC_MANCU            GET_TXT(TXT_CC_MANCU)
#define CC_ARCH             GET_TXT(TXT_CC_ARCH)
#define CC_SPIDER           GET_TXT(TXT_CC_SPIDER)
#define CC_CYBER            GET_TXT(TXT_CC_CYBER)
#define CC_NTROOP           GET_TXT(TXT_CC_NTROOP) // d64tc
#define CC_NDEMON           GET_TXT(TXT_CC_NDEMON) // d64tc
#define CC_NHEAD            GET_TXT(TXT_CC_NHEAD) // d64tc
#define CC_BITCH            GET_TXT(TXT_CC_BITCH) // d64tc
#define CC_HERO             GET_TXT(TXT_CC_HERO)

#endif
