/** @file con_data.cpp
 *
 * Console databases for cvars, ccmds, aliases and known words.
 *
 * @ingroup console
 *
 * @authors Copyright &copy; 2003-2013 Jaakko Keränen <jaakko.keranen@iki.fi>
 * @authors Copyright &copy; 2005-2013 Daniel Swanson <danij@dengine.net>
 *
 * @par License
 * GPL: http://www.gnu.org/licenses/gpl.html
 *
 * <small>This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by the
 * Free Software Foundation; either version 2 of the License, or (at your
 * option) any later version. This program is distributed in the hope that it
 * will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty
 * of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU General
 * Public License for more details. You should have received a copy of the GNU
 * General Public License along with this program; if not, write to the Free
 * Software Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA
 * 02110-1301 USA</small>
 */

#define DENG_NO_API_MACROS_CONSOLE

#include "de_platform.h"
#include "de_console.h"

//#include "cbuffer.h"
#include "Games"
#include "dd_help.h"
#include "dd_main.h"
#include "m_misc.h"

#include <de/PathTree>
#include <de/memory.h>
#include <de/memoryzone.h>
#include <de/memoryblockset.h>

#include <cstdio>
#include <cstdlib>
#include <cstdarg>
#include <cctype>

// Substrings in CVar names are delimited by this character.
#define CVARDIRECTORY_DELIMITER         '-'

using namespace de;

typedef UserDataPathTree CVarDirectory;

D_CMD(HelpWhat);
D_CMD(HelpApropos);
D_CMD(ListAliases);
D_CMD(ListCmds);
D_CMD(ListVars);
#if _DEBUG
D_CMD(PrintVarStats);
#endif

static bool inited;

/// Console variable directory.
static CVarDirectory *cvarDirectory;

static ccmd_t *ccmdListHead;
/// @todo Replace with a data structure that allows for deletion of elements.
static blockset_t *ccmdBlockSet;
/// Running total of the number of uniquely-named commands.
static uint numUniqueNamedCCmds;

/// @todo Replace with a BST.
static uint numCAliases;
static calias_t **caliases;

/// The list of known words (for completion).
/// @todo Replace with a persistent self-balancing BST (Treap?)?
static knownword_t *knownWords;
static uint numKnownWords;
static dd_bool knownWordsNeedUpdate;

static ddstring_s *emptyStr;
static uri_s *emptyUri;

void Con_DataRegister()
{
    C_CMD("help",           "s",    HelpWhat);
    C_CMD("apropos",        "s",    HelpApropos);
    C_CMD("listaliases",    NULL,   ListAliases);
    C_CMD("listcmds",       NULL,   ListCmds);
    C_CMD("listvars",       NULL,   ListVars);
#ifdef DENG_DEBUG
    C_CMD("varstats",       NULL,   PrintVarStats);
#endif
}

static int markVariableUserDataFreed(CVarDirectory::Node &node, void *context)
{
    DENG_ASSERT(context);

    cvar_t *var = reinterpret_cast<cvar_t *>(node.userPointer());
    void **ptr = (void **) context;
    if(var)
    switch(CVar_Type(var))
    {
    case CVT_CHARPTR:
        if(*ptr == CV_CHARPTR(var)) var->flags &= ~CVF_CAN_FREE;
        break;
    case CVT_URIPTR:
        if(*ptr == CV_URIPTR(var)) var->flags &= ~CVF_CAN_FREE;
        break;
    default: break;
    }
    return 0; // Continue iteration.
}

static int clearVariable(CVarDirectory::Node& node, void * /*context*/)
{
    cvar_t *var = reinterpret_cast<cvar_t*>(node.userPointer());
    if(var)
    {
        // Detach our user data from this node.
        node.setUserPointer(0);

        if(CVar_Flags(var) & CVF_CAN_FREE)
        {
            void** ptr = NULL;
            switch(CVar_Type(var))
            {
            case CVT_CHARPTR:
                if(!CV_CHARPTR(var)) break;

                ptr = (void**)var->ptr;
                /// @note Multiple vars could be using the same pointer (so only free once).
                cvarDirectory->traverse(PathTree::NoBranch, NULL, CVarDirectory::no_hash, markVariableUserDataFreed, ptr);
                M_Free(*ptr); *ptr = Str_Text(emptyStr);
                break;

            case CVT_URIPTR:
                if(!CV_URIPTR(var)) break;

                ptr = (void**)var->ptr;
                /// @note Multiple vars could be using the same pointer (so only free once).
                cvarDirectory->traverse(PathTree::NoBranch, NULL, CVarDirectory::no_hash, markVariableUserDataFreed, ptr);
                Uri_Delete((uri_s *)*ptr); *ptr = emptyUri;
                break;

            default:
                LOGDEV_SCR_WARNING("Attempt to free user data for non-pointer type variable %s [%p]")
                        << Str_Text(CVar_ComposePath(var)) << var;
                break;
            }
        }
        M_Free(var);
    }
    return 0; // Continue iteration.
}

static void clearVariables(void)
{
    /// If _DEBUG we'll traverse all nodes and verify our clear logic.
#if _DEBUG
    PathTree::ComparisonFlags flags;
#else
    PathTree::ComparisonFlags flags = PathTree::NoBranch;
#endif
    if(!cvarDirectory) return;

    cvarDirectory->traverse(flags, NULL, CVarDirectory::no_hash, clearVariable);
    cvarDirectory->clear();
}

/// Construct a new variable from the specified template and add it to the database.
static cvar_t* addVariable(cvartemplate_t const& tpl)
{
    Path path(tpl.path, CVARDIRECTORY_DELIMITER);
    CVarDirectory::Node* node = &cvarDirectory->insert(path);
    cvar_t* newVar;

    DENG_ASSERT(!node->userPointer());
    if(node->userPointer())
    {
        throw Error("Con_AddVariable", "A variable with path '" + String(tpl.path) + "' is already known!");
    }

    newVar = (cvar_t*) M_Malloc(sizeof *newVar);

    newVar->flags = tpl.flags;
    newVar->type = tpl.type;
    newVar->ptr = tpl.ptr;
    newVar->min = tpl.min;
    newVar->max = tpl.max;
    newVar->notifyChanged = tpl.notifyChanged;
    newVar->directoryNode = node;
    node->setUserPointer(newVar);

    knownWordsNeedUpdate = true;
    return newVar;
}

static void clearAliases(void)
{
    if(caliases)
    {
        // Free the alias data.
        calias_t** cal = caliases;
        for(uint i = 0; i < numCAliases; ++i, ++cal)
        {
            M_Free((*cal)->name);
            M_Free((*cal)->command);
            M_Free(*cal);
        }
        M_Free(caliases);
    }
    caliases = 0;
    numCAliases = 0;
}

static void clearCommands(void)
{
    if(ccmdBlockSet)
        BlockSet_Delete(ccmdBlockSet);
    ccmdBlockSet = 0;
    ccmdListHead = 0;
    numUniqueNamedCCmds = 0;
}

static void clearKnownWords(void)
{
    if(knownWords)
        M_Free(knownWords);
    knownWords = 0;
    numKnownWords = 0;
    knownWordsNeedUpdate = false;
}

static int compareKnownWordByName(void const *a, void const *b)
{
    knownword_t const *wA = (knownword_t const *)a;
    knownword_t const *wB = (knownword_t const *)b;
    AutoStr *textA = 0, *textB = 0;

    switch(wA->type)
    {
    case WT_CALIAS:   textA = AutoStr_FromTextStd(((calias_t *)wA->data)->name); break;
    case WT_CCMD:     textA = AutoStr_FromTextStd(((ccmd_t *)wA->data)->name); break;
    case WT_CVAR:     textA = CVar_ComposePath((cvar_t *)wA->data); break;
    case WT_GAME:     textA = AutoStr_FromTextStd(reinterpret_cast<Game *>(wA->data)->identityKey().toUtf8().constData()); break;

    default:
        Con_Error("compareKnownWordByName: Invalid type %i for word A.", wA->type);
        exit(1); // Unreachable
    }

    switch(wB->type)
    {
    case WT_CALIAS:   textB = AutoStr_FromTextStd(((calias_t *)wB->data)->name); break;
    case WT_CCMD:     textB = AutoStr_FromTextStd(((ccmd_t *)wB->data)->name); break;
    case WT_CVAR:     textB = CVar_ComposePath((cvar_t *)wB->data); break;
    case WT_GAME:     textB = AutoStr_FromTextStd(reinterpret_cast<Game *>(wB->data)->identityKey().toUtf8().constData()); break;

    default:
        Con_Error("compareKnownWordByName: Invalid type %i for word B.", wB->type);
        exit(1); // Unreachable
    }

    return Str_CompareIgnoreCase(textA, Str_Text(textB));
}

static dd_bool removeFromKnownWords(knownwordtype_t type, void* data)
{
    DENG_ASSERT(VALID_KNOWNWORDTYPE(type) && data != 0);

    for(uint i = 0; i < numKnownWords; ++i)
    {
        knownword_t* word = &knownWords[i];

        if(word->type != type || word->data != data)
            continue;

        if(i != numKnownWords-1)
            memmove(knownWords + i, knownWords + i + 1, sizeof(knownword_t) * (numKnownWords - 1 - i));

        --numKnownWords;
        if(numKnownWords != 0)
        {
            knownWords = (knownword_t*) M_Realloc(knownWords, sizeof(knownword_t) * numKnownWords);
        }
        else
        {
            M_Free(knownWords); knownWords = 0;
        }
        return true;
    }
    return false;
}

typedef struct {
    uint count;
    cvartype_t type;
    dd_bool hidden;
    dd_bool ignoreHidden;
} countvariableparams_t;

static int countVariable(CVarDirectory::Node& node, void* parameters)
{
    DENG_ASSERT(parameters);

    countvariableparams_t* p = (countvariableparams_t*) parameters;
    cvar_t* var = reinterpret_cast<cvar_t*>( node.userPointer() );

    if(!(p->ignoreHidden && (var->flags & CVF_HIDE)))
    {
        if(!VALID_CVARTYPE(p->type) && !p->hidden)
        {
            if(!p->ignoreHidden || !(var->flags & CVF_HIDE))
                ++(p->count);
        }
        else if((p->hidden && (var->flags & CVF_HIDE)) ||
                (VALID_CVARTYPE(p->type) && p->type == CVar_Type(var)))
        {
            ++(p->count);
        }
    }
    return 0; // Continue iteration.
}

static int addVariableToKnownWords(CVarDirectory::Node& node, void* parameters)
{
    DENG_ASSERT(parameters);

    cvar_t* var = reinterpret_cast<cvar_t*>( node.userPointer() );
    uint* index = (uint*) parameters;
    if(var && !(var->flags & CVF_HIDE))
    {
        knownWords[*index].type = WT_CVAR;
        knownWords[*index].data = var;
        ++(*index);
    }
    return 0; // Continue iteration.
}

/**
 * Collate all known words and sort them alphabetically.
 * Commands, variables (except those hidden) and aliases are known words.
 */
static void updateKnownWords(void)
{
    if(!knownWordsNeedUpdate) return;

    // Count the number of visible console variables.
    countvariableparams_t countCVarParams;
    countCVarParams.count = 0;
    countCVarParams.type = cvartype_t(-1);
    countCVarParams.hidden = false;
    countCVarParams.ignoreHidden = true;
    if(cvarDirectory)
    {
        cvarDirectory->traverse(PathTree::NoBranch, NULL, CVarDirectory::no_hash, countVariable, &countCVarParams);
    }

    // Build the known words table.
    numKnownWords =
            numUniqueNamedCCmds +
            countCVarParams.count +
            numCAliases +
            App_Games().count();
    size_t len = sizeof(knownword_t) * numKnownWords;
    knownWords = (knownword_t*) M_Realloc(knownWords, len);
    memset(knownWords, 0, len);

    uint knownWordIdx = 0;

    // Add commands?
    /// @note ccmd list is NOT yet sorted.
    for(ccmd_t* ccmd = ccmdListHead; ccmd; ccmd = ccmd->next)
    {
        // Skip overloaded variants.
        if(ccmd->prevOverload) continue;

        knownWords[knownWordIdx].type = WT_CCMD;
        knownWords[knownWordIdx].data = ccmd;
        ++knownWordIdx;
    }

    // Add variables?
    if(0 != countCVarParams.count)
    {
        /// @note cvars are NOT sorted.
        cvarDirectory->traverse(PathTree::NoBranch, NULL, CVarDirectory::no_hash, addVariableToKnownWords, &knownWordIdx);
    }

    // Add aliases?
    if(0 != numCAliases)
    {
        /// @note caliases array is already sorted.
        calias_t** cal = caliases;
        for(uint i = 0; i < numCAliases; ++i, ++cal)
        {
            knownWords[knownWordIdx].type = WT_CALIAS;
            knownWords[knownWordIdx].data = *cal;
            ++knownWordIdx;
        }
    }

    // Add games?
    foreach(Game *game, App_Games().all())
    {
        knownWords[knownWordIdx].type = WT_GAME;
        knownWords[knownWordIdx].data = game;
        ++knownWordIdx;
    }

    // Sort it so we get nice alphabetical word completions.
    qsort(knownWords, numKnownWords, sizeof(knownword_t), compareKnownWordByName);
    knownWordsNeedUpdate = false;
}

String CVar_TypeAsText(cvar_t const *var)
{
    // Human-readable type name.
    DENG_ASSERT(var);
    switch(var->type)
    {
    case CVT_BYTE:
        return "byte";
    case CVT_CHARPTR:
        return "text";
    case CVT_FLOAT:
        return "float";
    case CVT_INT:
        return "integer";
    case CVT_NULL:
        return "null";
    case CVT_URIPTR:
        return "uri";
    default:
        DENG_ASSERT(!"Con_VarTypeAsText: Unknown variable type");
        break;
    }
    return "";
}

template <typename ValueType>
void printTypeWarning(cvar_t const *var, String const &attemptedType, ValueType value)
{
    AutoStr* path = CVar_ComposePath(var);
    LOG_SCR_WARNING("Variable %s (of type '%s') is incompatible with %s ")
            << Str_Text(path) << CVar_TypeAsText(var)
            << attemptedType << value;
}

void CVar_PrintReadOnlyWarning(cvar_t const *var)
{
    AutoStr* path = CVar_ComposePath(var);
    LOG_SCR_WARNING("%s (%s cvar) is read-only; it cannot be changed (even with force)")
            << CVar_TypeAsText(var) << Str_Text(path);
}

ddstring_t const* CVar_TypeName(cvartype_t type)
{
    static de::Str const names[CVARTYPE_COUNT] = {
        "invalid",
        "CVT_BYTE",
        "CVT_INT",
        "CVT_FLOAT",
        "CVT_CHARPTR",
        "CVT_URIPTR"
    };
    return names[(VALID_CVARTYPE(type)? type : 0)];
}

cvartype_t CVar_Type(cvar_t const* var)
{
    DENG_ASSERT(var);
    return var->type;
}

int CVar_Flags(const cvar_t* var)
{
    DENG_ASSERT(var);
    return var->flags;
}

AutoStr *CVar_ComposePath(cvar_t const *var)
{
    DENG_ASSERT(var != 0);
    CVarDirectory::Node &node = *reinterpret_cast<CVarDirectory::Node *>(var->directoryNode);
    QByteArray path = node.path(CVARDIRECTORY_DELIMITER).toUtf8();
    return AutoStr_FromTextStd(path.constData());
}

void CVar_SetUri2(cvar_t *var, uri_s const *uri, int svFlags)
{
    DENG_ASSERT(var);

    uri_s *newUri;
    bool changed = false;

    if((var->flags & CVF_READ_ONLY) && !(svFlags & SVF_WRITE_OVERRIDE))
    {
        CVar_PrintReadOnlyWarning(var);
        return;
    }

    if(var->type != CVT_URIPTR)
    {
        Con_Error("CVar::SetUri: Not of type %s.", Str_Text(CVar_TypeName(CVT_URIPTR)));
        return; // Unreachable.
    }

    if(!CV_URIPTR(var) && !uri)
    {
        return;
    }

    // Compose the new uri.
    newUri = Uri_Dup(uri);

    if(!CV_URIPTR(var) || !Uri_Equality(CV_URIPTR(var), newUri))
    {
        changed = true;
    }

    // Free the old uri, if one exists.
    if((var->flags & CVF_CAN_FREE) && CV_URIPTR(var))
    {
        Uri_Delete(CV_URIPTR(var));
    }

    var->flags |= CVF_CAN_FREE;
    CV_URIPTR(var) = newUri;

    // Make the change notification callback
    if(var->notifyChanged && changed)
    {
        var->notifyChanged();
    }
}

void CVar_SetUri(cvar_t *var, uri_s const *uri)
{
    CVar_SetUri2(var, uri, 0);
}

void CVar_SetString2(cvar_t *var, char const *text, int svFlags)
{
    DENG_ASSERT(var != 0);

    bool changed = false;
    size_t oldLen, newLen;

    if((var->flags & CVF_READ_ONLY) && !(svFlags & SVF_WRITE_OVERRIDE))
    {
        CVar_PrintReadOnlyWarning(var);
        return;
    }

    if(var->type != CVT_CHARPTR)
    {
        printTypeWarning(var, "text", text);
        return;
    }

    oldLen = (!CV_CHARPTR(var)? 0 : strlen(CV_CHARPTR(var)));
    newLen = (!text           ? 0 : strlen(text));

    if(oldLen == 0 && newLen == 0)
        return;

    if(oldLen != newLen || qstricmp(text, CV_CHARPTR(var)))
        changed = true;

    // Free the old string, if one exists.
    if((var->flags & CVF_CAN_FREE) && CV_CHARPTR(var))
        free(CV_CHARPTR(var));

    // Allocate a new string.
    var->flags |= CVF_CAN_FREE;
    CV_CHARPTR(var) = (char*) M_Malloc(newLen + 1);
    qstrcpy(CV_CHARPTR(var), text);

    // Make the change notification callback
    if(var->notifyChanged != NULL && changed)
        var->notifyChanged();
}

void CVar_SetString(cvar_t* var, char const* text)
{
    CVar_SetString2(var, text, 0);
}

void CVar_SetInteger2(cvar_t* var, int value, int svFlags)
{
    DENG_ASSERT(var);

    bool changed = false;

    if((var->flags & CVF_READ_ONLY) && !(svFlags & SVF_WRITE_OVERRIDE))
    {
        CVar_PrintReadOnlyWarning(var);
        return;
    }

    switch(var->type)
    {
    case CVT_INT:
        if(CV_INT(var) != value)
            changed = true;
        CV_INT(var) = value;
        break;
    case CVT_BYTE:
        if(CV_BYTE(var) != (byte) value)
            changed = true;
        CV_BYTE(var) = (byte) value;
        break;
    case CVT_FLOAT:
        if(CV_FLOAT(var) != (float) value)
            changed = true;
        CV_FLOAT(var) = (float) value;
        break;

    default:
        printTypeWarning(var, "integer", value);
        return;
    }

    // Make a change notification callback?
    if(var->notifyChanged != 0 && changed)
        var->notifyChanged();
}

void CVar_SetInteger(cvar_t* var, int value)
{
    CVar_SetInteger2(var, value, 0);
}

void CVar_SetFloat2(cvar_t* var, float value, int svFlags)
{
    DENG_ASSERT(var);

    bool changed = false;

    LOG_AS("CVar_SetFloat2");

    if((var->flags & CVF_READ_ONLY) && !(svFlags & SVF_WRITE_OVERRIDE))
    {
        CVar_PrintReadOnlyWarning(var);
        return;
    }

    switch(var->type)
    {
    case CVT_INT:
        if(CV_INT(var) != (int) value)
            changed = true;
        CV_INT(var) = (int) value;
        break;
    case CVT_BYTE:
        if(CV_BYTE(var) != (byte) value)
            changed = true;
        CV_BYTE(var) = (byte) value;
        break;
    case CVT_FLOAT:
        if(CV_FLOAT(var) != value)
            changed = true;
        CV_FLOAT(var) = value;
        break;

    default:
        printTypeWarning(var, "float", value);
        return;
    }

    // Make a change notification callback?
    if(var->notifyChanged != 0 && changed)
        var->notifyChanged();
}

void CVar_SetFloat(cvar_t* var, float value)
{
    CVar_SetFloat2(var, value, 0);
}

static void printConversionWarning(cvar_t const *var)
{
    AutoStr* path = CVar_ComposePath(var);
    LOGDEV_SCR_WARNING("Incompatible variable %s [%p type:%s]")
            << Str_Text(path) << var << Str_Text(CVar_TypeName(CVar_Type(var)));
}

int CVar_Integer(cvar_t const* var)
{
    DENG_ASSERT(var);
    switch(var->type)
    {
    case CVT_BYTE:      return CV_BYTE(var);
    case CVT_INT:       return CV_INT(var);
    case CVT_FLOAT:     return CV_FLOAT(var);
    case CVT_CHARPTR:   return strtol(CV_CHARPTR(var), 0, 0);
    default: {
        LOG_AS("CVar_Integer");
        printConversionWarning(var);
        return 0; }
    }
}

float CVar_Float(cvar_t const* var)
{
    DENG_ASSERT(var);
    switch(var->type)
    {
    case CVT_BYTE:      return CV_BYTE(var);
    case CVT_INT:       return CV_INT(var);
    case CVT_FLOAT:     return CV_FLOAT(var);
    case CVT_CHARPTR:   return strtod(CV_CHARPTR(var), 0);
    default: {
        LOG_AS("CVar_Float");
        printConversionWarning(var);
        return 0; }
    }
}

byte CVar_Byte(cvar_t const* var)
{
    DENG_ASSERT(var);
    switch(var->type)
    {
    case CVT_BYTE:      return CV_BYTE(var);
    case CVT_INT:       return CV_INT(var);
    case CVT_FLOAT:     return CV_FLOAT(var);
    case CVT_CHARPTR:   return strtol(CV_CHARPTR(var), 0, 0);
    default: {
        LOG_AS("CVar_Byte");
        printConversionWarning(var);
        return 0; }
    }
}

char const* CVar_String(cvar_t const* var)
{
    DENG_ASSERT(var);
    /// @todo Why not implement in-place value to string conversion?
    switch(var->type)
    {
    case CVT_CHARPTR:   return CV_CHARPTR(var);
    default: {
        LOG_AS("CVar_String");
        printConversionWarning(var);
        return Str_Text(emptyStr); }
    }
}

uri_s const *CVar_Uri(cvar_t const *var)
{
    DENG_ASSERT(var != 0);
    /// @todo Why not implement in-place string to uri conversion?
    switch(var->type)
    {
    case CVT_URIPTR:   return CV_URIPTR(var);
    default: {
        LOG_AS("CVar_Uri");
        printConversionWarning(var);
        return emptyUri; }
    }
}

#undef Con_AddVariable
void Con_AddVariable(cvartemplate_t const *tpl)
{
    LOG_AS("Con_AddVariable");

    DENG_ASSERT(inited);
    if(!tpl) return;

    if(CVT_NULL == tpl->type)
    {
        LOGDEV_SCR_WARNING("Ignored attempt to register variable '%s' as type %s")
            << tpl->path << Str_Text(CVar_TypeName(CVT_NULL));
        return;
    }

    addVariable(*tpl);
}

#undef Con_AddVariableList
void Con_AddVariableList(cvartemplate_t const *tplList)
{
    DENG_ASSERT(inited);
    if(!tplList) return;

    for(; tplList->path; ++tplList)
    {
        if(Con_FindVariable(tplList->path))
            Con_Error("A CVAR with the name '%s' is already registered.", tplList->path);

        addVariable(*tplList);
    }
}

cvar_t *Con_FindVariable(char const *path)
{
    DENG_ASSERT(inited);
    if(!path || !path[0]) return 0;

    try
    {
        CVarDirectory::Node const &node = cvarDirectory->find(Path(path, CVARDIRECTORY_DELIMITER),
                                                              PathTree::NoBranch | PathTree::MatchFull);
        return (cvar_t*) node.userPointer();
    }
    catch(CVarDirectory::NotFoundError const&)
    {} // Ignore this error.
    return 0;
}

#undef Con_GetVariableType
cvartype_t Con_GetVariableType(char const *path)
{
    cvar_t *var = Con_FindVariable(path);
    if(!var) return CVT_NULL;
    return var->type;
}

String Con_VarAsStyledText(cvar_t *var, char const *prefix)
{
    DENG_ASSERT(inited);

    if(!var) return "";

    char equals = '=';
    if((var->flags & CVF_PROTECTED) || (var->flags & CVF_READ_ONLY))
        equals = ':';

    String str;
    QTextStream os(&str);

    if(prefix) os << prefix;

    AutoStr* path = CVar_ComposePath(var);

    os << _E(b) << Str_Text(path) << _E(.) << " " << equals << " " << _E(>);

    switch(var->type)
    {
    case CVT_BYTE:      os << CV_BYTE(var); break;
    case CVT_INT:       os << CV_INT(var); break;
    case CVT_FLOAT:     os << CV_FLOAT(var); break;
    case CVT_CHARPTR:   os << "\"" << CV_CHARPTR(var) << "\""; break;
    case CVT_URIPTR: {
        AutoStr* valPath = (CV_URIPTR(var)? Uri_ToString(CV_URIPTR(var)) : NULL);
        os << "\"" << (CV_URIPTR(var)? Str_Text(valPath) : "") << "\"";
        break; }
    default:
        DENG_ASSERT(false);
        break;
    }
    os << _E(<);
    return str;
}

void Con_PrintCVar(cvar_t* var, char const *prefix)
{
    LOG_SCR_MSG("%s") << Con_VarAsStyledText(var, prefix);
}

void Con_AddCommand(ccmdtemplate_t const* ccmd)
{
    DENG_ASSERT(inited);

    int minArgs, maxArgs;
    cvartype_t args[MAX_ARGS];
    ccmd_t* newCCmd, *overloaded = 0;

    if(!ccmd) return;

    DENG_ASSERT(ccmd->name != 0);

    // Decode the usage string if present.
    if(ccmd->argTemplate != 0)
    {
        size_t l, len;
        cvartype_t type = CVT_NULL;
        dd_bool unlimitedArgs;
        char c;

        len = strlen(ccmd->argTemplate);
        minArgs = 0;
        unlimitedArgs = false;
        for(l = 0; l < len; ++l)
        {
            c = ccmd->argTemplate[l];
            switch(c)
            {
            // Supported type symbols:
            case 'b': type = CVT_BYTE;     break;
            case 'i': type = CVT_INT;      break;
            case 'f': type = CVT_FLOAT;    break;
            case 's': type = CVT_CHARPTR;  break;

            // Special symbols:
            case '*':
                // Variable arg list.
                if(l != len-1)
                    Con_Error("Con_AddCommand: CCmd '%s': '*' character "
                              "not last in argument template: \"%s\".",
                              ccmd->name, ccmd->argTemplate);

                unlimitedArgs = true;
                type = CVT_NULL; // Not a real argument.
                break;

            // Erroneous symbol:
            default:
                Con_Error("Con_AddCommand: CCmd '%s': Invalid character "
                          "'%c' in argument template: \"%s\".", ccmd->name, c,
                          ccmd->argTemplate);
            }

            if(type != CVT_NULL)
            {
                if(minArgs >= MAX_ARGS)
                    Con_Error("Con_AddCommand: CCmd '%s': Too many arguments. "
                              "Limit is %i.", ccmd->name, MAX_ARGS);

                args[minArgs++] = type;
            }
        }

        // Set the min/max parameter counts for this ccmd.
        if(unlimitedArgs)
        {
            maxArgs = -1;
            if(minArgs == 0)
                minArgs = -1;
        }
        else
        {
            maxArgs = minArgs;
        }
    }
    else // It's usage is NOT validated by Doomsday.
    {
        minArgs = maxArgs = -1;
    }

    // Now check that the ccmd to be registered is unique.
    // We allow multiple ccmds with the same name if we can determine by
    // their paramater lists that they are unique (overloading).
    { ccmd_t* other;
    if((other = Con_FindCommand(ccmd->name)) != 0)
    {
        dd_bool unique = true;

        // The ccmd being registered is NOT a deng validated ccmd
        // and there is already an existing ccmd by this name?
        if(minArgs == -1 && maxArgs == -1)
            unique = false;

        if(unique)
        {
            // Check each variant.
            ccmd_t* variant = other;
            do
            {
                // An existing ccmd with no validation?
                if(variant->minArgs == -1 && variant->maxArgs == -1)
                    unique = false;
                // An existing ccmd with a lower minimum and no maximum?
                else if(variant->minArgs < minArgs && variant->maxArgs == -1)
                    unique = false;
                // An existing ccmd with a larger min and this ccmd has no max?
                else if(variant->minArgs > minArgs && maxArgs == -1)
                    unique = false;
                // An existing ccmd with the same minimum number of args?
                else if(variant->minArgs == minArgs)
                {
                    // \todo Implement support for paramater type checking.
                    unique = false;
                }

                // Sanity check.
                if(!unique && variant->execFunc == ccmd->execFunc)
                    Con_Error("Con_AddCommand: A CCmd by the name '%s' is already registered and the callback funcs are "
                              "the same, is this really what you wanted?", ccmd->name);
            } while((variant = variant->nextOverload) != 0);
        }

        if(!unique)
            Con_Error("Con_AddCommand: A CCmd by the name '%s' is already registered. Their parameter lists would be ambiguant.", ccmd->name);

        overloaded = other;
    }}

    if(!ccmdBlockSet)
        ccmdBlockSet = BlockSet_New(sizeof(ccmd_t), 32);

    newCCmd = (ccmd_t*) BlockSet_Allocate(ccmdBlockSet);

    // Make a static copy of the name in the zone (this allows the source
    // data to change in case of dynamic registrations).
    char* nameCopy = (char*) Z_Malloc(strlen(ccmd->name) + 1, PU_APPSTATIC, NULL);
    if(!nameCopy) Con_Error("Con_AddCommand: Failed on allocation of %lu bytes for command name.", (unsigned long) (strlen(ccmd->name) + 1));

    strcpy(nameCopy, ccmd->name);
    newCCmd->name = nameCopy;
    newCCmd->execFunc = ccmd->execFunc;
    newCCmd->flags = ccmd->flags;
    newCCmd->nextOverload = newCCmd->prevOverload = 0;
    newCCmd->minArgs = minArgs;
    newCCmd->maxArgs = maxArgs;
    memcpy(newCCmd->args, &args, sizeof(newCCmd->args));

    // Link it to the head of the global list of ccmds.
    newCCmd->next = ccmdListHead;
    ccmdListHead = newCCmd;

    if(!overloaded)
    {
        ++numUniqueNamedCCmds;
        knownWordsNeedUpdate = true;
        return;
    }

    // Link it to the head of the overload list.
    newCCmd->nextOverload = overloaded;
    overloaded->prevOverload = newCCmd;
}

void Con_AddCommandList(ccmdtemplate_t const* cmdList)
{
    if(!cmdList) return;
    for(; cmdList->name; ++cmdList)
    {
        Con_AddCommand(cmdList);
    }
}

ccmd_t *Con_FindCommand(char const *name)
{
    DENG_ASSERT(inited);
    /// @todo Use a faster than O(n) linear search.
    if(name && name[0])
    {
        for(ccmd_t *ccmd = ccmdListHead; ccmd; ccmd = ccmd->next)
        {
            if(qstricmp(name, ccmd->name)) continue;

            // Locate the head of the overload list.
            while(ccmd->prevOverload) { ccmd = ccmd->prevOverload; }
            return ccmd;
        }
    }
    return 0;
}

/**
 * Outputs the usage information for the given ccmd to the console.
 *
 * @param ccmd          The ccmd to print the usage info for.
 * @param allOverloads  @c true= print usage info for all overloaded variants.
 *                      Otherwise only the info for @a ccmd.
 */
static void printCommandUsage(ccmd_t *ccmd, bool allOverloads = true)
{
    DENG_ASSERT(inited);
    if(!ccmd) return;

    if(allOverloads)
    {
        // Locate the head of the overload list.
        while(ccmd->prevOverload) { ccmd = ccmd->prevOverload; }
    }

    LOG_SCR_NOTE(_E(b) "Usage:" _E(.) "\n  " _E(>) + Con_CmdUsageAsStyledText(ccmd));

    if(allOverloads)
    {
        while((ccmd = ccmd->nextOverload))
        {
            LOG_SCR_MSG("  " _E(>) + Con_CmdUsageAsStyledText(ccmd));
        }
    }
}

ccmd_t *Con_FindCommandMatchArgs(cmdargs_t *args)
{
    DENG_ASSERT(inited);

    if(!args) return 0;

    if(ccmd_t *ccmd = Con_FindCommand(args->argv[0]))
    {
        // Check each variant.
        ccmd_t *variant = ccmd;
        do
        {
            dd_bool invalidArgs = false;

            // Are we validating the arguments?
            // Note that strings are considered always valid.
            if(!(variant->minArgs == -1 && variant->maxArgs == -1))
            {
                // Do we have the right number of arguments?
                if(args->argc-1 < variant->minArgs)
                {
                    invalidArgs = true;
                }
                else if(variant->maxArgs != -1 && args->argc-1 > variant->maxArgs)
                {
                    invalidArgs = true;
                }
                else
                {
                    // Presently we only validate upto the minimum number of args.
                    /// @todo Validate non-required args.
                    for(int i = 0; i < variant->minArgs && !invalidArgs; ++i)
                    {
                        switch(variant->args[i])
                        {
                        case CVT_BYTE:
                            invalidArgs = !M_IsStringValidByte(args->argv[i+1]);
                            break;
                        case CVT_INT:
                            invalidArgs = !M_IsStringValidInt(args->argv[i+1]);
                            break;
                        case CVT_FLOAT:
                            invalidArgs = !M_IsStringValidFloat(args->argv[i+1]);
                            break;
                        default:
                            break;
                        }
                    }
                }
            }

            if(!invalidArgs)
            {
                return variant; // This is the one!
            }
        } while((variant = variant->nextOverload) != 0);

        // Perhaps the user needs some help.
        printCommandUsage(ccmd);
    }

    // No command found, or none with matching arguments.
    return 0;
}

dd_bool Con_IsValidCommand(char const* name)
{
    if(!name || !name[0])
        return false;

    // Try the console commands first.
    if(Con_FindCommand(name) != 0)
        return true;

    // Try the aliases (aliai?) then.
    return (Con_FindAlias(name) != 0);
}

String Con_CmdUsageAsStyledText(ccmd_t *ccmd)
{
    DENG2_ASSERT(ccmd != 0);

    if(ccmd->minArgs == -1 && ccmd->maxArgs == -1)
        return String();

    // Print the expected form for this ccmd.
    String argText;
    for(int i = 0; i < ccmd->minArgs; ++i)
    {
        switch(ccmd->args[i])
        {
        case CVT_BYTE:    argText += " (byte)";   break;
        case CVT_INT:     argText += " (int)";    break;
        case CVT_FLOAT:   argText += " (float)";  break;
        case CVT_CHARPTR: argText += " (string)"; break;

        default: break;
        }
    }
    if(ccmd->maxArgs == -1)
    {
        argText += " ...";
    }

    return _E(b) + String(ccmd->name) + _E(.) _E(l) + argText + _E(.);
}

calias_t *Con_FindAlias(char const *name)
{
    DENG_ASSERT(inited);

    uint bottomIdx, topIdx, pivot;
    calias_t* cal;
    dd_bool isDone;
    int result;

    if(numCAliases == 0) return 0;
    if(!name || !name[0]) return 0;

    bottomIdx = 0;
    topIdx = numCAliases-1;
    cal = NULL;
    isDone = false;
    while(bottomIdx <= topIdx && !isDone)
    {
        pivot = bottomIdx + (topIdx - bottomIdx)/2;

        result = stricmp(caliases[pivot]->name, name);
        if(result == 0)
        {
            // Found.
            cal = caliases[pivot];
            isDone = true;
        }
        else
        {
            if(result > 0)
            {
                if(pivot == 0)
                {
                    // Not present.
                    isDone = true;
                }
                else
                    topIdx = pivot - 1;
            }
            else
                bottomIdx = pivot + 1;
        }
    }

    return cal;
}

calias_t* Con_AddAlias(char const* name, char const* command)
{
    DENG_ASSERT(inited);

    if(!name || !name[0] || !command || !command[0]) return 0;

    caliases = (calias_t**) M_Realloc(caliases, sizeof(*caliases) * ++numCAliases);

    // Find the insertion point.
    uint idx;
    for(idx = 0; idx < numCAliases-1; ++idx)
    {
        if(qstricmp(caliases[idx]->name, name) > 0)
            break;
    }

    // Make room for the new alias.
    if(idx != numCAliases-1)
        memmove(caliases + idx + 1, caliases + idx, sizeof(*caliases) * (numCAliases - 1 - idx));

    // Add the new variable, making a static copy of the name in the zone (this allows
    // the source data to change in case of dynamic registrations).
    calias_t* newAlias = caliases[idx] = (calias_t*) M_Malloc(sizeof(*newAlias));
    newAlias->name = (char*) M_Malloc(strlen(name) + 1);
    strcpy(newAlias->name, name);
    newAlias->command = (char*) M_Malloc(strlen(command) + 1);
    strcpy(newAlias->command, command);

    knownWordsNeedUpdate = true;
    return newAlias;
}

void Con_DeleteAlias(calias_t* cal)
{
    DENG_ASSERT(inited && cal);

    uint idx;
    for(idx = 0; idx < numCAliases; ++idx)
    {
        if(caliases[idx] == cal)
            break;
    }
    if(idx == numCAliases) return;

    // Try to avoid rebuilding known words by simply removing ourself.
    if(!knownWordsNeedUpdate)
        removeFromKnownWords(WT_CALIAS, (void*)cal);

    M_Free(cal->name);
    M_Free(cal->command);
    M_Free(cal);

    if(idx < numCAliases - 1)
    {
        memmove(caliases + idx, caliases + idx + 1, sizeof(*caliases) * (numCAliases - idx - 1));
    }
    --numCAliases;
}

/**
 * @return New AutoStr with the text of the known word. Caller gets ownership.
 */
static AutoStr *textForKnownWord(knownword_t const *word)
{
    AutoStr *text = 0;

    switch(word->type)
    {
    case WT_CALIAS:   text = AutoStr_FromTextStd(((calias_t *)word->data)->name); break;
    case WT_CCMD:     text = AutoStr_FromTextStd(((ccmd_t *)word->data)->name); break;
    case WT_CVAR:     text = CVar_ComposePath((cvar_t *)word->data); break;
    case WT_GAME:     text = AutoStr_FromTextStd(reinterpret_cast<Game *>(word->data)->identityKey().toUtf8().constData()); break;

    default:
        Con_Error("textForKnownWord: Invalid type %i for word.", word->type);
        exit(1); // Unreachable
    }

    return text;
}

AutoStr *Con_KnownWordToString(knownword_t const *word)
{
    return textForKnownWord(word);
}

int Con_IterateKnownWords(char const *pattern, knownwordtype_t type,
                          int (*callback)(knownword_t const *word, void *parameters),
                          void *parameters)
{
    return Con_IterateKnownWords(KnownWordStartsWith, pattern, type, callback, parameters);
}

int Con_IterateKnownWords(KnownWordMatchMode matchMode,
                          char const* pattern, knownwordtype_t type,
                          int (*callback)(knownword_t const* word, void* parameters),
                          void* parameters)
{
    DENG_ASSERT(inited && callback);

    knownwordtype_t matchType = (VALID_KNOWNWORDTYPE(type)? type : WT_ANY);
    size_t patternLength = (pattern? strlen(pattern) : 0);
    int result = 0;

    updateKnownWords();

    for(uint i = 0; i < numKnownWords; ++i)
    {
        knownword_t const* word = &knownWords[i];
        if(matchType != WT_ANY && word->type != type) continue;

        if(patternLength)
        {
            AutoStr* textString = textForKnownWord(word);
            if(matchMode == KnownWordStartsWith)
            {
                if(strnicmp(Str_Text(textString), pattern, patternLength))
                    continue; // Didn't match.
            }
            else if(matchMode == KnownWordExactMatch)
            {
                if(strcasecmp(Str_Text(textString), pattern))
                    continue; // Didn't match.
            }
        }

        result = callback(word, parameters);
        if(result) break;
    }

    return result;
}

static int countMatchedWordWorker(knownword_t const* /*word*/, void* parameters)
{
    DENG_ASSERT(parameters);
    uint* count = (uint*) parameters;
    ++(*count);
    return 0; // Continue iteration.
}

typedef struct {
    knownword_t const** matches; /// Matched word array.
    uint index; /// Current position in the collection.
} collectmatchedwordworker_paramaters_t;

static int collectMatchedWordWorker(knownword_t const* word, void* parameters)
{
    DENG_ASSERT(word && parameters);
    collectmatchedwordworker_paramaters_t* p = (collectmatchedwordworker_paramaters_t*) parameters;
    p->matches[p->index++] = word;
    return 0; // Continue iteration.
}

knownword_t const** Con_CollectKnownWordsMatchingWord(char const* word,
    knownwordtype_t type, uint* count)
{
    DENG_ASSERT(inited);

    uint localCount = 0;
    Con_IterateKnownWords(word, type, countMatchedWordWorker, &localCount);

    if(count) *count = localCount;

    if(localCount != 0)
    {
        // Collect the pointers.
        collectmatchedwordworker_paramaters_t p;
        p.matches = (knownword_t const**) M_Malloc(sizeof(*p.matches) * (localCount + 1));
        p.index = 0;

        Con_IterateKnownWords(word, type, collectMatchedWordWorker, &p);
        p.matches[localCount] = 0; // Terminate.

        return p.matches;
    }

    return 0; // No matches.
}

void Con_InitDatabases(void)
{
    if(inited) return;

    // Create the empty variable directory now.
    cvarDirectory = new CVarDirectory();

    ccmdListHead = 0;
    ccmdBlockSet = 0;
    numUniqueNamedCCmds = 0;

    numCAliases = 0;
    caliases = 0;

    knownWords = 0;
    numKnownWords = 0;
    knownWordsNeedUpdate = false;

    emptyStr = Str_NewStd();
    emptyUri = Uri_New();

    inited = true;
}

void Con_ClearDatabases(void)
{
    if(!inited) return;
    clearKnownWords();
    clearAliases();
    clearCommands();
    clearVariables();
}

void Con_ShutdownDatabases(void)
{
    if(!inited) return;

    Con_ClearDatabases();
    delete cvarDirectory; cvarDirectory = 0;
    Str_Delete(emptyStr); emptyStr = 0;
    Uri_Delete(emptyUri); emptyUri = 0;
    inited = false;
}

static int aproposPrinter(knownword_t const *word, void *matching)
{
    AutoStr *text = textForKnownWord(word);

    // See if 'matching' is anywhere in the known word.
    if(strcasestr(Str_Text(text), (const char*)matching))
    {
        char const* wType[KNOWNWORDTYPE_COUNT] = {
            "cmd ", "var ", "alias ", "game "
        };

        String str;
        QTextStream os(&str);

        os << _E(l) << wType[word->type]
           << _E(0) << _E(b) << Str_Text(text) << " " << _E(2) << _E(>);

        // Look for a short description.
        String tmp;
        if(word->type == WT_CCMD || word->type == WT_CVAR)
        {
            char const *desc = DH_GetString(DH_Find(Str_Text(text)), HST_DESCRIPTION);
            if(desc)
            {
                tmp = desc;
            }
        }
        else if(word->type == WT_GAME)
        {
            tmp = reinterpret_cast<Game *>(word->data)->title();
        }

        os << tmp;

        LOG_SCR_MSG("%s") << str;
    }

    return 0;
}

static void printApropos(char const *matching)
{
    /// @todo  Extend the search to cover the contents of all help strings (dd_help.c).
    Con_IterateKnownWords(0, WT_ANY, aproposPrinter, (void *)matching);
}

static void printHelpAbout(char const *query)
{
    // Try the console commands first.
    if(ccmd_t *ccmd = Con_FindCommand(query))
    {
        LOG_SCR_MSG(_E(b) "%s" _E(.) " (Command)") << ccmd->name;

        HelpId help = DH_Find(ccmd->name);
        if(char const *description = DH_GetString(help, HST_DESCRIPTION))
        {
            LOG_SCR_MSG("") << description;
        }

        printCommandUsage(ccmd); // For all overloaded variants.

        // Any extra info?
        if(char const *info = DH_GetString(help, HST_INFO))
        {
            LOG_SCR_MSG("  " _E(>) _E(l)) << info;
        }
        return;
    }

    if(cvar_t *var = Con_FindVariable(query))
    {
        AutoStr *path = CVar_ComposePath(var);
        LOG_SCR_MSG(_E(b) "%s" _E(.) " (Variable)") << Str_Text(path);

        HelpId help = DH_Find(Str_Text(path));
        if(char const *description = DH_GetString(help, HST_DESCRIPTION))
        {
            LOG_SCR_MSG("") << description;
        }
        return;
    }

    if(calias_t *calias = Con_FindAlias(query))
    {
        LOG_SCR_MSG(_E(b) "%s" _E(.) " alias of:\n")
                << calias->name << calias->command;
        return;
    }

    // Perhaps a game?
    try
    {
        Game &game = App_Games().byIdentityKey(query);
        LOG_SCR_MSG(_E(b) "%s" _E(.) " (IdentityKey)") << game.identityKey();

        LOG_SCR_MSG("Unique identifier of the " _E(b) "%s" _E(.) " game mode.") << game.title();
        LOG_SCR_MSG("An 'IdentityKey' is used when referencing a game unambiguously from the console and on the command line.");
        LOG_SCR_MSG(_E(D) "Related commands:");
        LOG_SCR_MSG("  " _E(>) "Enter " _E(b) "inspectgame %s" _E(.) " for information and status of this game") << game.identityKey();
        LOG_SCR_MSG("  " _E(>) "Enter " _E(b) "listgames" _E(.) " to list all installed games and their status");
        LOG_SCR_MSG("  " _E(>) "Enter " _E(b) "load %s" _E(.) " to load the " _E(l) "%s" _E(.) " game mode") << game.identityKey() << game.title();
        return;
    }
    catch(Games::NotFoundError const &)
    {} // Ignore this error.

    LOG_SCR_NOTE("There is no help about '%s'") << query;
}

D_CMD(HelpApropos)
{
    DENG2_UNUSED2(argc, src);

    printApropos(argv[1]);
    return true;
}

D_CMD(HelpWhat)
{
    DENG2_UNUSED2(argc, src);

    if(!String(argv[1]).compareWithoutCase("(what)"))
    {
        LOG_SCR_MSG("You've got to be kidding!");
        return true;
    }

    printHelpAbout(argv[1]);
    return true;
}

String Con_CmdAsStyledText(ccmd_t *cmd)
{
    char const *str;
    if((str = DH_GetString(DH_Find(cmd->name), HST_DESCRIPTION)))
    {
        return String(_E(b) "%1 " _E(.) _E(>) _E(2) "%2" _E(.) _E(<)).arg(cmd->name).arg(str);
    }
    else
    {
        return String(_E(b) "%1" _E(.)).arg(cmd->name);
    }
}

String Con_AliasAsStyledText(calias_t *alias)
{
    QString str;
    QTextStream os(&str);

    os << _E(b) << alias->name << _E(.) " == " _E(>) << alias->command << _E(<);

    return str;
}

String Con_GameAsStyledText(Game *game)
{
    DENG2_ASSERT(game != 0);
    return String(_E(1)) + game->identityKey() + _E(.);
}

static int printKnownWordWorker(knownword_t const *word, void *parameters)
{
    DENG_ASSERT(word);
    uint *numPrinted = (uint *) parameters;

    switch(word->type)
    {
    case WT_CCMD: {
        ccmd_t *ccmd = (ccmd_t *) word->data;
        if(ccmd->prevOverload)
        {
            return 0; // Skip overloaded variants.
        }
        LOG_SCR_MSG("%s") << Con_CmdAsStyledText(ccmd);
        break; }

    case WT_CVAR: {
        cvar_t *cvar = (cvar_t *) word->data;
        if(cvar->flags & CVF_HIDE)
        {
            return 0; // Skip hidden variables.
        }
        Con_PrintCVar(cvar, "");
        break; }

    case WT_CALIAS:
        LOG_SCR_MSG("%s") << Con_AliasAsStyledText((calias_t *) word->data);
        break;

    case WT_GAME:
        LOG_SCR_MSG("%s") << Con_GameAsStyledText((Game *) word->data);
        break;

    default:
        DENG_ASSERT(false);
        break;
    }

    if(numPrinted) ++(*numPrinted);
    return 0; // Continue iteration.
}

#undef Con_SetUri2
void Con_SetUri2(char const *path, uri_s const *uri, int svFlags)
{
    cvar_t* var = Con_FindVariable(path);
    if(!var) return;
    CVar_SetUri2(var, uri, svFlags);
}

#undef Con_SetUri
void Con_SetUri(char const *path, uri_s const *uri)
{
    Con_SetUri2(path, uri, 0);
}

#undef Con_SetString2
void Con_SetString2(char const *path, char const *text, int svFlags)
{
    cvar_t *var = Con_FindVariable(path);
    if(!var) return;
    CVar_SetString2(var, text, svFlags);
}

#undef Con_SetString
void Con_SetString(char const *path, char const *text)
{
    Con_SetString2(path, text, 0);
}

#undef Con_SetInteger2
void Con_SetInteger2(char const *path, int value, int svFlags)
{
    cvar_t *var = Con_FindVariable(path);
    if(!var) return;
    CVar_SetInteger2(var, value, svFlags);
}

#undef Con_SetInteger
void Con_SetInteger(char const *path, int value)
{
    Con_SetInteger2(path, value, 0);
}

#undef Con_SetFloat2
void Con_SetFloat2(char const *path, float value, int svFlags)
{
    cvar_t *var = Con_FindVariable(path);
    if(!var) return;
    CVar_SetFloat2(var, value, svFlags);
}

#undef Con_SetFloat
void Con_SetFloat(char const *path, float value)
{
    Con_SetFloat2(path, value, 0);
}

#undef Con_GetInteger
int Con_GetInteger(char const *path)
{
    cvar_t *var = Con_FindVariable(path);
    if(!var) return 0;
    return CVar_Integer(var);
}

#undef Con_GetFloat
float Con_GetFloat(char const *path)
{
    cvar_t *var = Con_FindVariable(path);
    if(!var) return 0;
    return CVar_Float(var);
}

#undef Con_GetByte
byte Con_GetByte(char const *path)
{
    cvar_t *var = Con_FindVariable(path);
    if(!var) return 0;
    return CVar_Byte(var);
}

#undef Con_GetString
char const *Con_GetString(char const *path)
{
    cvar_t *var = Con_FindVariable(path);
    if(!var) return Str_Text(emptyStr);
    return CVar_String(var);
}

#undef Con_GetUri
uri_s const *Con_GetUri(char const *path)
{
    cvar_t *var = Con_FindVariable(path);
    if(!var) return emptyUri;
    return CVar_Uri(var);
}

D_CMD(ListCmds)
{
    DENG_UNUSED(src);

    LOG_SCR_MSG(_E(b) "Console commands:");
    uint numPrinted = 0;
    Con_IterateKnownWords(argc > 1? argv[1] : 0, WT_CCMD, printKnownWordWorker, &numPrinted);
    LOG_SCR_MSG("Found %i console commands") << numPrinted;
    return true;
}

D_CMD(ListVars)
{
    DENG_UNUSED(src);

    uint numPrinted = 0;
    LOG_SCR_MSG(_E(b) "Console variables:");
    Con_IterateKnownWords(argc > 1? argv[1] : 0, WT_CVAR, printKnownWordWorker, &numPrinted);
    LOG_SCR_MSG("Found %i console variables") << numPrinted;
    return true;
}

#ifdef DENG_DEBUG
D_CMD(PrintVarStats)
{
    DENG2_UNUSED3(src, argc, argv);

    uint numCVars = 0, numCVarsHidden = 0;

    LOG_SCR_MSG(_E(b) "Console Variable Statistics:");
    if(cvarDirectory)
    {
        countvariableparams_t p;
        p.hidden = false;
        p.ignoreHidden = false;
        for(uint i = uint(CVT_BYTE); i < uint(CVARTYPE_COUNT); ++i)
        {
            p.count = 0;
            p.type = cvartype_t(i);
            cvarDirectory->traverse(PathTree::NoBranch, NULL, CVarDirectory::no_hash, countVariable, &p);
            LOGDEV_SCR_MSG("%12s: %i") << Str_Text(CVar_TypeName(p.type)) << p.count;
        }
        p.count = 0;
        p.type = cvartype_t(-1);
        p.hidden = true;

        cvarDirectory->traverse(PathTree::NoBranch, NULL, CVarDirectory::no_hash, countVariable, &p);
        numCVars = cvarDirectory->size();
        numCVarsHidden = p.count;
    }
    LOG_SCR_MSG("       Total: %i\n      Hidden: %i") << numCVars << numCVarsHidden;

    if(cvarDirectory)
    {
        cvarDirectory->debugPrintHashDistribution();
        cvarDirectory->debugPrint(CVARDIRECTORY_DELIMITER);
    }
    return true;
}
#endif

D_CMD(ListAliases)
{
    DENG_UNUSED(src);

    LOG_SCR_MSG(_E(b) "Aliases:");
    uint numPrinted = 0;
    Con_IterateKnownWords(argc > 1? argv[1] : 0, WT_CALIAS, printKnownWordWorker, &numPrinted);
    LOG_SCR_MSG("Found %i aliases") << numPrinted;
    return true;
}