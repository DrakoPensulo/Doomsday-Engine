# Doomsday Engine

This is the source code for Doomsday Engine: a portable, enhanced source port of id Software's Doom I/II and Raven Software's Heretic and Hexen. The sources are under the GNU General Public license (see doomsday/gpl-3.0.txt), with the exception of the Doomsday 2 libraries that are under the GNU Lesser General Public License (see doomsday/lgpl-3.0.txt).

For [compilation instructions](https://manual.dengine.net/devel/compilation) and other details, see the [Doomsday Manual](https://manual.dengine.net/).

Linux 64-bit [![Linux Build Status](https://travis-ci.org/skyjake/Doomsday-Engine.svg)](https://travis-ci.org/skyjake/Doomsday-Engine) Windows 32-bit [![Windows Build Status](https://ci.appveyor.com/api/projects/status/79h7egw7q225gj2h?svg=true)](https://ci.appveyor.com/project/skyjake/doomsday-engine)

## Libraries

**libcore** is the core of Doomsday 2. It is a C++ class framework containing functionality such as the file system, plugin loading, Doomsday Script, network communications, and generic data structures. Almost everything relies or will rely on this core library.

**liblegacy** is a collection of C language routines extracted from the old Doomsday 1 code base. Its purpose is to (eventually) act as a C wrapper for libcore. (Game plugins are mostly in C.)

**libgui** builds on libcore to add low-level GUI capabilities such as OpenGL graphics, fonts, images, and input devices.

**libappfw** contains the Doomsday UI framework: widgets, generic dialogs, abstract data models. libappfw is built on libgui and libcore.

**libshell** has functionality related to connecting to and controlling Doomsday servers remotely.

**libdoomsday** is an application-level library that contains shared functionality for the client, server, and plugins.

## External Dependencies

### CMake

Doomsday is compiled using [CMake](http://cmake.org/). Version 3.1 or later is required.

### Qt

Using the latest version of Qt 5 is recommended. The minimum required version is Qt 5.5 (complete build) or Qt 5.0 (client disabled).

### Open Asset Import Library

libgui requires the [Open Asset Import Library](http://assimp.sourceforge.net/lib_html/index.html) for reading 3D model and animation files. It is compiled automatically as part of the build and is expected to be present as a Git submodule in _doomsday/external/assimp_. Source tarballs come with the Assimp sources included.

### SDL 2

[SDL 2](http://libsdl.org) is needed for game controller input (e.g., joysticks and gamepads). Additionally, SDL2_mixer can be used for audio output (not required).

### FMOD Studio

The optional FMOD audio plugin requires the [FMOD Studio Low-Level Programmer API](http://www.fmod.org/download).

## Branches

The following branches are currently active in the repository.

- **master**: Main code base. This is where releases are made from on a biweekly basis. Bug fixing is done in this branch, while larger development efforts occur in separate work branches.
- **stable**: Latest stable release. Patch releases can be made from this branch when necessary.
- **stable-x.y**: Stable release x.y.
- **legacy**: Old stable code base. Currently at the 1.8.6 release.
