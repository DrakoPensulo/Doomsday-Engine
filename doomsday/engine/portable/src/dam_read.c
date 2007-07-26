/**\file
 *\section License
 * License: GPL
 * Online License Link: http://www.gnu.org/licenses/gpl.html
 *
 *\author Copyright © 2003-2007 Jaakko Keränen <jaakko.keranen@iki.fi>
 *\author Copyright © 2006-2007 Daniel Swanson <danij@dengine.net>
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
 * dam_read.c: Doomsday Archived Map (DAM), reader.
 */

// HEADER FILES ------------------------------------------------------------

#include "de_base.h"
#include "de_dam.h"
#include "de_defs.h"
#include "de_misc.h"

#include "p_mapdata.h"

// MACROS ------------------------------------------------------------------

// TYPES -------------------------------------------------------------------

typedef struct {
    gamemap_t  *map;
    size_t      elmsize;
    uint        elements;
    uint        numProps;
    readprop_t *props;
} damlumpreadargs_t;

typedef struct setargs_s {
    gamemap_t  *map;
    int         type;
    uint        prop;

    valuetype_t valueType;
    boolean    *booleanValues;
    byte       *byteValues;
    short      *shortValues;
    int        *intValues;
    uint       *uintValues;
    fixed_t    *fixedValues;
    long       *longValues;
    unsigned long *ulongValues;
    float      *floatValues;
    angle_t    *angleValues;
    void      **ptrValues;
} damsetargs_t;

// EXTERNAL FUNCTION PROTOTYPES --------------------------------------------

void *DAM_IndexToPtr(gamemap_t *map, int objectType, uint id);

// PUBLIC FUNCTION PROTOTYPES ----------------------------------------------

// PRIVATE FUNCTION PROTOTYPES ---------------------------------------------

static int ReadAndCallback(int dataType, unsigned int startIndex,
                           const byte *buffer, void* context,
                           int (*callback)(int type, uint index, void* ctx));

// EXTERNAL DATA DECLARATIONS ----------------------------------------------

// PUBLIC DATA DEFINITIONS -------------------------------------------------

// PRIVATE DATA DEFINITIONS ------------------------------------------------

// CODE --------------------------------------------------------------------

/**
 * Initializes a damsetargs struct.
 *
 * @param type  Type of the map data object (e.g., DAM_LINE).
 * @param prop  Property of the map data object.
 */
static void initArgs(damsetargs_t* args, int type, uint prop)
{
    memset(args, 0, sizeof(*args));
    args->type = type;
    args->prop = prop;
}

boolean DAM_ReadMapDataFromLump(gamemap_t *map, mapdatalumpinfo_t *mapLump,
                                uint startIndex, readprop_t *props,
                                uint numProps,
                                int (*callback)(int type, uint index, void* context))
{
    int         type = DAM_MapLumpInfoForLumpClass(mapLump->lumpClass)->dataType;
    damlumpreadargs_t   args;

    // Is this a supported lump type?
    switch(type)
    {
    case DAM_THING:
    case DAM_VERTEX:
    case DAM_LINE:
    case DAM_SIDE:
    case DAM_SECTOR:
    case DAM_SEG:
    case DAM_SUBSECTOR:
    case DAM_NODE:
        break;

    default:
        return false; // Read from this lump type is not supported.
    }

     // Select the lump size, number of elements etc...
    args.map = map;
    args.elmsize = Def_GetMapLumpFormat(mapLump->format->formatName)->elmsize;
    args.elements = mapLump->elements;
    args.numProps = numProps;
    args.props = props;

    // Have we cached the lump yet?
    if(mapLump->lumpp == NULL)
        mapLump->lumpp = (byte *) W_CacheLumpNum(mapLump->lumpNum, PU_STATIC);

    // Read in that data!
    // NOTE: We'll leave the lump cached, our caller probably knows better
    // than us whether it should be free'd.
    return ReadAndCallback(type, startIndex,
                           (mapLump->lumpp + mapLump->startOffset),
                           &args, callback);
}

/**
 * Sets a value. Does some basic type checking so that incompatible types are
 * not assigned. Simple conversions are also done, e.g., float to fixed.
 */
static void SetValue(valuetype_t valueType, void* dst, damsetargs_t* args,
                     uint index)
{
    if(valueType == DDVT_FIXED)
    {
        fixed_t* d = dst;

        switch(args->valueType)
        {
        case DDVT_BYTE:
            *d = (args->byteValues[index] << FRACBITS);
            break;
        case DDVT_INT:
            *d = (args->intValues[index] << FRACBITS);
            break;
        case DDVT_FIXED:
            *d = args->fixedValues[index];
            break;
        case DDVT_FLOAT:
            *d = FLT2FIX(args->floatValues[index]);
            break;
        default:
            Con_Error("SetValue: DDVT_FIXED incompatible with value type %s.\n",
                      value_Str(args->valueType));
        }
    }
    else if(valueType == DDVT_FLOAT)
    {
        float* d = dst;

        switch(args->valueType)
        {
        case DDVT_BYTE:
            *d = args->byteValues[index];
            break;
        case DDVT_SHORT:
            *d = args->shortValues[index];
            break;
        case DDVT_UINT:
            *d = args->uintValues[index];
            break;
        case DDVT_INT:
            *d = args->intValues[index];
            break;
        case DDVT_FIXED:
            *d = FIX2FLT(args->fixedValues[index]);
            break;
        case DDVT_FLOAT:
            *d = args->floatValues[index];
            break;
        default:
            Con_Error("SetValue: DDVT_FLOAT incompatible with value type %s.\n",
                      value_Str(args->valueType));
        }
    }
    else if(valueType == DDVT_BOOL)
    {
        boolean* d = dst;

        switch(args->valueType)
        {
        case DDVT_BOOL:
            *d = args->booleanValues[index];
            break;
        default:
            Con_Error("SetValue: DDVT_BOOL incompatible with value type %s.\n",
                      value_Str(args->valueType));
        }
    }
    else if(valueType == DDVT_BYTE)
    {
        byte* d = dst;

        switch(args->valueType)
        {
        case DDVT_BOOL:
            *d = args->booleanValues[index];
            break;
        case DDVT_BYTE:
            *d = args->byteValues[index];
            break;
        case DDVT_SHORT:
            *d = args->shortValues[index];
            break;
        case DDVT_INT:
            *d = args->intValues[index];
            break;
        case DDVT_FLOAT:
            *d = (byte) args->floatValues[index];
            break;
        default:
            Con_Error("SetValue: DDVT_BYTE incompatible with value type %s.\n",
                      value_Str(args->valueType));
        }
    }
    else if(valueType == DDVT_INT)
    {
        int* d = dst;

        switch(args->valueType)
        {
        case DDVT_BOOL:
            *d = args->booleanValues[index];
            break;
        case DDVT_BYTE:
            *d = args->byteValues[index];
            break;
        case DDVT_INT:
            *d = args->intValues[index];
            break;
        case DDVT_FLOAT:
            *d = args->floatValues[index];
            break;
        case DDVT_FIXED:
            *d = (args->fixedValues[index] >> FRACBITS);
            break;
        default:
            Con_Error("SetValue: DDVT_INT incompatible with value type %s.\n",
                      value_Str(args->valueType));
        }
    }
    else if(valueType == DDVT_UINT)
    {
        uint* d = dst;

        switch(args->valueType)
        {
        case DDVT_BOOL:
            *d = args->booleanValues[index];
            break;
        case DDVT_BYTE:
            *d = args->byteValues[index];
            break;
        case DDVT_INT:
            *d = (unsigned) args->intValues[index];
            break;
        case DDVT_UINT:
            *d = args->uintValues[index];
            break;
        case DDVT_FLOAT:
            *d = (unsigned) args->floatValues[index];
            break;
        case DDVT_FIXED:
            *d = (unsigned) (args->fixedValues[index] >> FRACBITS);
            break;
        default:
            Con_Error("SetValue: DDVT_UINT incompatible with value type %s.\n",
                      value_Str(args->valueType));
        }
    }
    else if(valueType == DDVT_SHORT || valueType == DDVT_FLAT_INDEX)
    {
        short* d = dst;

        switch(args->valueType)
        {
        case DDVT_BOOL:
            *d = args->booleanValues[index];
            break;
        case DDVT_BYTE:
            *d = args->byteValues[index];
            break;
        case DDVT_SHORT:
            *d = args->shortValues[index];
            break;
        case DDVT_INT:
            *d = args->intValues[index];
            break;
        case DDVT_ULONG:
            *d = args->ulongValues[index];
            break;
        case DDVT_FLOAT:
            *d = args->floatValues[index];
            break;
        case DDVT_FIXED:
            *d = (args->fixedValues[index] >> FRACBITS);
            break;
        default:
            Con_Error("SetValue: DDVT_SHORT incompatible with value type %s.\n",
                      value_Str(args->valueType));
        }
    }
    else if(valueType == DDVT_ANGLE)
    {
        angle_t* d = dst;

        switch(args->valueType)
        {
        case DDVT_ANGLE:
            *d = args->angleValues[index];
            break;
        default:
            Con_Error("SetValue: DDVT_ANGLE incompatible with value type %s.\n",
                      value_Str(args->valueType));
        }
    }
    else if(valueType == DDVT_BLENDMODE)
    {
        blendmode_t* d = dst;

        switch(args->valueType)
        {
        case DDVT_INT:
            if(args->intValues[index] > DDNUM_BLENDMODES || args->intValues[index] < 0)
                Con_Error("SetValue: %d is not a valid value for DDVT_BLENDMODE.\n",
                          args->intValues[index]);

            *d = args->intValues[index];
            break;
        default:
            Con_Error("SetValue: DDVT_BLENDMODE incompatible with value type %s.\n",
                      value_Str(args->valueType));
        }
    }
    else if(valueType == DDVT_PTR)
    {
        void** d = dst;

        switch(args->valueType)
        {
        case DDVT_SECT_IDX:
            *(sector_t **) dst = DAM_IndexToPtr(args->map, DAM_SECTOR, (unsigned) args->longValues[index]);
            break;
        case DDVT_VERT_IDX:
            {
            long        value;
            // \fixme There has to be a better way to do this...
            value = DAM_VertexIdx(args->longValues[index]);
            // end FIXME
            *(vertex_t **) dst = DAM_IndexToPtr(args->map, DAM_VERTEX, (unsigned) value);
            }
            break;
        case DDVT_LINE_IDX:
            *(line_t **) dst = DAM_IndexToPtr(args->map, DAM_LINE, (unsigned) args->longValues[index]);
            break;
        case DDVT_SIDE_IDX:
            *(side_t **) dst = DAM_IndexToPtr(args->map, DAM_SIDE, (unsigned) args->longValues[index]);
            break;
        case DDVT_SEG_IDX:
            *(seg_t **) dst = DAM_IndexToPtr(args->map, DAM_SEG, (unsigned) args->longValues[index]);
            break;
        case DDVT_PTR:
            *d = args->ptrValues[index];
            break;
        default:
            Con_Error("SetValue: DDVT_PTR incompatible with value type %s.\n",
                      value_Str(args->valueType));
        }
    }
    else
    {
        Con_Error("SetValue: unknown value type %d.\n", valueType);
    }
}

/**
 * Reads a value from the (little endian) source buffer and writes it back
 * to the location specified by damsetargs_t.
 *
 * Does some basic type checking so that incompatible types are not
 * assigned. Simple conversions are also done, e.g., float to fixed.
 */
static void ReadValue(valuetype_t valueType, uint elmIdx, size_t size,
                      const byte *src, damsetargs_t *args, uint index,
                      int flags)
{
    if(valueType == DDVT_BYTE)
    {
        switch(size)
        {
        case 1:
        case 2:
        case 4:
            break;
        default:
            Con_Error("ReadValue: DDVT_BYTE no conversion from %i bytes.",
                      size);
        }
        args->byteValues[index] = *src;
    }
    else if(valueType == DDVT_FLOAT)
    {
        float       d = 0;
        switch(size)
        {
        case 2:
            if(flags & DT_UNSIGNED)
            {
                if(flags & DT_FRACBITS)
                    d = FIX2FLT(USHORT(*((int16_t*)(src))) << FRACBITS);
                else
                    d = FIX2FLT(USHORT(*((int16_t*)(src))));
            }
            else
            {
                if(flags & DT_FRACBITS)
                    d = FIX2FLT(SHORT(*((int16_t*)(src))) << FRACBITS);
                else
                    d = FIX2FLT(SHORT(*((int16_t*)(src))));
            }
            break;

        case 4:
            if(flags & DT_UNSIGNED)
            {
                if(flags & DT_FRACBITS)
                    d = FIX2FLT(ULONG(*((int32_t*)(src))) << FRACBITS);
                else
                    d = FIX2FLT(ULONG(*((int32_t*)(src))));
            }
            else
            {
                if(flags & DT_FRACBITS)
                    d = FIX2FLT(LONG(*((int32_t*)(src))) << FRACBITS);
                else
                    d = FIX2FLT(LONG(*((int32_t*)(src))));
            }
            break;

        default:
            Con_Error("ReadValue: DDVT_FLOAT no conversion from %i bytes.",
                      size);
        }
        args->floatValues[index] = d;
    }
    else if(valueType == DDVT_SHORT || valueType == DDVT_FLAT_INDEX)
    {
        short       d = 0;
        switch(size)
        {
        case 2:
            if(flags & DT_UNSIGNED)
            {
                if(flags & DT_FRACBITS)
                    d = USHORT(*((int16_t*)(src))) << FRACBITS;
                else
                    d = USHORT(*((int16_t*)(src)));
            }
            else
            {
                if(flags & DT_FRACBITS)
                    d = SHORT(*((int16_t*)(src))) << FRACBITS;
                else
                    d = SHORT(*((int16_t*)(src)));
            }
            break;

        case 8:
            {
            if(flags & DT_TEXTURE)
            {
                d = P_CheckTexture((char*)((int64_t*)(src)), false, valueType,
                                   elmIdx, args->prop);
            }
            else if(flags & DT_FLAT)
            {
                d = P_CheckTexture((char*)((int64_t*)(src)), true, valueType,
                                   elmIdx, args->prop);
            }
            break;
            }
         default:
            Con_Error("ReadValue: DDVT_SHORT no conversion from %i bytes.",
                      size);
         }
        args->shortValues[index] = d;
    }
    else if(valueType == DDVT_FIXED)
    {
        fixed_t     d = 0;
        switch(size) // Number of src bytes
        {
        case 2:
            if(flags & DT_UNSIGNED)
            {
                if(flags & DT_FRACBITS)
                    d = USHORT(*((int16_t*)(src))) << FRACBITS;
                else
                    d = USHORT(*((int16_t*)(src)));
            }
            else
            {
                if(flags & DT_FRACBITS)
                    d = SHORT(*((int16_t*)(src))) << FRACBITS;
                else
                    d = SHORT(*((int16_t*)(src)));
            }
            break;

        case 4:
            if(flags & DT_UNSIGNED)
                d = ULONG(*((int32_t*)(src)));
            else
                d = LONG(*((int32_t*)(src)));
            break;

        default:
            Con_Error("ReadValue: DDVT_FIXED no conversion from %i bytes.",
                      size);
        }
        args->fixedValues[index] = d;
    }
    else if(valueType == DDVT_ULONG)
    {
        unsigned long   d = 0;

        switch(size) // Number of src bytes
        {
        case 2:
            if(flags & DT_UNSIGNED)
            {
                if(flags & DT_FRACBITS)
                    d = USHORT(*((int16_t*)(src))) << FRACBITS;
                else
                    d = USHORT(*((int16_t*)(src)));
            }
            else
            {
                if(flags & DT_FRACBITS)
                    d = SHORT(*((int16_t*)(src))) << FRACBITS;
                else
                    d = SHORT(*((int16_t*)(src)));
            }
            break;

        case 4:
            if(flags & DT_UNSIGNED)
                d = ULONG(*((int32_t*)(src)));
            else
                d = LONG(*((int32_t*)(src)));
            break;

        default:
            Con_Error("ReadValue: DDVT_ULONG no conversion from %i bytes.",
                      size);
        }
        args->ulongValues[index] = d;
    }
    else if(valueType == DDVT_UINT)
    {
        unsigned int    d = 0;

        switch(size) // Number of src bytes
        {
        case 2:
            if(flags & DT_UNSIGNED)
            {
                if(flags & DT_FRACBITS)
                    d = USHORT(*((int16_t*)(src))) << FRACBITS;
                else
                    d = USHORT(*((int16_t*)(src)));
            }
            else
            {
                if(flags & DT_NOINDEX)
                {
                    unsigned short num = SHORT(*((int16_t*)(src)));

                    d = NO_INDEX;

                    if(num != ((unsigned short)-1))
                        d = num;
                }
                else
                {
                    if(flags & DT_FRACBITS)
                        d = SHORT(*((int16_t*)(src))) << FRACBITS;
                    else
                        d = SHORT(*((int16_t*)(src)));
                }
            }
            if((flags & DT_MSBCONVERT) && (d & 0x8000))
            {
                d &= ~0x8000;
                d |= 0x80000000;
            }
            break;

        case 4:
            if(flags & DT_UNSIGNED)
                d = ULONG(*((int32_t*)(src)));
            else
                d = LONG(*((int32_t*)(src)));
            break;

        default:
            Con_Error("ReadValue: DDVT_INT no conversion from %i bytes.",
                      size);
        }
        args->uintValues[index] = d;
    }
    else if(valueType == DDVT_VERT_IDX || valueType == DDVT_LINE_IDX ||
            valueType == DDVT_SIDE_IDX || valueType == DDVT_SECT_IDX ||
            valueType == DDVT_SEG_IDX)
    {
        long d = NO_INDEX;

        switch(size) // Number of src bytes
        {
        case 2:
            if(flags & DT_UNSIGNED)
            {
                d = USHORT(*((int16_t*)(src)));
            }
            else
            {
                if(flags & DT_NOINDEX)
                {
                    unsigned short num = SHORT(*((int16_t*)(src)));

                    if(num != ((unsigned short)-1))
                        d = num;
                }
                else
                {
                    d = SHORT(*((int16_t*)(src)));
                }
            }
            break;

        case 4:
            if(flags & DT_UNSIGNED)
                d = ULONG(*((int32_t*)(src)));
            else
                d = LONG(*((int32_t*)(src)));
            break;

        default:
            Con_Error("ReadValue: %s no conversion from %i bytes.\n",
                      value_Str(valueType), size);
        }
        args->longValues[index] = d;
    }
    else if(valueType == DDVT_INT)
    {
        int         d = 0;
        switch(size) // Number of src bytes
        {
        case 2:
            if(flags & DT_UNSIGNED)
            {
                if(flags & DT_FRACBITS)
                    d = USHORT(*((int16_t*)(src))) << FRACBITS;
                else
                    d = USHORT(*((int16_t*)(src)));
            }
            else
            {
                if(flags & DT_NOINDEX)
                {
                    unsigned short num = SHORT(*((int16_t*)(src)));

                    d = NO_INDEX;

                    if(num != ((unsigned short)-1))
                        d = num;
                }
                else
                {
                    if(flags & DT_FRACBITS)
                        d = SHORT(*((int16_t*)(src))) << FRACBITS;
                    else
                        d = SHORT(*((int16_t*)(src)));
                }
            }
            if((flags & DT_MSBCONVERT) && (d & 0x8000))
            {
                d &= ~0x8000;
                d |= 0x80000000;
            }
            break;

        case 4:
            if(flags & DT_UNSIGNED)
                d = ULONG(*((int32_t*)(src)));
            else
                d = LONG(*((int32_t*)(src)));
            break;

        default:
            Con_Error("ReadValue: DDVT_INT no conversion from %i bytes.",
                      size);
        }
        args->intValues[index] = d;
    }
    else if(valueType == DDVT_ANGLE)
    {
        angle_t     d = 0;
        switch(size) // Number of src bytes
        {
        case 2:
            if(flags & DT_FRACBITS)
                d = (angle_t) (SHORT(*((int16_t*)(src))) << FRACBITS);
            else
                d = (angle_t) SHORT(*((int16_t*)(src)));
            break;

        default:
            Con_Error("ReadValue: DDVT_ANGLE no conversion from %i bytes.",
                      size);
        }
        args->angleValues[index] = d;
    }
    else
    {
        Con_Error("ReadValue: unknown value type %s.\n",
                  value_Str(valueType));
    }
}

static int SetProperty2(void *ptr, void *context)
{
    damsetargs_t *args = (damsetargs_t*) context;

    // These are the exported map data properties that can be
    // assigned to when reading map data.
    switch(args->type)
    {
    case DAM_VERTEX:
        {
        vertex_t* p = ptr;

        switch(args->prop)
        {
        case DAM_X:
            SetValue(DMT_VERTEX_POS, &p->V_pos[VX], args, 0);
            break;

        case DAM_Y:
            SetValue(DMT_VERTEX_POS, &p->V_pos[VY], args, 0);
            break;

        default:
            Con_Error("SetProperty: DAM_VERTEX has no property %s.\n",
                      DAM_Str(args->prop));
        }
        break;
        }
    case DAM_LINE:
        {
        line_t* p = ptr;

        switch(args->prop)
        {
        case DAM_FLAGS:
            SetValue(DMT_LINE_MAPFLAGS, &p->mapflags, args, 0);
            break;

        case DAM_SIDE0:
            SetValue(DMT_LINE_SIDES, &p->L_frontside, args, 0);
            break;

        case DAM_SIDE1:
            SetValue(DMT_LINE_SIDES, &p->L_backside, args, 0);
            break;

        default:
            Con_Error("SetProperty: DAM_LINE has no property %s.\n",
                      DAM_Str(args->prop));
        }
        break;
        }
    case DAM_SIDE:
        {
        side_t* p = ptr;

        switch(args->prop)
        {
        case DAM_TOP_TEXTURE_OFFSET_X:
            SetValue(DMT_SURFACE_OFFX, &p->SW_topoffx, args, 0);
            break;

        case DAM_TOP_TEXTURE_OFFSET_Y:
            SetValue(DMT_SURFACE_OFFY, &p->SW_topoffy, args, 0);
            break;

        case DAM_MIDDLE_TEXTURE_OFFSET_X:
            SetValue(DMT_SURFACE_OFFX, &p->SW_middleoffx, args, 0);
            break;

        case DAM_MIDDLE_TEXTURE_OFFSET_Y:
            SetValue(DMT_SURFACE_OFFY, &p->SW_middleoffy, args, 0);
            break;

        case DAM_BOTTOM_TEXTURE_OFFSET_X:
            SetValue(DMT_SURFACE_OFFX, &p->SW_bottomoffx, args, 0);
            break;

        case DAM_BOTTOM_TEXTURE_OFFSET_Y:
            SetValue(DMT_SURFACE_OFFY, &p->SW_bottomoffy, args, 0);
            break;

        case DAM_TOP_TEXTURE:
            SetValue(DMT_MATERIAL_TEXTURE, &p->SW_toptexture, args, 0);
            break;

        case DAM_MIDDLE_TEXTURE:
            SetValue(DMT_MATERIAL_TEXTURE, &p->SW_middletexture, args, 0);
            break;

        case DAM_BOTTOM_TEXTURE:
            SetValue(DMT_MATERIAL_TEXTURE, &p->SW_bottomtexture, args, 0);
            break;

        case DAM_FRONT_SECTOR:
            SetValue(DMT_SIDE_SECTOR, &p->sector, args, 0);
            break;

        default:
            Con_Error("SetProperty: DAM_SIDE has no property %s.\n",
                      DAM_Str(args->prop));
        }
        break;
        }
    case DAM_SECTOR:
        {
        sector_t* p = ptr;

        switch(args->prop)
        {
        case DAM_FLOOR_HEIGHT:
            SetValue(DMT_PLANE_HEIGHT, &p->SP_floorheight, args, 0);
            break;

        case DAM_CEILING_HEIGHT:
            SetValue(DMT_PLANE_HEIGHT, &p->SP_ceilheight, args, 0);
            break;

        case DAM_FLOOR_TEXTURE:
            SetValue(DMT_MATERIAL_TEXTURE, &p->SP_floortexture, args, 0);
            break;

        case DAM_CEILING_TEXTURE:
            SetValue(DMT_MATERIAL_TEXTURE, &p->SP_ceiltexture, args, 0);
            break;

        case DAM_LIGHT_LEVEL:
            SetValue(DMT_SECTOR_LIGHTLEVEL, &p->lightlevel, args, 0);
            p->lightlevel /= 255.0f;
            break;

        default:
            Con_Error("SetProperty: DAM_SECTOR has no property %s.\n",
                      DAM_Str(args->prop));
        }
        break;
        }
    case DAM_SEG:
        {
        seg_t* p = ptr;

        switch(args->prop)
        {
        case DAM_VERTEX1:
            SetValue(DMT_SEG_V, &p->SG_v1, args, 0);
            break;

        case DAM_VERTEX2:
            SetValue(DMT_SEG_V, &p->SG_v2, args, 0);
            break;

        case DAM_LINE:
            SetValue(DMT_SEG_LINEDEF, &p->linedef, args, 0);
            break;

        case DAM_SIDE:
            SetValue(DMT_SEG_SIDE, &p->side, args, 0);
            break;

        case DAM_SEG:
            SetValue(DMT_SEG_BACKSEG, &p->backseg, args, 0);
            break;

        default:
            Con_Error("SetProperty: DAM_SEG has no property %s.\n",
                      DAM_Str(args->prop));
        }
        break;
        }
    case DAM_SUBSECTOR:
        {
        subsector_t* p = ptr;

        switch(args->prop)
        {
        case DAM_SEG_COUNT:
            SetValue(DMT_SUBSECTOR_SEGCOUNT, &p->segcount, args, 0);
            break;

        case DAM_SEG_FIRST:
            SetValue(DDVT_UINT, &p->firstWADSeg, args, 0);
            break;

        default:
            Con_Error("SetProperty: DAM_SUBSECTOR has no property %s.\n",
                      DAM_Str(args->prop));
        }
        break;
        }
    case DAM_NODE:
        {
        node_t* p = ptr;

        switch(args->prop)
        {
        case DAM_X:
            SetValue(DMT_NODE_X, &p->x, args, 0);
            break;

        case DAM_Y:
            SetValue(DMT_NODE_Y, &p->y, args, 0);
            break;

        case DAM_DX:
            SetValue(DMT_NODE_DX, &p->dx, args, 0);
            break;

        case DAM_DY:
            SetValue(DMT_NODE_DY, &p->dy, args, 0);
            break;

        case DAM_BBOX_RIGHT_TOP_Y:
            SetValue(DMT_NODE_BBOX, &p->bbox[0][0], args, 0);
            break;

        case DAM_BBOX_RIGHT_LOW_Y:
            SetValue(DMT_NODE_BBOX, &p->bbox[0][1], args, 0);
            break;

        case DAM_BBOX_RIGHT_LOW_X:
            SetValue(DMT_NODE_BBOX, &p->bbox[0][2], args, 0);
            break;

        case DAM_BBOX_RIGHT_TOP_X:
            SetValue(DMT_NODE_BBOX, &p->bbox[0][3], args, 0);
            break;

        case DAM_BBOX_LEFT_TOP_Y:
            SetValue(DMT_NODE_BBOX, &p->bbox[1][0], args, 0);
            break;

        case DAM_BBOX_LEFT_LOW_Y:
            SetValue(DMT_NODE_BBOX, &p->bbox[1][1], args, 0);
            break;

        case DAM_BBOX_LEFT_LOW_X:
            SetValue(DMT_NODE_BBOX, &p->bbox[1][2], args, 0);
            break;

        case DAM_BBOX_LEFT_TOP_X:
            SetValue(DMT_NODE_BBOX, &p->bbox[1][3], args, 0);
            break;

        case DAM_CHILD_RIGHT:
            SetValue(DMT_NODE_CHILDREN, &p->children[0], args, 0);
            break;

        case DAM_CHILD_LEFT:
            SetValue(DMT_NODE_CHILDREN, &p->children[1], args, 0);
            break;

        default:
            Con_Error("SetProperty: DAM_NODE has no property %s.\n",
                      DAM_Str(args->prop));
        }
        break;
        }
    default:
        Con_Error("SetProperty: Type cannot be assigned to from a map format.\n");
    }

    return true; // Continue iteration
}

int DAM_SetProperty(int type, uint idx, void *context)
{
    uint        index = idx;
    damsetargs_t *args = (damsetargs_t*) context;

    // Handle unknown (game specific) properties.
    if(args->prop >= NUM_DAM_PROPERTIES)
    {
        void        *data = NULL;

        if(!gx.HandleMapDataProperty)
            return true; // Continue iteration.

        switch(args->valueType)
        {
        case DDVT_BYTE:
            data = &args->byteValues[0];
            break;
        case DDVT_SHORT:
            data = &args->shortValues[0];
            break;
        case DDVT_FIXED:
            data = &args->fixedValues[0];
            break;
        case DDVT_INT:
            data = &args->intValues[0];
            break;
        case DDVT_FLOAT:
            data = &args->floatValues[0];
            break;
        default:
            Con_Error("SetProperty: Unsupported data type id %s.\n",
                      value_Str(args->valueType));
        };
        gx.HandleMapDataProperty(index, args->type, args->prop,
                                 args->valueType, data);
        return true; // Continue iteration.
    }
    else
    {
        void       *ptr = (type == DAM_THING? &index :
                           DAM_IndexToPtr(args->map, type, index));
        // \todo Use DMU's SetProperty for this, save code duplication.
        return SetProperty2(ptr, context);
    }
}

static int ReadAndCallback(int dataType, uint startIndex,
                           const byte *buffer, void *context,
                           int (*callback)(int type, uint index, void* ctx))
{
    uint        idx;
    uint        i, k;
    damlumpreadargs_t  *largs = (damlumpreadargs_t*) context;
    const readprop_t *prop;

    damsetargs_t args;
    angle_t     tmpangle;
    boolean     tmpboolean;
    byte        tmpbyte;
    fixed_t     tmpfixed;
    float       tmpfloat;
    short       tmpshort;
    int         tmpint;
    uint        tmpuint;
    long        tmplong;
    unsigned long tmpulong;
    void       *tmpptr;

    for(i = 0, idx = startIndex + i; i < largs->elements; ++i)
    {
        for(prop = &largs->props[k = 0]; k < largs->numProps;
            prop = &largs->props[++k])
        {
            initArgs(&args, dataType, prop->id);
            args.map = largs->map;
            args.valueType = prop->valueType;

            args.angleValues = &tmpangle;
            args.booleanValues = &tmpboolean;
            args.byteValues = &tmpbyte;
            args.fixedValues = &tmpfixed;
            args.floatValues = &tmpfloat;
            args.shortValues = &tmpshort;
            args.intValues = &tmpint;
            args.uintValues = &tmpuint;
            args.longValues = &tmplong;
            args.ulongValues = &tmpulong;
            args.ptrValues = &tmpptr;

            // Read the value(s) from the src buffer and insert into them
            // in a damsetargs_t
            ReadValue(prop->valueType, idx, prop->size,
                      buffer + prop->offset, &args, 0, prop->flags);
            if(!callback(dataType, idx, &args))
                return false;
        }
        buffer += largs->elmsize;
        ++idx;
    }

    return true;
}
