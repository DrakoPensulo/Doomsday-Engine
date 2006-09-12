/**\file
 *\section Copyright and License Summary
 * License: GPL
 * Online License Link: http://www.gnu.org/licenses/gpl.html
 *
 *\author Copyright � 2003-2006 Jaakko Ker�nen <skyjake@dengine.net>
 *\author Copyright � 2006 Daniel Swanson <danij@dengine.net>
 *\author Copyright � 2003-2005 Samuel Villarreal <svkaiser@gmail.com>
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

/*
 * Sfx and music identifiers
 * Generated with DED Manager 1.0
 */

#ifndef __AUDIO_CONSTANTS_H__
#define __AUDIO_CONSTANTS_H__

// Sounds.
typedef enum {
    sfx_None,                      // 000
    sfx_pistol,                    // 001
    sfx_shotgn,                    // 002
    sfx_sgcock,                    // 003
    sfx_dshtgn,                    // 004
    sfx_dbopn,                     // 005
    sfx_dbcls,                     // 006
    sfx_dbload,                    // 007
    sfx_plasma,                    // 008
    sfx_bfg,                       // 009
    sfx_sawup,                     // 010
    sfx_sawidl,                    // 011
    sfx_sawful,                    // 012
    sfx_sawhit,                    // 013
    sfx_rlaunc,                    // 014
    sfx_rxplod,                    // 015
    sfx_firsht,                    // 016
    sfx_firxpl,                    // 017
    sfx_pstart,                    // 018
    sfx_pstop,                     // 019
    sfx_doropn,                    // 020
    sfx_dorcls,                    // 021
    sfx_stnmov,                    // 022
    sfx_swtchn,                    // 023
    sfx_swtchx,                    // 024
    sfx_plpain,                    // 025
    sfx_dmpain,                    // 026
    sfx_popain,                    // 027
    sfx_vipain,                    // 028
    sfx_mnpain,                    // 029
    sfx_pepain,                    // 030
    sfx_slop,                      // 031
    sfx_itemup,                    // 032
    sfx_wpnup,                     // 033
    sfx_oof,                       // 034
    sfx_telept,                    // 035
    sfx_posit1,                    // 036
    sfx_posit2,                    // 037
    sfx_posit3,                    // 038
    sfx_bgsit1,                    // 039
    sfx_bgsit2,                    // 040
    sfx_sgtsit,                    // 041
    sfx_cacsit,                    // 042
    sfx_brssit,                    // 043
    sfx_cybsit,                    // 044
    sfx_spisit,                    // 045
    sfx_bspsit,                    // 046
    sfx_kntsit,                    // 047
    sfx_vilsit,                    // 048
    sfx_mansit,                    // 049
    sfx_pesit,                     // 050
    sfx_sklatk,                    // 051
    sfx_sgtatk,                    // 052
    sfx_skepch,                    // 053
    sfx_vilatk,                    // 054
    sfx_claw,                      // 055
    sfx_skeswg,                    // 056
    sfx_pldeth,                    // 057
    sfx_pdiehi,                    // 058
    sfx_podth1,                    // 059
    sfx_podth2,                    // 060
    sfx_podth3,                    // 061
    sfx_bgdth1,                    // 062
    sfx_bgdth2,                    // 063
    sfx_sgtdth,                    // 064
    sfx_cacdth,                    // 065
    sfx_skldth,                    // 066
    sfx_brsdth,                    // 067
    sfx_cybdth,                    // 068
    sfx_spidth,                    // 069
    sfx_bspdth,                    // 070
    sfx_vildth,                    // 071
    sfx_kntdth,                    // 072
    sfx_pedth,                     // 073
    sfx_skedth,                    // 074
    sfx_posact,                    // 075
    sfx_bgact,                     // 076
    sfx_dmact,                     // 077
    sfx_bspact,                    // 078
    sfx_bspwlk,                    // 079
    sfx_vilact,                    // 080
    sfx_noway,                     // 081
    sfx_barexp,                    // 082
    sfx_punch,                     // 083
    sfx_hoof,                      // 084
    sfx_metal,                     // 085
    sfx_chgun,                     // 086
    sfx_tink,                      // 087
    sfx_bdopn,                     // 088
    sfx_bdcls,                     // 089
    sfx_itmbk,                     // 090
    sfx_flame,                     // 091
    sfx_flamst,                    // 092
    sfx_getpow,                    // 093
    sfx_bospit,                    // 094
    sfx_boscub,                    // 095
    sfx_bossit,                    // 096
    sfx_bospn,                     // 097
    sfx_bosdth,                    // 098
    sfx_manatk,                    // 099
    sfx_mandth,                    // 100
    sfx_sssit,                     // 101
    sfx_ssdth,                     // 102
    sfx_keenpn,                    // 103
    sfx_keendt,                    // 104
    sfx_skeact,                    // 105
    sfx_skesit,                    // 106
    sfx_skeatk,                    // 107
    sfx_radio,                     // 108
    //sfx_wsplash, // d64tc
    //sfx_nsplash, // d64tc
    //sfx_blurb, // d64tc
    // d64tc >
    sfx_psidl,
    sfx_laser,
    sfx_mthatk,
    sfx_mthsit,
    sfx_mthpai,
    sfx_mthact,
    sfx_mthdth,
    sfx_stlkst,
    sfx_stlkpn,
    sfx_stlktp,
    sfx_htime,
    // < d64tc
    NUMSFX
} sfxenum_t;

// Music.
typedef enum {
    mus_None,                      // 000
    mus_runnin,                    // 033
    mus_stalks,                    // 034
    mus_countd,                    // 035
    mus_betwee,                    // 036
    mus_doom,                      // 037
    mus_the_da,                    // 038
    mus_ddtblu,                    // 040
    mus_dead,                      // 042
    mus_stlks2,                    // 043
    mus_theda2,                    // 044
    mus_doom2,                     // 045
    mus_ddtbl2,                    // 046
    mus_runni2,                    // 047
    mus_stlks3,                    // 049
    mus_shawn2,                    // 051
    mus_count2,                    // 053
    mus_ddtbl3,                    // 054
    mus_ampie,                     // 055
    mus_evil,                      // 063
    mus_read_m,                    // 065
    mus_dm2ttl,                    // 066
    mus_dm2int,                    // 067
    NUMMUSIC                       // 068
} musicenum_t;

#endif
