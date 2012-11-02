/**
 * @file filedirectory.h
 *
 * FileDirectory. Core system component representing a hierarchical file path
 * structure.
 *
 * A specialization of de::PathTree which implements automatic population
 * of the directory itself from the virtual file system.
 *
 * @note Paths are resolved prior to pushing them into the directory.
 *
 * @ingroup fs
 *
 * @authors Copyright &copy; 2003-2012 Jaakko Keränen <jaakko.keranen@iki.fi>
 * @authors Copyright &copy; 2009-2012 Daniel Swanson <danij@dengine.net>
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

#ifndef LIBDENG_FILEDIRECTORY_H
#define LIBDENG_FILEDIRECTORY_H

#include "uri.h"
#include "pathtree.h"

#ifdef __cplusplus

namespace de {

class FileDirectory : private PathTree
{
public:
    /**
     * @param basePath  Used with directories which represent the internal paths
     *                  natively as relative paths. @c NULL means only absolute
     *                  paths will be added to the directory (attempts to add
     *                  relative paths will fail silently).
     */
    explicit FileDirectory(char const* basePath = 0);
    ~FileDirectory();

    /**
     * Add a new path. Duplicates are automatically pruned.
     *
     * @param flags         @ref searchPathFlags
     * @param path          Path to be added.
     * @param callback      Callback to make if the path was added to this directory.
     * @param parameters    Passed to the callback.
     */
    void addPath(int flags, de::Uri const& searchPath,
                 int (*callback) (Node& node, void* parameters), void* parameters = 0);

    /**
     * Clear this file directory's contents.
     */
    void clear();

    /**
     * Find a single path in this directory.
     *
     * @param type              Only consider nodes of this type.
     * @param searchPath        Relative or absolute path.
     * @param searchDelimiter   Fragments of @a searchPath are delimited by this.
     * @param foundPath         If not @c NULL, the fully composed path to the found
     *                          directory node is written back here.
     * @param foundDelimiter    Delimiter to be use when composing @a foundPath.
     *
     * @return  @c true iff successful.
     */
    bool find(NodeType type, char const* searchPath, char searchDelimiter = '/',
              ddstring_t* foundPath = 0, char foundDelimiter = '/');

    /**
     * Collate all referenced paths in the hierarchy into a list.
     *
     * @param found         Set of paths that match the result.
     * @param flags         @ref pathComparisonFlags
     * @param delimiter     Fragments of the composed path will be delimited by this character.
     *
     * @return  Number of paths found.
     */
    int findAllPaths(FoundPaths& found, int flags = 0, char delimiter = '/');

#if _DEBUG
    static void debugPrint(FileDirectory& inst);
    static void debugPrintHashDistribution(FileDirectory& inst);
#endif

private:
    struct Instance;
    Instance* d;
};

} // namespace de

extern "C" {
#endif

struct filedirectory_s; // The filedirectory instance (opaque).
typedef struct filedirectory_s FileDirectory;

#ifdef __cplusplus
} // extern "C"
#endif

#endif /* LIBDENG_FILEDIRECTORY_H */
