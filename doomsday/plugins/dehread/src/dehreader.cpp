/** @file dehreader.cpp  DeHackEd patch reader plugin for Doomsday Engine.
 * @ingroup dehread
 *
 * @todo Presently there are a number of unsupported features which should not
 *       be ignored. (Most if not all features should be supported.)
 *
 * @author Copyright © 2006-2014 Daniel Swanson <danij@dengine.net>
 * @author Copyright © 2003-2013 Jaakko Keränen <jaakko.keranen@iki.fi>
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

#include "dehread.h"

#include <QDebug>
#include <QDir>
#include <QFile>
#include <QRegExp>
#include <QStringList>

#include <doomsday/filesys/lumpindex.h>
#include <de/App>
#include <de/Block>
#include <de/Error>
#include <de/Log>
#include <de/String>
#include <de/memory.h>

#include "dehreader.h"
#include "dehreader_util.h"
#include "info.h"

using namespace de;

static int stackDepth;
static const int maxIncludeDepth = de::max(0, DEHREADER_INCLUDE_DEPTH_MAX);

/// Mask containing only those reader flags which should be passed from the current
/// parser to any child parsers for file include statements.
static DehReaderFlag const DehReaderFlagsIncludeMask = IgnoreEOF;

/**
 * Not exposed outside this source file; use readDehPatch() instead.
 */
class DehReader
{
    /// The parser encountered a syntax error in the source file. @ingroup errors
    DENG2_ERROR(SyntaxError);
    /// The parser encountered an unknown section in the source file. @ingroup errors
    DENG2_ERROR(UnknownSection);
    /// The parser reached the end of the source file. @ingroup errors
    DENG2_ERROR(EndOfFile);

    Block const &patch;
    int pos;
    int currentLineNumber;

    DehReaderFlags flags;

    int patchVersion;
    int doomVersion;

    String line; ///< Current line.

public:
    DehReader(Block const &_patch, DehReaderFlags _flags = 0)
        : patch(_patch)
        , pos(0)
        , currentLineNumber(0)
        , flags(_flags)
        , patchVersion(-1) // unknown
        , doomVersion(-1) // unknown
        , line("")
    {
        stackDepth++;
    }

    ~DehReader()
    {
        stackDepth--;
    }

#if 0
    LogEntry &log(Log::LogLevel level, String const &format)
    {
        return LOG().enter(level, format);
    }
#endif

    /**
     * Doom version numbers in the patch use the orignal game versions,
     * "16" => Doom v1.6, "19" => Doom v1.9, etc...
     */
    static inline bool normalizeDoomVersion(int &ver)
    {
        switch(ver)
        {
        case 16: ver = 0; return true;
        case 17: ver = 2; return true;
        case 19: ver = 3; return true;
        case 20: ver = 1; return true;
        case 21: ver = 4; return true;
        default: return false; // What is this??
        }
    }

    bool atRealEnd()
    {
        return size_t(pos) >= patch.size();
    }

    bool atEnd()
    {
        if(atRealEnd()) return true;
        if(!(flags & IgnoreEOF) && patch.at(pos) == '\0') return true;
        return false;
    }

    void advance()
    {
        if(atEnd()) return;

        // Handle special characters in the input.
        char ch = currentChar().toLatin1();
        switch(ch)
        {
        case '\0':
            if(size_t(pos) != patch.size() - 1)
            {
                LOG_WARNING("Unexpected EOF encountered on line #%i, ignoring.") << currentLineNumber;
            }
            break;
        case '\n':
            currentLineNumber++;
            break;
        default: break;
        }

        pos++;
    }

    QChar currentChar()
    {
        if(atEnd()) return 0;
        return QChar::fromLatin1(patch.at(pos));
    }

    void skipToEOL()
    {
        while(!atEnd() && currentChar() != '\n') advance();
    }

    void readLine()
    {
        int start = pos;
        skipToEOL();
        if(!atEnd())
        {
            int endOfLine = pos - start;
            // Ignore any trailing carriage return.
            if(endOfLine > 0 && patch.at(start + endOfLine - 1) == '\r') endOfLine -= 1;

            QByteArray rawLine = patch.mid(start, endOfLine);

            // When tolerating mid stream EOF characters, we must first
            // strip them before attempting any encoding conversion.
            if(flags & IgnoreEOF)
            {
                rawLine.replace('\0', "");
            }

            // Perform encoding conversion for this line and move on.
            line = QString::fromLatin1(rawLine);
            if(currentChar() == '\n') advance();
            return;
        }
        throw EndOfFile(String("EOF on line #%1").arg(currentLineNumber));
    }

    /**
     * Keep reading lines until we find one that is something other than
     * whitespace or a whole-line comment.
     */
    String const &skipToNextLine()
    {
        forever
        {
            readLine();
            if(!line.trimmed().isEmpty() && line.at(0) != '#') break;
        }
        return line;
    }

    bool lineInCurrentSection()
    {
        return line.indexOf('=') != -1;
    }

    void skipToNextSection()
    {
        do skipToNextLine();
        while(lineInCurrentSection());
    }

    void logPatchInfo()
    {
        // Log reader settings and patch version information.
        LOG_RES_MSG("DeHackEd patch version: %i, Doom version: %i\nNoText: %b")
            << patchVersion << doomVersion << bool(flags & NoText);

        if(patchVersion != 6)
        {
            LOG_WARNING("Unknown DeHackEd patch version, unexpected results may occur") << patchVersion;
        }
    }

    void parse()
    {
        LOG_AS_STRING(stackDepth == 1? "DehReader" : QString("[%1]").arg(stackDepth - 1));

        skipToNextLine();

        // Attempt to parse the DeHackEd patch signature and version numbers.
        if(line.beginsWith("Patch File for DeHackEd v", Qt::CaseInsensitive))
        {
            skipToNextLine();
            parsePatchSignature();
        }
        else
        {
            LOG_WARNING("Patch is missing a signature, assuming BEX.");
            doomVersion  = 19;
            patchVersion = 6;
        }

        logPatchInfo();

        // Is this for a known Doom version?
        if(!normalizeDoomVersion(doomVersion))
        {
            LOG_WARNING("Unknown Doom version, assuming v1.9.");
            doomVersion = 3;
        }

        // Patches are subdivided into sections.
        try
        {
            forever
            {
                try
                {
                    /// @note Some sections have their own grammar quirks!
                    if(line.beginsWith("include", Qt::CaseInsensitive)) // BEX
                    {
                        parseInclude(line.substr(7).leftStrip());
                        skipToNextSection();
                    }
                    else if(line.beginsWith("Thing", Qt::CaseInsensitive))
                    {
                        String const arg           = line.substr(5).leftStrip();
                        int mobjType               = 0;
                        bool const isKnownMobjType = parseMobjType(arg, &mobjType);
                        bool const ignore          = !isKnownMobjType;

                        if(!isKnownMobjType)
                        {
                            LOG_WARNING("Thing '%s' out of range, ignoring. (Create more Thing defs.)") << arg;
                        }

                        skipToNextLine();
                        parseThing(&ded->mobjs[mobjType], ignore);
                    }
                    else if(line.beginsWith("Frame", Qt::CaseInsensitive))
                    {
                        String const arg           = line.substr(5).leftStrip();
                        int stateNum               = 0;
                        bool const isKnownStateNum = parseStateNum(arg, &stateNum);
                        bool const ignore          = !isKnownStateNum;

                        if(!isKnownStateNum)
                        {
                            LOG_WARNING("Frame '%s' out of range, ignoring. (Create more State defs.)") << arg;
                        }

                        skipToNextLine();
                        parseFrame(&ded->states[stateNum], ignore);
                    }
                    else if(line.beginsWith("Pointer", Qt::CaseInsensitive))
                    {
                        String const arg           = line.substr(7).leftStrip();
                        int stateNum               = 0;
                        bool const isKnownStateNum = parseStateNumFromActionOffset(arg, &stateNum);
                        bool const ignore          = !isKnownStateNum;

                        if(!isKnownStateNum)
                        {
                            LOG_WARNING("Pointer '%s' out of range, ignoring. (Create more State defs.)") << arg;
                        }

                        skipToNextLine();
                        parsePointer(&ded->states[stateNum], ignore);
                    }
                    else if(line.beginsWith("Sprite", Qt::CaseInsensitive))
                    {
                        String const arg            = line.substr(6).leftStrip();
                        int spriteNum               = 0;
                        bool const isKnownSpriteNum = parseSpriteNum(arg, &spriteNum);
                        bool const ignore           = !isKnownSpriteNum;

                        if(!isKnownSpriteNum)
                        {
                            LOG_WARNING("Sprite '%s' out of range, ignoring. (Create more Sprite defs.)") << arg;
                        }

                        skipToNextLine();
                        parseSprite(&ded->sprites[spriteNum], ignore);
                    }
                    else if(line.beginsWith("Ammo", Qt::CaseInsensitive))
                    {
                        String const arg          = line.substr(4).leftStrip();
                        int ammoNum               = 0;
                        bool const isKnownAmmoNum = parseAmmoNum(arg, &ammoNum);
                        bool const ignore         = !isKnownAmmoNum;

                        if(!isKnownAmmoNum)
                        {
                            LOG_WARNING("Ammo '%s' out of range, ignoring.") << arg;
                        }

                        skipToNextLine();
                        parseAmmo(ammoNum, ignore);
                    }
                    else if(line.beginsWith("Weapon", Qt::CaseInsensitive))
                    {
                        String const arg            = line.substr(6).leftStrip();
                        int weaponNum               = 0;
                        bool const isKnownWeaponNum = parseWeaponNum(arg, &weaponNum);
                        bool const ignore           = !isKnownWeaponNum;

                        if(!isKnownWeaponNum)
                        {
                            LOG_WARNING("Weapon '%s' out of range, ignoring.") << arg;
                        }

                        skipToNextLine();
                        parseWeapon(weaponNum, ignore);
                    }
                    else if(line.beginsWith("Sound", Qt::CaseInsensitive))
                    {
                        String const arg           = line.substr(5).leftStrip();
                        int soundNum               = 0;
                        bool const isKnownSoundNum = parseSoundNum(arg, &soundNum);
                        bool const ignore          = !isKnownSoundNum;

                        if(!isKnownSoundNum)
                        {
                            LOG_WARNING("Sound '%s' out of range, ignoring. (Create more Sound defs.)") << arg;
                        }

                        skipToNextLine();
                        parseSound(&ded->sounds[soundNum], ignore);
                    }
                    else if(line.beginsWith("Text", Qt::CaseInsensitive))
                    {
                        String args = line.substr(4).leftStrip();
                        int firstArgEnd = args.indexOf(' ');
                        if(firstArgEnd < 0)
                        {
                            throw SyntaxError(String("Expected old text size on line #%1")
                                                .arg(currentLineNumber));
                        }

                        bool isNumber;
                        int const oldSize = args.toInt(&isNumber, 10, String::AllowSuffix);
                        if(!isNumber)
                        {
                            throw SyntaxError(String("Expected old text size but encountered \"%1\" on line #%2")
                                                .arg(args.substr(firstArgEnd)).arg(currentLineNumber));
                        }

                        args.remove(0, firstArgEnd + 1);

                        int const newSize = args.toInt(&isNumber, 10, String::AllowSuffix);
                        if(!isNumber)
                        {
                            throw SyntaxError(String("Expected new text size but encountered \"%1\" on line #%2")
                                                .arg(args).arg(currentLineNumber));
                        }

                        parseText(oldSize, newSize);
                    }
                    else if(line.beginsWith("Misc", Qt::CaseInsensitive))
                    {
                        skipToNextLine();
                        parseMisc();
                    }
                    else if(line.beginsWith("Cheat", Qt::CaseInsensitive))
                    {
                        LOG_WARNING("[Cheat] patches are not supported.");
                        skipToNextSection();
                    }
                    else if(line.beginsWith("[CODEPTR]", Qt::CaseInsensitive)) // BEX
                    {
                        skipToNextLine();
                        parseCodePointers();
                    }
                    else if(line.beginsWith("[PARS]", Qt::CaseInsensitive)) // BEX
                    {
                        skipToNextLine();
                        parsePars();
                    }
                    else if(line.beginsWith("[STRINGS]", Qt::CaseInsensitive)) // BEX
                    {
                        skipToNextLine();
                        parseStrings();
                    }
                    else if(line.beginsWith("[HELPER]", Qt::CaseInsensitive)) // BEX
                    {
                        // Not yet supported (Helper Dogs from MBF).
                        //skipToNextLine();
                        parseHelper();
                        skipToNextSection();
                    }
                    else if(line.beginsWith("[SPRITES]", Qt::CaseInsensitive)) // BEX
                    {
                        // Not yet supported.
                        //skipToNextLine();
                        parseSprites();
                        skipToNextSection();
                    }
                    else if(line.beginsWith("[SOUNDS]", Qt::CaseInsensitive)) // BEX
                    {
                        skipToNextLine();
                        parseSounds();
                    }
                    else if(line.beginsWith("[MUSIC]", Qt::CaseInsensitive)) // BEX
                    {
                        skipToNextLine();
                        parseMusic();
                    }
                    else
                    {
                        // An unknown section.
                        throw UnknownSection(String("Expected section name but encountered \"%1\" on line #%2")
                                                 .arg(line).arg(currentLineNumber));
                    }
                }
                catch(UnknownSection const &er)
                {
                    LOG_WARNING("%s. Skipping section...") << er.asText();
                    skipToNextSection();
                }
            }
        }
        catch(EndOfFile const & /*er*/)
        {} // Ignore.
    }

    void parseAssignmentStatement(String const &line, String &var, String &expr)
    {
        // Determine the split (or 'pivot') position.
        int assign = line.indexOf('=');
        if(assign < 0)
        {
            throw SyntaxError("parseAssignmentStatement",
                              String("Expected assignment statement but encountered \"%1\" on line #%2")
                                .arg(line).arg(currentLineNumber));
        }

        var  = line.substr(0, assign).rightStrip();
        expr = line.substr(assign + 1).leftStrip();

        // Basic grammar checking.
        // Nothing before '=' ?
        if(var.isEmpty())
        {
            throw SyntaxError("parseAssignmentStatement",
                              String("Expected keyword before '=' on line #%1").arg(currentLineNumber));
        }

        // Nothing after '=' ?
        if(expr.isEmpty())
        {
            throw SyntaxError("parseAssignmentStatement",
                              String("Expected expression after '=' on line #%1").arg(currentLineNumber));
        }
    }

    bool parseAmmoNum(String const &str, int *ammoNum)
    {
        int result = str.toInt(0, 0, String::AllowSuffix);
        if(ammoNum) *ammoNum = result;
        return (result >= 0 && result < 4);
    }

    bool parseMobjType(String const &str, int *mobjType)
    {
        int result = str.toInt(0, 0, String::AllowSuffix) - 1; // Patch indices are 1-based.
        if(mobjType) *mobjType = result;
        return (result >= 0 && result < ded->mobjs.size());
    }

    bool parseSoundNum(String const &str, int *soundNum)
    {
        int result = str.toInt(0, 0, String::AllowSuffix);
        if(soundNum) *soundNum = result;
        return (result >= 0 && result < ded->sounds.size());
    }

    bool parseSpriteNum(String const &str, int *spriteNum)
    {
        int result = str.toInt(0, 0, String::AllowSuffix);
        if(spriteNum) *spriteNum = result;
        return (result >= 0 && result < NUMSPRITES);
    }

    bool parseStateNum(String const &str, int *stateNum)
    {
        int result = str.toInt(0, 0, String::AllowSuffix);
        if(stateNum) *stateNum = result;
        return (result >= 0 && result < ded->states.size());
    }

    bool parseStateNumFromActionOffset(String const &str, int *stateNum)
    {
        int result = stateIndexForActionOffset(str.toInt(0, 0, String::AllowSuffix));
        if(stateNum) *stateNum = result;
        return (result >= 0 && result < ded->states.size());
    }

    bool parseWeaponNum(String const &str, int *weaponNum)
    {
        int result = str.toInt(0, 0, String::AllowSuffix);
        if(weaponNum) *weaponNum = result;
        return (result >= 0);
    }

    bool parseMobjTypeState(QString const &token, StateMapping const **state)
    {
        return findStateMappingByDehLabel(token, state) >= 0;
    }

    bool parseMobjTypeFlag(QString const &token, FlagMapping const **flag)
    {
        return findMobjTypeFlagMappingByDehLabel(token, flag) >= 0;
    }

    bool parseMobjTypeSound(QString const &token, SoundMapping const **sound)
    {
        return findSoundMappingByDehLabel(token, sound) >= 0;
    }

    bool parseWeaponState(QString const &token, WeaponStateMapping const **state)
    {
        return findWeaponStateMappingByDehLabel(token, state) >= 0;
    }

    bool parseMiscValue(QString const &token, ValueMapping const **value)
    {
        return findValueMappingForDehLabel(token, value) >= 0;
    }

    void parsePatchSignature()
    {
        for(; lineInCurrentSection(); skipToNextLine())
        {
            String var, expr;
            parseAssignmentStatement(line, var, expr);

            if(!var.compareWithoutCase("Doom version"))
            {
                doomVersion = expr.toInt(0, 10, String::AllowSuffix);
            }
            else if(!var.compareWithoutCase("Patch format"))
            {
                patchVersion = expr.toInt(0, 10, String::AllowSuffix);
            }
            else if(!var.compareWithoutCase("Engine config") ||
                    !var.compareWithoutCase("IWAD"))
            {
                // Ignore these WhackEd2 specific values.
            }
            else
            {
                LOG_WARNING("Unexpected symbol \"%s\" encountered on line #%i, ignoring.")
                        << var << currentLineNumber;
            }
        }
    }

    void parseInclude(QString arg)
    {
        if(flags & NoInclude)
        {
            LOG_AS("parseInclude");
            LOG_DEBUG("Skipping disabled Include directive.");
            return;
        }

        if(stackDepth > maxIncludeDepth)
        {
            LOG_AS("parseInclude");
            if(!maxIncludeDepth)
            {
                LOG_WARNING("Sorry, nested includes are not supported. Directive ignored.");
            }
            else
            {
                char const *includes = (maxIncludeDepth == 1? "include" : "includes");
                LOG_WARNING("Sorry, there can be at most %i nested %s. Directive ignored.")
                        << maxIncludeDepth << includes;
            }
        }
        else
        {
            DehReaderFlags includeFlags = flags & DehReaderFlagsIncludeMask;

            if(arg.startsWith("notext ", Qt::CaseInsensitive))
            {
                includeFlags |= NoText;
                arg.remove(0, 7);
            }

            if(!arg.isEmpty())
            {
                NativePath const filePath(arg);
                QFile file(filePath);
                if(!file.open(QFile::ReadOnly | QFile::Text))
                {
                    LOG_AS("parseInclude");
                    LOG_RES_WARNING("Failed opening \"%s\" for read, aborting...") << filePath;
                }
                else
                {
                    /// @todo Do not use a local buffer.
                    Block deh = file.readAll();
                    deh.append(QChar(0));
                    file.close();

                    LOG_RES_VERBOSE("Including \"%s\"...") << filePath.pretty();

                    try
                    {
                        DehReader(deh, includeFlags).parse();
                    }
                    catch(Error const &er)
                    {
                        LOG_WARNING(er.asText() + ".");
                    }
                }
            }
            else
            {
                LOG_AS("parseInclude");
                LOG_RES_WARNING("DeHackEd Include directive missing filename");
            }
        }
    }

    String readTextBlob(int size)
    {
        if(!size) return String(); // Return an empty string.

        String string;
        do
        {
            // Ignore carriage returns.
            QChar c = currentChar();
            if(c != '\r')
                string += c;
            else
                size++;

            advance();
        } while(--size);

        return string.trimmed();
    }

    /**
     * @todo fixme - missing translations!!!
     *
     * @param arg           Flag argument string to be parsed.
     * @param flagGroups    Flag groups to be updated.
     *
     * @return (& 0x1)= flag group #1 changed, (& 0x2)= flag group #2 changed, etc..
     */
    int parseMobjTypeFlags(QString const &arg, int flagGroups[NUM_MOBJ_FLAGS])
    {
        DENG2_ASSERT(flagGroups);

        if(arg.isEmpty()) return 0; // Erm? No change...
        int changedGroups = 0;

        // Split the argument into discreet tokens and process each individually.
        /// @todo Re-implement with a left-to-right algorithm.
        QStringList tokens = arg.split(QRegExp("[,+| ]|\t|\f|\r"), QString::SkipEmptyParts);
        DENG2_FOR_EACH_CONST(QStringList, i, tokens)
        {
            String const &token = *i;
            bool tokenIsNumber;

            int flagsValue = token.toInt(&tokenIsNumber, 10, String::AllowSuffix);
            if(tokenIsNumber)
            {
                // Force the top 4 bits to 0 so that the user is forced to use
                // the mnemonics to change them.
                /// @todo fixme - What about the other groups???
                flagGroups[0] |= (flagsValue & 0x0fffffff);

                changedGroups |= 0x1;
                continue;
            }

            // Flags can also be specified by name (a BEX extension).
            FlagMapping const *flag;
            if(parseMobjTypeFlag(token, &flag))
            {
                /// @todo fixme - Get the proper bit values from the ded def db.
                int value = 0;
                if(flag->bit & 0xff00)
                    value |= 1 << (flag->bit >> 8);
                value |= 1 << (flag->bit & 0xff);

                // Apply the new value.
                DENG2_ASSERT(flag->group >= 0 && flag->group < NUM_MOBJ_FLAGS);
                flagGroups[flag->group] |= value;

                changedGroups |= 1 << flag->group;
                continue;
            }

            LOG_WARNING("Unknown flag mnemonic '%s' on line #%i, ignoring.")
                    << token << currentLineNumber;
        }

        return changedGroups;
    }

    void parseThing(ded_mobj_t *mobj, bool ignore = false)
    {
        int const mobjType = ded->mobjs.indexOf(mobj);
        bool hadHeight     = false, checkHeight = false;

        LOG_AS("parseThing");
        for(; lineInCurrentSection(); skipToNextLine())
        {
            String var, expr;
            parseAssignmentStatement(line, var, expr);

            if(var.endsWith(" frame", Qt::CaseInsensitive))
            {
                StateMapping const *mapping;
                String const dehStateName = var.left(var.size() - 6);
                if(!parseMobjTypeState(dehStateName, &mapping))
                {
                    if(!ignore)
                        LOG_WARNING("Unknown frame '%s' on line #%i, ignoring.")
                                << dehStateName << currentLineNumber;
                }
                else
                {
                    int const value = expr.toInt(0, 0, String::AllowSuffix);
                    if(!ignore)
                    {
                        if(value < 0 || value >= ded->states.size())
                        {
                            LOG_WARNING("Frame #%i out of range, ignoring.") << value;
                        }
                        else
                        {
                            int const stateIdx = value;
                            ded_state_t const &state = ded->states[stateIdx];

                            DENG2_ASSERT(mapping->id >= 0 && mapping->id < STATENAMES_COUNT);
                            qstrncpy(mobj->states[mapping->id], state.id, DED_STRINGID_LEN + 1);

                            LOG_DEBUG("Type #%i \"%s\" state:%s => \"%s\" (#%i)")
                                    << mobjType << mobj->id << mapping->name
                                    << mobj->states[mapping->id] << stateIdx;
                        }
                    }
                }
            }
            else if(var.endsWith(" sound", Qt::CaseInsensitive))
            {
                SoundMapping const *mapping;
                String const dehSoundName = var.left(var.size() - 6);
                if(!parseMobjTypeSound(dehSoundName, &mapping))
                {
                    if(!ignore)
                        LOG_WARNING("Unknown sound '%s' on line #%i, ignoring.")
                                << dehSoundName << currentLineNumber;
                }
                else
                {
                    int const value = expr.toInt(0, 0, String::AllowSuffix);
                    if(!ignore)
                    {
                        if(value < 0 || value >= ded->sounds.size())
                        {
                            LOG_WARNING("Sound #%i out of range, ignoring.") << value;
                        }
                        else
                        {
                            int const soundsIdx = value;
                            void *soundAdr;
                            switch(mapping->id)
                            {
                            case SDN_PAIN:      soundAdr = &mobj->painSound;   break;
                            case SDN_DEATH:     soundAdr = &mobj->deathSound;  break;
                            case SDN_ACTIVE:    soundAdr = &mobj->activeSound; break;
                            case SDN_ATTACK:    soundAdr = &mobj->attackSound; break;
                            case SDN_SEE:       soundAdr = &mobj->seeSound;    break;
                            default:
                                throw Error("DehReader", String("Unknown soundname id %i").arg(mapping->id));
                            }

                            ded_sound_t const &sound = ded->sounds[soundsIdx];
                            qstrncpy((char *)soundAdr, sound.id, DED_STRINGID_LEN + 1);

                            LOG_DEBUG("Type #%i \"%s\" sound:%s => \"%s\" (#%i)")
                                    << mobjType << mobj->id << mapping->name
                                    << (char *)soundAdr << soundsIdx;
                        }
                    }
                }
            }
            else if(!var.compareWithoutCase("Bits"))
            {
                int flags[NUM_MOBJ_FLAGS] = { 0, 0, 0 };
                int changedFlagGroups = parseMobjTypeFlags(expr, flags);
                if(!ignore)
                {
                    // Apply the new flags.
                    for(uint k = 0; k < NUM_MOBJ_FLAGS; ++k)
                    {
                        if(!(changedFlagGroups & (1 << k))) continue;

                        mobj->flags[k] = flags[k];
                        LOG_DEBUG("Type #%i \"%s\" flags:%i => %X (%i)")
                                << mobjType << mobj->id << k
                                << mobj->flags[k] << mobj->flags[k];
                    }

                    // Any special translation necessary?
                    if(changedFlagGroups & 0x1)
                    {
                        if(mobj->flags[0] & 0x100/*mf_spawnceiling*/)
                            checkHeight = true;

                        // Bit flags are no longer used to specify translucency.
                        // This is just a temporary hack.
                        /*if(info->flags[0] & 0x60000000)
                            info->translucency = (info->flags[0] & 0x60000000) >> 15; */
                    }
                }
            }
            else if(!var.compareWithoutCase("Bits2")) // Eternity
            {
                /// @todo Support this extension.
                LOG_WARNING("Thing - \"Bits2\" patches are not supported.");
            }
            else if(!var.compareWithoutCase("Bits3")) // Eternity
            {
                /// @todo Support this extension.
                LOG_WARNING("Thing - \"Bits3\" patches are not supported.");
            }
            else if(!var.compareWithoutCase("Blood color")) // Eternity
            {
                /**
                 * Red (normal)        0
                 * Grey                1
                 * Green               2
                 * Blue                3
                 * Yellow              4
                 * Black               5
                 * Purple              6
                 * White               7
                 * Orange              8
                 */
                /// @todo Support this extension.
                LOG_WARNING("Thing - \"Blood color\" patches are not supported.");
            }
            else if(!var.compareWithoutCase("ID #"))
            {
                int const value = expr.toInt(0, 10, String::AllowSuffix);
                if(!ignore)
                {
                    mobj->doomEdNum = value;
                    LOG_DEBUG("Type #%i \"%s\" doomEdNum => %i") << mobjType << mobj->id << mobj->doomEdNum;
                }
            }
            else if(!var.compareWithoutCase("Height"))
            {
                int const value = expr.toInt(0, 10, String::AllowSuffix);
                if(!ignore)
                {
                    mobj->height = value / float(0x10000);
                    hadHeight = true;
                    LOG_DEBUG("Type #%i \"%s\" height => %f") << mobjType << mobj->id << mobj->height;
                }
            }
            else if(!var.compareWithoutCase("Hit points"))
            {
                int const value = expr.toInt(0, 10, String::AllowSuffix);
                if(!ignore)
                {
                    mobj->spawnHealth = value;
                    LOG_DEBUG("Type #%i \"%s\" spawnHealth => %i") << mobjType << mobj->id << mobj->spawnHealth;
                }
            }
            else if(!var.compareWithoutCase("Mass"))
            {
                int const value = expr.toInt(0, 10, String::AllowSuffix);
                if(!ignore)
                {
                    mobj->mass = value;
                    LOG_DEBUG("Type #%i \"%s\" mass => %i") << mobjType << mobj->id << mobj->mass;
                }
            }
            else if(!var.compareWithoutCase("Missile damage"))
            {
                int const value = expr.toInt(0, 10, String::AllowSuffix);
                if(!ignore)
                {
                    mobj->damage = value;
                    LOG_DEBUG("Type #%i \"%s\" damage => %i") << mobjType << mobj->id << mobj->damage;
                }
            }
            else if(!var.compareWithoutCase("Pain chance"))
            {
                int const value = expr.toInt(0, 10, String::AllowSuffix);
                if(!ignore)
                {
                    mobj->painChance = value;
                    LOG_DEBUG("Type #%i \"%s\" painChance => %i") << mobjType << mobj->id << mobj->painChance;
                }
            }
            else if(!var.compareWithoutCase("Reaction time"))
            {
                int const value = expr.toInt(0, 10, String::AllowSuffix);
                if(!ignore)
                {
                    mobj->reactionTime = value;
                    LOG_DEBUG("Type #%i \"%s\" reactionTime => %i") << mobjType << mobj->id << mobj->reactionTime;
                }
            }
            else if(!var.compareWithoutCase("Speed"))
            {
                int const value = expr.toInt(0, 10, String::AllowSuffix);
                if(!ignore)
                {
                    /// @todo Is this right??
                    mobj->speed = (abs(value) < 256 ? float(value) : FIX2FLT(value));
                    LOG_DEBUG("Type #%i \"%s\" speed => %f") << mobjType << mobj->id << mobj->speed;
                }
            }
            else if(!var.compareWithoutCase("Translucency")) // Eternity
            {
                //int const value = expr.toInt(0, 10, String::AllowSuffix);
                //float const opacity = de::clamp(0, value, 65536) / 65536.f;
                /// @todo Support this extension.
                LOG_WARNING("Thing - \"Translucency\" patches are not supported.");
            }
            else if(!var.compareWithoutCase("Width"))
            {
                int const value = expr.toInt(0, 10, String::AllowSuffix);
                if(!ignore)
                {
                    mobj->radius = value / float(0x10000);
                    LOG_DEBUG("Type #%i \"%s\" radius => %f") << mobjType << mobj->id << mobj->radius;
                }
            }
            else
            {
                LOG_WARNING("Unexpected symbol \"%s\" encountered on line #%i, ignoring.")
                        << var << currentLineNumber;
            }
        }

        /// @todo Does this still make sense given DED can change the values?
        if(checkHeight && !hadHeight)
        {
            mobj->height = originalHeightForMobjType(mobjType);
        }
    }

    void parseFrame(ded_state_t *state, bool ignore = false)
    {
        int const stateNum = ded->states.indexOf(state);
        LOG_AS("parseFrame");
        for(; lineInCurrentSection(); skipToNextLine())
        {
            String var, expr;
            parseAssignmentStatement(line, var, expr);

            if(!var.compareWithoutCase("Duration"))
            {
                int const value = expr.toInt(0, 10, String::AllowSuffix);
                if(!ignore)
                {
                    state->tics = value;
                    LOG_DEBUG("State #%i \"%s\" tics => %i") << stateNum << state->id << state->tics;
                }
            }
            else if(!var.compareWithoutCase("Next frame"))
            {
                int const value = expr.toInt(0, 0, String::AllowSuffix);
                if(!ignore)
                {
                    if(value < 0 || value >= ded->states.size())
                    {
                        LOG_WARNING("Frame #%i out of range, ignoring.") << value;
                    }
                    else
                    {
                        int const nextStateIdx = value;
                        qstrncpy(state->nextState, ded->states[nextStateIdx].id, DED_STRINGID_LEN + 1);
                        LOG_DEBUG("State #%i \"%s\" nextState => \"%s\" (#%i)")
                                << stateNum << state->id << state->nextState << nextStateIdx;
                    }
                }
            }
            else if(!var.compareWithoutCase("Particle event")) // Eternity
            {
                /// @todo Support this extension.
                LOG_WARNING("Frame - \"Particle event\" patches are not supported.");
            }
            else if(!var.compareWithoutCase("Sprite number"))
            {
                int const value = expr.toInt(0, 10, String::AllowSuffix);
                if(!ignore)
                {
                    if(value < 0 || value > ded->sprites.size())
                    {
                        LOG_WARNING("Sprite #%i out of range, ignoring.") << value;
                    }
                    else
                    {
                        int const spriteIdx = value;
                        ded_sprid_t const &sprite = ded->sprites[spriteIdx];
                        qstrncpy(state->sprite.id, sprite.id, DED_SPRITEID_LEN + 1);
                        LOG_DEBUG("State #%i \"%s\" sprite => \"%s\" (#%i)")
                                << stateNum << state->id << state->sprite.id << spriteIdx;
                    }
                }
            }
            else if(!var.compareWithoutCase("Sprite subnumber"))
            {
                int const value = expr.toInt(0, 10, String::AllowSuffix);
                if(!ignore)
                {
                    int const FF_FULLBRIGHT = 0x8000;

                    // Translate the old fullbright bit.
                    if(value & FF_FULLBRIGHT)   state->flags |=  STF_FULLBRIGHT;
                    else                        state->flags &= ~STF_FULLBRIGHT;
                    state->frame = value & ~FF_FULLBRIGHT;

                    LOG_DEBUG("State #%i \"%s\" frame => %i") << stateNum << state->id << state->frame;
                }
            }
            else if(var.startsWith("Unknown ", Qt::CaseInsensitive))
            {
                int const miscIdx = var.substr(8).toInt(0, 10, String::AllowSuffix);
                int const value   = expr.toInt(0, 10, String::AllowSuffix);
                if(!ignore)
                {
                    if(miscIdx < 0 || miscIdx >= NUM_STATE_MISC)
                    {
                        LOG_WARNING("Unknown unknown-value '%s' on line #%i, ignoring.")
                                << var.mid(8) << currentLineNumber;
                    }
                    else
                    {
                        state->misc[miscIdx] = value;
                        LOG_DEBUG("State #%i \"%s\" misc:%i => %i")
                                << stateNum << state->id << miscIdx << value;
                    }
                }
            }
            else if(var.beginsWith("Args", Qt::CaseInsensitive)) // Eternity
            {
                LOG_WARNING("Frame - \"%s\" patches are not supported.") << var;
            }
            else
            {
                LOG_WARNING("Unknown symbol \"%s\" encountered on line #%i, ignoring.")
                        << var << currentLineNumber;
            }
        }
    }

    void parseSprite(ded_sprid_t *sprite, bool ignore = false)
    {
        int const sprNum = ded->sprites.indexOf(sprite);
        LOG_AS("parseSprite");
        for(; lineInCurrentSection(); skipToNextLine())
        {
            String var, expr;
            parseAssignmentStatement(line, var, expr);

            if(!var.compareWithoutCase("Offset"))
            {
                int const value = expr.toInt(0, 0, String::AllowSuffix);
                if(!ignore)
                {
                    // Calculate offset from beginning of sprite names.
                    int offset = -1;
                    if(value > 0)
                    {
                        // From DeHackEd source.
                        DENG2_ASSERT(doomVersion >= 0 && doomVersion < 5);
                        static int const spriteNameTableOffset[] = { 129044, 129044, 129044, 129284, 129380 };
                        offset = (value - spriteNameTableOffset[doomVersion] - 22044) / 8;
                    }

                    if(offset < 0 || offset >= ded->sprites.size())
                    {
                        LOG_WARNING("Sprite offset #%i out of range, ignoring.") << value;
                    }
                    else
                    {
                        ded_sprid_t const &origSprite = origSpriteNames[offset];
                        qstrncpy(sprite->id, origSprite.id, DED_STRINGID_LEN + 1);
                        LOG_DEBUG("Sprite #%i id => \"%s\" (#%i)") << sprNum << sprite->id << offset;
                    }
                }
            }
            else
            {
                LOG_WARNING("Unexpected symbol \"%s\" encountered on line #%i, ignoring.")
                        << var << currentLineNumber;
            }
        }
    }

    void parseSound(ded_sound_t *sound, bool ignore = false)
    {
        int const soundIdx = ded->sounds.indexOf(sound);
        LOG_AS("parseSound");
        for(; lineInCurrentSection(); skipToNextLine())
        {
            String var, expr;
            parseAssignmentStatement(line, var, expr);

            if(!var.compareWithoutCase("Offset")) // sound->id
            {
                LOG_WARNING("Sound - \"Offset\" patches are not supported.");
            }
            else if(!var.compareWithoutCase("Zero/One"))
            {
                int const value = expr.toInt(0, 10, String::AllowSuffix);
                if(!ignore)
                {
                    sound->group = value;
                    LOG_DEBUG("Sound #%i \"%s\" group => %i")
                            << soundIdx << sound->id << sound->group;
                }
            }
            else if(!var.compareWithoutCase("Value"))
            {
                int const value = expr.toInt(0, 10, String::AllowSuffix);
                if(!ignore)
                {
                    sound->priority = value;
                    LOG_DEBUG("Sound #%i \"%s\" priority => %i")
                            << soundIdx << sound->id << sound->priority;
                }
            }
            else if(!var.compareWithoutCase("Zero 1")) // sound->link
            {
                LOG_WARNING("Sound - \"Zero 1\" patches are not supported.");
            }
            else if(!var.compareWithoutCase("Zero 2"))
            {
                int const value = expr.toInt(0, 10, String::AllowSuffix);
                if(!ignore)
                {
                    sound->linkPitch = value;
                    LOG_DEBUG("Sound #%i \"%s\" linkPitch => %i")
                            << soundIdx << sound->id << sound->linkPitch;
                }
            }
            else if(!var.compareWithoutCase("Zero 3"))
            {
                int const value = expr.toInt(0, 10, String::AllowSuffix);
                if(!ignore)
                {
                    sound->linkVolume = value;
                    LOG_DEBUG("Sound #%i \"%s\" linkVolume => %i")
                            << soundIdx << sound->id << sound->linkVolume;
                }
            }
            else if(!var.compareWithoutCase("Zero 4")) // ??
            {
                LOG_WARNING("Sound - \"Zero 4\" patches are not supported.");
            }
            else if(!var.compareWithoutCase("Neg. One 1")) // ??
            {
                LOG_WARNING("Sound - \"Neg. One 1\" patches are not supported.");
            }
            else if(!var.compareWithoutCase("Neg. One 2"))
            {
                int const lumpNum = expr.toInt(0, 0, String::AllowSuffix);
                if(!ignore)
                {
                    LumpIndex const &lumpIndex = *reinterpret_cast<LumpIndex const *>(F_LumpIndex());
                    int const numLumps = lumpIndex.size();
                    if(lumpNum < 0 || lumpNum >= numLumps)
                    {
                        LOG_WARNING("Neg. One 2 #%i out of range, ignoring.") << lumpNum;
                    }
                    else
                    {
                        qstrncpy(sound->lumpName, lumpIndex[lumpNum].name().toUtf8().constData(), DED_STRINGID_LEN + 1);
                        LOG_DEBUG("Sound #%i \"%s\" lumpName => \"%s\"")
                                << soundIdx << sound->id << sound->lumpName;
                    }
                }
            }
            else
            {
                LOG_WARNING("Unknown symbol \"%s\" encountered on line #%i, ignoring.")
                        << var << currentLineNumber;
            }
        }
    }

    void parseAmmo(int const ammoNum, bool ignore = false)
    {
        static char const *ammostr[4] = { "Clip", "Shell", "Cell", "Misl" };
        char const *theAmmo = ammostr[ammoNum];
        LOG_AS("parseAmmo");
        for(; lineInCurrentSection(); skipToNextLine())
        {
            String var, expr;
            parseAssignmentStatement(line, var, expr);

            if(!var.compareWithoutCase("Max ammo"))
            {
                int const value = expr.toInt(0, 10, String::AllowSuffix);
                if(!ignore) createValueDef(String("Player|Max ammo|%1").arg(theAmmo), QString::number(value));
            }
            else if(!var.compareWithoutCase("Per ammo"))
            {
                int per = expr.toInt(0, 10, String::AllowSuffix);
                if(!ignore) createValueDef(String("Player|Clip ammo|%1").arg(theAmmo), QString::number(per));
            }
            else
            {
                LOG_WARNING("Unknown symbol \"%s\" encountered on line #%i, ignoring.")
                        << var << currentLineNumber;
            }
        }
    }

    void parseWeapon(int const weapNum, bool ignore = false)
    {
        LOG_AS("parseWeapon");
        for(; lineInCurrentSection(); skipToNextLine())
        {
            String var, expr;
            parseAssignmentStatement(line, var, expr);

            if(var.endsWith(" frame", Qt::CaseInsensitive))
            {
                String const dehStateName = var.left(var.size() - 6);
                int const value           = expr.toInt(0, 0, String::AllowSuffix);

                WeaponStateMapping const *weapon;
                if(!parseWeaponState(dehStateName, &weapon))
                {
                    if(!ignore)
                        LOG_WARNING("Unknown frame '%s' on line #%i, ignoring.")
                                << dehStateName << currentLineNumber;
                }
                else
                {
                    if(!ignore)
                    {
                        if(value < 0 || value > ded->states.size())
                        {
                            LOG_WARNING("Frame #%i out of range, ignoring.") << value;
                        }
                        else
                        {
                            DENG2_ASSERT(weapon->id >= 0 && weapon->id < ded->states.size());

                            ded_state_t const &state = ded->states[value];
                            createValueDef(String("Weapon Info|%1|%2").arg(weapNum).arg(weapon->name),
                                           QString::fromUtf8(state.id));
                        }
                    }
                }
            }
            else if(!var.compareWithoutCase("Ammo type"))
            {
                String const ammotypes[] = { "clip", "shell", "cell", "misl", "-", "noammo" };
                int const value = expr.toInt(0, 10, String::AllowSuffix);
                if(!ignore)
                {
                    if(value < 0 || value >= 6)
                    {
                        LOG_WARNING("Unknown ammotype %i on line #%i, ignoring.")
                                << value << currentLineNumber;
                    }
                    else
                    {
                        createValueDef(String("Weapon Info|%1|Type").arg(weapNum), ammotypes[value]);
                    }
                }
            }
            else if(!var.compareWithoutCase("Ammo per shot")) // Eternity
            {
                int const value = expr.toInt(0, 10, String::AllowSuffix);
                if(!ignore) createValueDef(String("Weapon Info|%1|Per shot").arg(weapNum), QString::number(value));
            }
            else
            {
                LOG_WARNING("Unknown symbol \"%s\" encountered on line #%i, ignoring.")
                        << var << currentLineNumber;
            }
        }
    }

    void parsePointer(ded_state_t *state, bool ignore)
    {
        int const stateIdx = ded->states.indexOf(state);
        LOG_AS("parsePointer");
        for(; lineInCurrentSection(); skipToNextLine())
        {
            String var, expr;
            parseAssignmentStatement(line, var, expr);

            if(!var.compareWithoutCase("Codep Frame"))
            {
                int const actionIdx = expr.toInt(0, 0, String::AllowSuffix);
                if(!ignore)
                {
                    if(actionIdx < 0 || actionIdx >= NUMSTATES)
                    {
                        LOG_WARNING("Codep frame #%i out of range, ignoring.") << actionIdx;
                    }
                    else
                    {
                        ded_funcid_t const &newAction = origActionNames[actionIdx];
                        qstrncpy(state->action, newAction, DED_STRINGID_LEN + 1);
                        LOG_DEBUG("State #%i \"%s\" action => \"%s\"")
                                << stateIdx << state->id << state->action;
                    }
                }
            }
            else
            {
                LOG_WARNING("Unknown symbol \"%s\" encountered on line #%i, ignoring.")
                        << var << currentLineNumber;
            }
        }
    }

    void parseMisc()
    {
        LOG_AS("parseMisc");
        for(; lineInCurrentSection(); skipToNextLine())
        {
            String var, expr;
            parseAssignmentStatement(line, var, expr);

            ValueMapping const *mapping;
            if(parseMiscValue(var, &mapping))
            {
                int const value = expr.toInt(0, 10, String::AllowSuffix);
                createValueDef(mapping->valuePath, QString::number(value));
            }
            else
            {
                LOG_WARNING("Unknown value \"%s\" on line #%i, ignoring.")
                        << var << currentLineNumber;
            }
        }
    }

    void parsePars() // BEX
    {
        LOG_AS("parsePars");
        // BEX doesn't follow the same rules as .deh
        for(; !line.trimmed().isEmpty(); readLine())
        {
            // Skip comment lines.
            if(line.at(0) == '#') continue;

            try
            {
                if(line.beginsWith("par", Qt::CaseInsensitive))
                {
                    String const argStr = line.substr(3).leftStrip();
                    if(argStr.isEmpty())
                    {
                        throw SyntaxError("parseParsBex", String("Expected format expression on line #%1")
                                                              .arg(currentLineNumber));
                    }

                    /**
                     * @attention Team TNT's original DEH parser would read the first one
                     * or two tokens then apply atoi() on the remainder of the line to
                     * obtain the last argument (i.e., par time).
                     *
                     * Here we emulate this behavior by splitting the line into at most
                     * three arguments and then apply atoi()-like de::String::toIntLeft()
                     * on the last.
                     */
                    int const maxArgs = 3;
                    QStringList args = splitMax(argStr, ' ', maxArgs);

                    // If the third argument is a comment remove it.
                    if(args.size() == 3)
                    {
                        if(String(args.at(2)).beginsWith('#'))
                            args.removeAt(2);
                    }

                    if(args.size() < 2)
                    {
                        throw SyntaxError("parseParsBex", String("Invalid format string \"%1\" on line #%2")
                                                              .arg(argStr).arg(currentLineNumber));
                    }

                    // Parse values from the arguments.
                    int arg = 0;
                    int episode   = (args.size() > 2? args.at(arg++).toInt(0, 10) : 0);
                    int map       = args.at(arg++).toInt(0, 10);
                    float parTime = float(String(args.at(arg++)).toInt(0, 10, String::AllowSuffix));

                    // Apply.
                    de::Uri const uri = composeMapUri(episode, map);
                    ded_mapinfo_t *def;
                    int idx = mapInfoDefForUri(uri, &def);
                    if(idx >= 0)
                    {
                        def->parTime = parTime;
                        LOG_DEBUG("MapInfo #%i \"%s\" parTime => %d")
                                << idx << uri << def->parTime;
                    }
                    else
                    {
                        LOG_WARNING("Failed locating MapInfo for \"%s\" (episode:%i, map:%i), ignoring.")
                                << uri << episode << map;
                    }
                }
            }
            catch(SyntaxError const &er)
            {
                LOG_WARNING("%s, ignoring.") << er.asText();
            }
        }

        if(line.trimmed().isEmpty())
        {
            skipToNextSection();
        }
    }

    void parseHelper() // BEX
    {
        LOG_AS("parseHelper");
        LOG_WARNING("[HELPER] patches are not supported.");
    }

    void parseSprites() // BEX
    {
        LOG_AS("parseSprites");
        LOG_WARNING("[SPRITES] patches are not supported.");
    }

    void parseSounds() // BEX
    {
        LOG_AS("parseSounds");
        // BEX doesn't follow the same rules as .deh
        for(; !line.trimmed().isEmpty(); readLine())
        {
            // Skip comment lines.
            if(line.at(0) == '#') continue;

            try
            {
                String var, expr;
                parseAssignmentStatement(line, var, expr);
                if(!patchSoundLumpNames(var, expr))
                {
                    LOG_WARNING("Failed to locate sound \"%s\" for patching.") << var;
                }
            }
            catch(SyntaxError const &er)
            {
                LOG_WARNING("%s, ignoring.") << er.asText();
            }
        }

        if(line.trimmed().isEmpty())
        {
            skipToNextSection();
        }
    }

    void parseMusic() // BEX
    {
        LOG_AS("parseMusic");
        // BEX doesn't follow the same rules as .deh
        for(; !line.trimmed().isEmpty(); readLine())
        {
            // Skip comment lines.
            if(line.at(0) == '#') continue;

            try
            {
                String var, expr;
                parseAssignmentStatement(line, var, expr);
                if(!patchMusicLumpNames(var, expr))
                {
                    LOG_WARNING("Failed to locate music \"%s\" for patching.") << var;
                }
            }
            catch(SyntaxError const &er)
            {
                LOG_WARNING("%s, ignoring.") << er.asText();
            }
        }

        if(line.trimmed().isEmpty())
        {
            skipToNextSection();
        }
    }

    void parseCodePointers() // BEX
    {
        LOG_AS("parseCodePointers");
        // BEX doesn't follow the same rules as .deh
        for(; !line.trimmed().isEmpty(); readLine())
        {
            // Skip comment lines.
            if(line.at(0) == '#') continue;

            String var, expr;
            parseAssignmentStatement(line, var, expr);

            if(var.startsWith("Frame ", Qt::CaseInsensitive))
            {
                int const stateNum = var.substr(6).toInt(0, 0, String::AllowSuffix);
                if(stateNum < 0 || stateNum >= ded->states.size())
                {
                    LOG_WARNING("Frame #%d out of range, ignoring. (Create more State defs!)")
                            << stateNum;
                }
                else
                {
                    ded_state_t &state = ded->states[stateNum];

                    // Compose the action name.
                    String action = expr.rightStrip();
                    if(!action.startsWith("A_", Qt::CaseInsensitive))
                        action.prepend("A_");
                    action.truncate(32);

                    // Is this a known action?
                    if(!action.compareWithoutCase("A_NULL"))
                    {
                        qstrncpy(state.action, "NULL", DED_STRINGID_LEN+1);
                        LOG_DEBUG("State #%i \"%s\" action => \"NULL\"")
                                << stateNum << state.id;
                    }
                    else
                    {
                        Block actionUtf8 = action.toUtf8();
                        if(Def_Get(DD_DEF_ACTION, actionUtf8.constData(), 0) >= 0)
                        {
                            qstrncpy(state.action, actionUtf8.constData(), DED_STRINGID_LEN + 1);
                            LOG_DEBUG("State #%i \"%s\" action => \"%s\"")
                                    << stateNum << state.id << state.action;
                        }
                        else
                        {
                            LOG_WARNING("Unknown action '%s' on line #%i, ignoring.")
                                    << action.mid(2) << currentLineNumber;
                        }
                    }
                }
            }
        }

        if(line.trimmed().isEmpty())
        {
            skipToNextSection();
        }
    }

    void parseText(int const oldSize, int const newSize)
    {
        LOG_AS("parseText");

        String const oldStr = readTextBlob(oldSize);
        String const newStr = readTextBlob(newSize);

        if(!(flags & NoText)) // Disabled?
        {
            // Try each type of "text" replacement in turn...
            bool found = false;
            if(patchFinaleBackgroundNames(oldStr, newStr))  found = true;
            if(patchMusicLumpNames(oldStr, newStr))         found = true;
            if(patchSpriteNames(oldStr, newStr))            found = true;
            if(patchSoundLumpNames(oldStr, newStr))         found = true;
            if(patchText(oldStr, newStr))                   found = true;

            // Give up?
            if(!found)
            {
                LOG_WARNING("Failed to determine source for:\nText %i %i\n%s")
                        << oldSize << newSize << oldStr;
            }
        }
        else
        {
            LOG_DEBUG("Skipping disabled Text patch.");
        }

        skipToNextLine();
    }

    static void replaceTextValue(String const &id, String newValue)
    {
        if(id.isEmpty()) return;

        int textIdx = Def_Get(DD_DEF_TEXT, id.toUtf8().constData(), nullptr);
        if(textIdx < 0) return;

        // We must escape new lines.
        newValue.replace("\n", "\\n");

        // Replace this text.
        Def_Set(DD_DEF_TEXT, textIdx, 0, newValue.toUtf8().constData());
        LOG_DEBUG("Text #%i \"%s\" is now:\n%s")
                << textIdx << id << newValue;
    }

    void parseStrings() // BEX
    {
        LOG_AS("parseStrings");

        bool multiline = false;
        String textId;
        String newValue;

        // BEX doesn't follow the same rules as .deh
        for(;; readLine())
        {
            if(!multiline)
            {
                if(line.trimmed().isEmpty()) break;

                // Skip comment lines.
                if(line.at(0) == '#') continue;

                // Determine the split (or 'pivot') position.
                int assign = line.indexOf('=');
                if(assign < 0)
                {
                    throw SyntaxError("parseStrings", String("Expected assignment statement but encountered \"%1\" on line #%2")
                                                        .arg(line).arg(currentLineNumber));
                }

                textId = line.substr(0, assign).rightStrip();

                // Nothing before '=' ?
                if(textId.isEmpty())
                {
                    throw SyntaxError("parseStrings", String("Expected keyword before '=' on line #%1")
                                                        .arg(currentLineNumber));
                }

                newValue = line.substr(assign + 1).leftStrip();
            }
            else
            {
                newValue += line.leftStrip();
            }

            // Concatenate another multi-line replacement?
            if(newValue.endsWith('\\'))
            {
                newValue.truncate(newValue.length() - 1);
                multiline = true;
                continue;
            }

            replaceTextValue(textId, newValue);
            multiline = false;
        }

        if(line.trimmed().isEmpty())
        {
            skipToNextSection();
        }
    }

    void createValueDef(String const &path, String const &value)
    {
        // An existing value?
        ded_value_t *def;
        int idx = valueDefForPath(path, &def);
        if(idx < 0)
        {
            // Not found - create a new Value.
            def = ded->values.append();
            def->id = M_StrDup(path.toUtf8());
            def->text = 0;

            idx = ded->values.indexOf(def);
        }

        def->text = static_cast<char *>(M_Realloc(def->text, value.length() + 1));
        qstrcpy(def->text, value.toUtf8());

        LOG_DEBUG("Value #%i \"%s\" => \"%s\"") << idx << path << def->text;
    }

    bool patchSpriteNames(String const &origName, String const &newName)
    {
        // Is this a sprite name?
        if(origName.length() != 4) return false;

        // Only sprites names in the original name map can be patched.
        /// @todo Why the restriction?
        int spriteIdx = findSpriteNameInMap(origName);
        if(spriteIdx < 0) return false;

        /// @todo Presently disabled because the engine can't handle remapping.
        DENG2_UNUSED(newName);
        LOG_WARNING("Sprite name table remapping is not supported.");
        return true; // Pretend success.

#if 0
        if(spriteIdx >= ded->count.sprites.num)
        {
            LOG_WARNING("Sprite name #%i out of range, ignoring.") << spriteIdx;
            return false;
        }

        ded_sprid_t &def = ded->sprites[spriteIdx];
        Block newNameUtf8 = newName.toUtf8();
        qstrncpy(def.id, newNameUtf8.constData(), DED_SPRITEID_LEN + 1);

        LOG_DEBUG("Sprite #%i id => \"%s\"") << spriteIdx << def.id;
        return true;
#endif
    }

    bool patchFinaleBackgroundNames(String const &origName, String const &newName)
    {
        FinaleBackgroundMapping const *mapping;
        if(findFinaleBackgroundMappingForText(origName, &mapping) < 0) return false;
        createValueDef(mapping->mnemonic, newName);
        return true;
    }

    bool patchMusicLumpNames(String const &origName, String const &newName)
    {
        // Only music lump names in the original name map can be patched.
        /// @todo Why the restriction?
        if(findMusicLumpNameInMap(origName) < 0) return false;

        Block origNamePrefUtf8 = String("D_%1").arg(origName).toUtf8();
        Block newNamePrefUtf8  = String("D_%1").arg(newName ).toUtf8();

        // Update ALL songs using this lump name.
        int numPatched = 0;
        for(int i = 0; i < ded->music.size(); ++i)
        {
            ded_music_t &music = ded->music[i];
            if(qstricmp(music.lumpName, origNamePrefUtf8.constData())) continue;

            qstrncpy(music.lumpName, newNamePrefUtf8.constData(), 9);
            numPatched++;

            LOG_DEBUG("Music #%i \"%s\" lumpName => \"%s\"")
                    << i << music.id << music.lumpName;
        }
        return (numPatched > 0);
    }

    bool patchSoundLumpNames(String const &origName, String const &newName)
    {
        // Only sound lump names in the original name map can be patched.
        /// @todo Why the restriction?
        if(findSoundLumpNameInMap(origName) < 0) return false;

        Block origNamePrefUtf8 = String("DS%1").arg(origName).toUtf8();
        Block newNamePrefUtf8  = String("DS%1").arg(newName ).toUtf8();

        // Update ALL sounds using this lump name.
        int numPatched = 0;
        for(int i = 0; i < ded->sounds.size(); ++i)
        {
            ded_sound_t &sound = ded->sounds[i];
            if(qstricmp(sound.lumpName, origNamePrefUtf8.constData())) continue;

            qstrncpy(sound.lumpName, newNamePrefUtf8.constData(), 9);
            numPatched++;

            LOG_DEBUG("Sound #%i \"%s\" lumpName => \"%s\"")
                    << i << sound.id << sound.lumpName;
        }
        return (numPatched > 0);
    }

    bool patchText(String const &origStr, String const &newStr)
    {
        TextMapping const *textMapping;

        // Which text are we replacing?
        if(textMappingForBlob(origStr, &textMapping) < 0) return false;

        // Is replacement disallowed/not-supported?
        if(textMapping->name.isEmpty()) return true; // Pretend success.

        Block textNameUtf8 = textMapping->name.toUtf8();
        int textIdx = Def_Get(DD_DEF_TEXT, textNameUtf8.constData(), NULL);
        if(textIdx < 0) return false;

        // We must escape new lines.
        String newStrCopy = newStr;
        Block newStrUtf8 = newStrCopy.replace("\n", "\\n").toUtf8();

        // Replace this text.
        Def_Set(DD_DEF_TEXT, textIdx, 0, newStrUtf8.constData());

        LOG_DEBUG("Text #%i \"%s\" is now:\n%s")
                << textIdx << textMapping->name << newStrUtf8.constData();
        return true;
    }
};

void readDehPatch(Block const &patch, DehReaderFlags flags)
{
    try
    {
        DehReader(patch, flags).parse();
    }
    catch(Error const &er)
    {
        LOG_WARNING(er.asText() + ".");
    }
}
