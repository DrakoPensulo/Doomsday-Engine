/** @file fontlinewrapping.cpp  Font line wrapping.
 *
 * @authors Copyright (c) 2013 Jaakko Keränen <jaakko.keranen@iki.fi>
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
 * General Public License along with this program; if not, see:
 * http://www.gnu.org/licenses</small> 
 */

#include "ui/widgets/fontlinewrapping.h"

using namespace de;
using namespace de::shell;

DENG2_PIMPL_NOREF(FontLineWrapping)
{
    Font const *font;
    QList<WrappedLine> lines;
    String text;

    Instance() : font(0) {}

    int rangeWidth(Range const &range) const
    {
        if(font)
        {
            return font->measure(text.substr(range.start, range.size())).width();
        }
        return 0;
    }
};

FontLineWrapping::FontLineWrapping() : d(new Instance)
{}

void FontLineWrapping::setFont(Font const &font)
{
    d->font = &font;
}

Font const &FontLineWrapping::font() const
{
    DENG2_ASSERT(d->font != 0);
    return *d->font;
}

bool FontLineWrapping::isEmpty() const
{
    return d->lines.isEmpty();
}

void FontLineWrapping::clear()
{
    d->lines.clear();
    d->text.clear();
}

void FontLineWrapping::wrapTextToWidth(String const &text, int maxWidth)
{
    /**
     * @note This is quite similar to MonospaceLineWrapping::wrapTextToWidth().
     * Perhaps a generic method could be abstracted from these two.
     */

    QChar const newline('\n');

    clear();

    if(maxWidth < 1 || !d->font) return;

    // This is the text that we will be wrapping.
    d->text = text;

    int begin = 0;
    forever
    {
        // Newlines always cause a wrap.
        int end = begin;
        while(d->rangeWidth(Range(begin, end)) < maxWidth &&
              end < text.size() && text.at(end) != newline) ++end;

        if(end == text.size())
        {
            // Out of characters; time to stop.
            d->lines.append(WrappedLine(Range(begin, text.size())));
            break;
        }

        int const wrapPosMax = de::max(end - 1, begin + 1);

        // Find a good (whitespace) break point.
        while(!text.at(end).isSpace())
        {
            if(--end == begin)
            {
                // Ran out of non-space chars, force a break.
                end = wrapPosMax;
                break;
            }
        }

        if(text.at(end) == newline)
        {
            // The newline will be omitted from the wrapped lines.
            d->lines.append(WrappedLine(Range(begin, end)));
            begin = end + 1;
        }
        else
        {
            if(text.at(end).isSpace()) ++end;
            d->lines.append(WrappedLine(Range(begin, end)));
            begin = end;
        }
    }

    // Mark the final line.
    d->lines.last().isFinal = true;
}

WrappedLine FontLineWrapping::line(int index) const
{
    DENG2_ASSERT(index >= 0 && index < height());
    return d->lines[index];
}

int FontLineWrapping::width() const
{
    if(!d->font) return 0;

    int w = 0;
    for(int i = 0; i < d->lines.size(); ++i)
    {
        WrappedLine const &span = d->lines[i];
        w = de::max(w, d->rangeWidth(span.range));
    }
    return w;
}

int FontLineWrapping::height() const
{
    return d->lines.size();
}

int FontLineWrapping::totalHeightInPixels() const
{
    if(!d->font) return 0;

    int const lines = height();
    int pixels = 0;

    if(lines > 1)
    {
        // Full baseline-to-baseline spacing.
        pixels += (lines - 1) * d->font->lineSpacing().value();
    }
    if(lines > 0)
    {
        // The last (or a single) line is just one 'font height' tall.
        pixels += d->font->height().value();
    }
    return pixels;
}
