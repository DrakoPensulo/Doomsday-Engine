/**\file
 *\section License
 * License: Raven
 * Online License Link: http://www.dengine.net/raven_license/End_User_License_Hexen_Source_Code.html
 *
 *\author Copyright © 2003-2007 Jaakko Keränen <jaakko.keranen@iki.fi>
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

// HEADER FILES ------------------------------------------------------------

#include <string.h>
#include <stdlib.h>
#include "jhexen.h"

// MACROS ------------------------------------------------------------------

#define MAX_STRING_SIZE 64
#define ASCII_COMMENT (';')
#define ASCII_QUOTE (34)
#define LUMP_SCRIPT 1
#define FILE_ZONE_SCRIPT 2
#define FILE_CLIB_SCRIPT 3

// TYPES -------------------------------------------------------------------

// EXTERNAL FUNCTION PROTOTYPES --------------------------------------------

// PUBLIC FUNCTION PROTOTYPES ----------------------------------------------

// PRIVATE FUNCTION PROTOTYPES ---------------------------------------------

static void CheckOpen(void);
static void OpenScript(char *name, int type);

// EXTERNAL DATA DECLARATIONS ----------------------------------------------

// PUBLIC DATA DEFINITIONS -------------------------------------------------

char   *sc_String;
int     sc_Number;
int     sc_Line;
boolean sc_End;
boolean sc_Crossed;
boolean sc_FileScripts = false;
char   *sc_ScriptsDir = "";

// PRIVATE DATA DEFINITIONS ------------------------------------------------

static char ScriptName[32];
static char *ScriptBuffer;
static char *ScriptPtr;
static char *ScriptEndPtr;
static char StringBuffer[MAX_STRING_SIZE];
static boolean ScriptOpen = false;
static boolean ScriptFreeCLib;  // true = de-allocate using free()
static size_t ScriptSize;
static boolean AlreadyGot = false;

// CODE --------------------------------------------------------------------

void SC_Open(char *name)
{
    char        fileName[128];

    if(sc_FileScripts == true)
    {
        sprintf(fileName, "%s%s.txt", sc_ScriptsDir, name);
        SC_OpenFile(fileName);
    }
    else
    {
        SC_OpenLump(name);
    }
}

/**
 * Loads a script (from the WAD files) and prepares it for parsing.
 */
void SC_OpenLump(char *name)
{
    OpenScript(name, LUMP_SCRIPT);
}

/**
 * Loads a script (from a file) and prepares it for parsing.  Uses the
 * zone memory allocator for memory allocation and de-allocation.
 */
void SC_OpenFile(char *name)
{
    OpenScript(name, FILE_ZONE_SCRIPT);
}

/**
 * Loads a script (from a file) and prepares it for parsing.  Uses C
 * library function calls for memory allocation and de-allocation.
 */
void SC_OpenFileCLib(char *name)
{
    OpenScript(name, FILE_CLIB_SCRIPT);
}

static void OpenScript(char *name, int type)
{
    SC_Close();
    if(type == LUMP_SCRIPT)
    {                           // Lump script
        ScriptBuffer = (char *) W_CacheLumpName(name, PU_STATIC);
        ScriptSize = W_LumpLength(W_GetNumForName(name));
        strcpy(ScriptName, name);
        ScriptFreeCLib = false; // De-allocate using gi.Z_Free()
    }
    else if(type == FILE_ZONE_SCRIPT)
    {                           // File script - zone
        ScriptSize = M_ReadFile(name, (byte **) &ScriptBuffer);
        M_ExtractFileBase(name, ScriptName);
        ScriptFreeCLib = false; // De-allocate using gi.Z_Free()
    }
    else
    {                           // File script - clib
        ScriptSize = M_ReadFileCLib(name, (byte **) &ScriptBuffer);
        M_ExtractFileBase(name, ScriptName);
        ScriptFreeCLib = true;  // De-allocate using free()
    }
    ScriptPtr = ScriptBuffer;
    ScriptEndPtr = ScriptPtr + ScriptSize;
    sc_Line = 1;
    sc_End = false;
    ScriptOpen = true;
    sc_String = StringBuffer;
    AlreadyGot = false;
}

void SC_Close(void)
{
    if(ScriptOpen)
    {
        if(ScriptFreeCLib == true)
        {
            free(ScriptBuffer);
        }
        else
        {
            Z_Free(ScriptBuffer);
        }
        ScriptOpen = false;
    }
}

boolean SC_GetString(void)
{
    char       *text;
    boolean     foundToken;

    CheckOpen();
    if(AlreadyGot)
    {
        AlreadyGot = false;
        return true;
    }
    foundToken = false;
    sc_Crossed = false;
    if(ScriptPtr >= ScriptEndPtr)
    {
        sc_End = true;
        return false;
    }
    while(foundToken == false)
    {
        while(*ScriptPtr <= 32)
        {
            if(ScriptPtr >= ScriptEndPtr)
            {
                sc_End = true;
                return false;
            }
            if(*ScriptPtr++ == '\n')
            {
                sc_Line++;
                sc_Crossed = true;
            }
        }
        if(ScriptPtr >= ScriptEndPtr)
        {
            sc_End = true;
            return false;
        }
        if(*ScriptPtr != ASCII_COMMENT)
        {                       // Found a token
            foundToken = true;
        }
        else
        {                       // Skip comment
            while(*ScriptPtr++ != '\n')
            {
                if(ScriptPtr >= ScriptEndPtr)
                {
                    sc_End = true;
                    return false;

                }
            }
            sc_Line++;
            sc_Crossed = true;
        }
    }
    text = sc_String;
    if(*ScriptPtr == ASCII_QUOTE)
    {                           // Quoted string
        ScriptPtr++;
        while(*ScriptPtr != ASCII_QUOTE)
        {
            *text++ = *ScriptPtr++;
            if(ScriptPtr == ScriptEndPtr ||
               text == &sc_String[MAX_STRING_SIZE - 1])
            {
                break;
            }
        }
        ScriptPtr++;
    }
    else
    {                           // Normal string
        while((*ScriptPtr > 32) && (*ScriptPtr != ASCII_COMMENT))
        {
            *text++ = *ScriptPtr++;
            if(ScriptPtr == ScriptEndPtr ||
               text == &sc_String[MAX_STRING_SIZE - 1])
            {
                break;
            }
        }
    }
    *text = 0;
    return true;
}

void SC_MustGetString(void)
{
    if(SC_GetString() == false)
    {
        SC_ScriptError("Missing string.");
    }
}

void SC_MustGetStringName(char *name)
{
    SC_MustGetString();
    if(SC_Compare(name) == false)
    {
        SC_ScriptError(NULL);
    }
}

boolean SC_GetNumber(void)
{
    char       *stopper;

    CheckOpen();
    if(SC_GetString())
    {
        sc_Number = strtol(sc_String, &stopper, 0);
        if(*stopper != 0)
        {
            Con_Error("SC_GetNumber: Bad numeric constant \"%s\".\n"
                      "Script %s, Line %d", sc_String, ScriptName, sc_Line);
        }
        return true;
    }
    else
    {
        return false;
    }
}

void SC_MustGetNumber(void)
{
    if(SC_GetNumber() == false)
    {
        SC_ScriptError("Missing integer.");
    }
}

/**
 * Assumes there is a valid string in sc_String.
 */
void SC_UnGet(void)
{
    AlreadyGot = true;
}

/**
 * @return              Index of the first match to sc_String from the passed
 *                      array of strings, ELSE <code>-1</code>.
 */
int SC_MatchString(char **strings)
{
    int         i;

    for(i = 0; *strings != NULL; ++i)
    {
        if(SC_Compare(*strings++))
        {
            return i;
        }
    }
    return -1;
}

int SC_MustMatchString(char **strings)
{
    int         i;

    i = SC_MatchString(strings);
    if(i == -1)
    {
        SC_ScriptError(NULL);
    }
    return i;
}

boolean SC_Compare(char *text)
{
    if(strcasecmp(text, sc_String) == 0)
    {
        return true;
    }
    return false;
}

void SC_ScriptError(char *message)
{
    if(message == NULL)
    {
        message = "Bad syntax.";
    }
    Con_Error("Script error, \"%s\" line %d: %s", ScriptName, sc_Line,
              message);
}

static void CheckOpen(void)
{
    if(ScriptOpen == false)
    {
        Con_Error("SC_ call before SC_Open().");
    }
}
