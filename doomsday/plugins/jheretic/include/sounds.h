/**\file
 *\section License
 * License: GPL + jHeretic/jHexen Exception
 * Online License Link: http://www.gnu.org/licenses/gpl.html
 *
 *\author Copyright © 2003-2007 Jaakko Keränen <jaakko.keranen@iki.fi>
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
 *
 * In addition, as a special exception, we, the authors of deng
 * give permission to link the code of our release of deng with
 * the libjhexen and/or the libjheretic libraries (or with modified
 * versions of it that use the same license as the libjhexen or
 * libjheretic libraries), and distribute the linked executables.
 * You must obey the GNU General Public License in all respects for
 * all of the code used other than “libjhexen or libjheretic”. If
 * you modify this file, you may extend this exception to your
 * version of the file, but you are not obligated to do so. If you
 * do not wish to do so, delete this exception statement from your version.
 */

// Sfx and music identifiers
// Generated with DED Manager 1.1

#ifndef __AUDIO_CONSTANTS_H__
#define __AUDIO_CONSTANTS_H__

#ifndef __JHERETIC__
#  error "Using jHeretic headers without __JHERETIC__"
#endif

// Sounds.
typedef enum {
	sfx_None,					   // 000
	sfx_gldhit,					   // 001
	sfx_gntful,					   // 002
	sfx_gnthit,					   // 003
	sfx_gntpow,					   // 004
	sfx_gntact,					   // 005
	sfx_gntuse,					   // 006
	sfx_phosht,					   // 007
	sfx_phohit,					   // 008
	sfx_phopow,					   // 009
	sfx_lobsht,					   // 010
	sfx_lobhit,					   // 011
	sfx_lobpow,					   // 012
	sfx_hrnsht,					   // 013
	sfx_hrnhit,					   // 014
	sfx_hrnpow,					   // 015
	sfx_ramphit,				   // 016
	sfx_ramrain,				   // 017
	sfx_bowsht,					   // 018
	sfx_stfhit,					   // 019
	sfx_stfpow,					   // 020
	sfx_stfcrk,					   // 021
	sfx_impsit,					   // 022
	sfx_impat1,					   // 023
	sfx_impat2,					   // 024
	sfx_impdth,					   // 025
	sfx_impact,					   // 026
	sfx_imppai,					   // 027
	sfx_mumsit,					   // 028
	sfx_mumat1,					   // 029
	sfx_mumat2,					   // 030
	sfx_mumdth,					   // 031
	sfx_mumact,					   // 032
	sfx_mumpai,					   // 033
	sfx_mumhed,					   // 034
	sfx_bstsit,					   // 035
	sfx_bstatk,					   // 036
	sfx_bstdth,					   // 037
	sfx_bstact,					   // 038
	sfx_bstpai,					   // 039
	sfx_clksit,					   // 040
	sfx_clkatk,					   // 041
	sfx_clkdth,					   // 042
	sfx_clkact,					   // 043
	sfx_clkpai,					   // 044
	sfx_snksit,					   // 045
	sfx_snkatk,					   // 046
	sfx_snkdth,					   // 047
	sfx_snkact,					   // 048
	sfx_snkpai,					   // 049
	sfx_kgtsit,					   // 050
	sfx_kgtatk,					   // 051
	sfx_kgtat2,					   // 052
	sfx_kgtdth,					   // 053
	sfx_kgtact,					   // 054
	sfx_kgtpai,					   // 055
	sfx_wizsit,					   // 056
	sfx_wizatk,					   // 057
	sfx_wizdth,					   // 058
	sfx_wizact,					   // 059
	sfx_wizpai,					   // 060
	sfx_minsit,					   // 061
	sfx_minat1,					   // 062
	sfx_minat2,					   // 063
	sfx_minat3,					   // 064
	sfx_mindth,					   // 065
	sfx_minact,					   // 066
	sfx_minpai,					   // 067
	sfx_hedsit,					   // 068
	sfx_hedat1,					   // 069
	sfx_hedat2,					   // 070
	sfx_hedat3,					   // 071
	sfx_heddth,					   // 072
	sfx_hedact,					   // 073
	sfx_hedpai,					   // 074
	sfx_sorzap,					   // 075
	sfx_sorrise,				   // 076
	sfx_sorsit,					   // 077
	sfx_soratk,					   // 078
	sfx_soract,					   // 079
	sfx_sorpai,					   // 080
	sfx_sordsph,				   // 081
	sfx_sordexp,				   // 082
	sfx_sordbon,				   // 083
	sfx_sbtsit,					   // 084
	sfx_sbtatk,					   // 085
	sfx_sbtdth,					   // 086
	sfx_sbtact,					   // 087
	sfx_sbtpai,					   // 088
	sfx_plroof,					   // 089
	sfx_plrpai,					   // 090
	sfx_plrdth,					   // 091
	sfx_gibdth,					   // 092
	sfx_plrwdth,				   // 093
	sfx_plrcdth,				   // 094
	sfx_itemup,					   // 095
	sfx_wpnup,					   // 096
	sfx_telept,					   // 097
	sfx_doropn,					   // 098
	sfx_dorcls,					   // 099
	sfx_dormov,					   // 100
	sfx_artiup,					   // 101
	sfx_switch,					   // 102
	sfx_pstart,					   // 103
	sfx_pstop,					   // 104
	sfx_stnmov,					   // 105
	sfx_chicpai,				   // 106
	sfx_chicatk,				   // 107
	sfx_chicdth,				   // 108
	sfx_chicact,				   // 109
	sfx_chicpk1,				   // 110
	sfx_chicpk2,				   // 111
	sfx_chicpk3,				   // 112
	sfx_keyup,					   // 113
	sfx_ripslop,				   // 114
	sfx_newpod,					   // 115
	sfx_podexp,					   // 116
	sfx_bounce,					   // 117
	sfx_volsht,					   // 118
	sfx_volhit,					   // 119
	sfx_burn,					   // 120
	sfx_splash,					   // 121
	sfx_gloop,					   // 122
	sfx_respawn,				   // 123
	sfx_blssht,					   // 124
	sfx_blshit,					   // 125
	sfx_chat,					   // 126
	sfx_artiuse,				   // 127
	sfx_gfrag,					   // 128
	sfx_waterfl,				   // 129
	sfx_wind,					   // 130
	sfx_amb1,					   // 131
	sfx_amb2,					   // 132
	sfx_amb3,					   // 133
	sfx_amb4,					   // 134
	sfx_amb5,					   // 135
	sfx_amb6,					   // 136
	sfx_amb7,					   // 137
	sfx_amb8,					   // 138
	sfx_amb9,					   // 139
	sfx_amb10,					   // 140
	sfx_amb11,					   // 141
	NUMSFX						   // 142
} sfxenum_t;

// Music.
typedef enum {
	mus_e1m1,					   // 000
	mus_e1m2,					   // 001
	mus_e1m3,					   // 002
	mus_e1m4,					   // 003
	mus_e1m5,					   // 004
	mus_e1m6,					   // 005
	mus_e1m7,					   // 006
	mus_e1m8,					   // 007
	mus_e1m9,					   // 008
	mus_e2m1,					   // 009
	mus_e2m2,					   // 010
	mus_e2m3,					   // 011
	mus_e2m4,					   // 012
	mus_e2m5,					   // 013
	mus_e2m6,					   // 014
	mus_e2m7,					   // 015
	mus_e2m8,					   // 016
	mus_e2m9,					   // 017
	mus_e3m1,					   // 018
	mus_e3m2,					   // 019
	mus_e3m3,					   // 020
	mus_e3m4,					   // 021
	mus_e3m5,					   // 022
	mus_e3m6,					   // 023
	mus_e3m7,					   // 024
	mus_e3m8,					   // 025
	mus_e3m9,					   // 026
	mus_e4m1,					   // 027
	mus_e4m2,					   // 028
	mus_e4m3,					   // 029
	mus_e4m4,					   // 030
	mus_e4m5,					   // 031
	mus_e4m6,					   // 032
	mus_e4m7,					   // 033
	mus_e4m8,					   // 034
	mus_e4m9,					   // 035
	mus_e5m1,					   // 036
	mus_e5m2,					   // 037
	mus_e5m3,					   // 038
	mus_e5m4,					   // 039
	mus_e5m5,					   // 040
	mus_e5m6,					   // 041
	mus_e5m7,					   // 042
	mus_e5m8,					   // 043
	mus_e5m9,					   // 044
	mus_e6m1,					   // 045
	mus_e6m2,					   // 046
	mus_e6m3,					   // 047
	mus_titl,					   // 048
	mus_intr,					   // 049
	mus_cptd,					   // 050
	NUMMUSIC					   // 051
} musicenum_t;

#endif
