#
# Copyright 2014-2015 Dario Manesku. All rights reserved.
# License: http://www.opensource.org/licenses/BSD-2-Clause
#

VS2008_DEVENV_DIR=C:\Program Files (x86)\Microsoft Visual Studio 9.0\Common7\IDE
VS2010_DEVENV_DIR=C:\Program Files (x86)\Microsoft Visual Studio 11.0\Common7\IDE
VS2012_DEVENV_DIR=C:\Program Files (x86)\Microsoft Visual Studio 11.0\Common7\IDE

SILENT ?= @

UNAME := $(shell uname -s)
ifeq ($(UNAME),$(filter $(UNAME),Linux Darwin))
	ifeq ($(UNAME),$(filter $(UNAME),Darwin))
		OS=darwin
	else
		OS=linux
	endif
else
	OS=windows
endif

GENIE=./../bx/tools/bin/$(OS)/genie --unity-build --with-amalgamated

export CMFTVIEWER_WIN_CLANG_DIR_=$(subst \,\\,$(subst /,\,$(WIN_CLANG_DIR)))
export CMFTVIEWER_WIN_MINGW_DIR_=$(subst \,\\,$(subst /,\,$(WIN_MINGW_DIR)))

.PHONY: all
all:
	$(GENIE) --file=scripts/main.lua xcode4
	$(GENIE) --file=scripts/main.lua vs2008
	$(GENIE) --file=scripts/main.lua vs2010
	$(GENIE) --file=scripts/main.lua vs2012
	$(GENIE) --file=scripts/main.lua vs2013
	$(GENIE) --file=scripts/main.lua --gcc=mingw-gcc gmake
	$(GENIE) --file=scripts/main.lua --gcc=linux-gcc gmake
	$(GENIE) --file=scripts/main.lua --gcc=osx       gmake

.PHONY: clean-projects
clean-projects:
	@echo Removing _projects folder.
	-@rm -rf _projects

.PHONY: clean-build
clean-build:
	@echo Removing _build folder.
	-@rm -rf _build

.PHONY: clean
clean: clean-build clean-projects

_projects/xcode4:
	$(GENIE) --file=scripts/main.lua xcode4

_projects/vs2008:
	$(GENIE) --file=scripts/main.lua vs2008
vs2008-debug32:
	"$(subst /,\\,$(VS2008_DEVENV_DIR))\devenv" _projects/vs2008/cmftStudio.sln /Build "Debug|Win32"
vs2008-release32:
	"$(subst /,\\,$(VS2008_DEVENV_DIR))\devenv" _projects/vs2008/cmftStudio.sln /Build "Release|Win32"
vs2008-debug64:
	"$(subst /,\\,$(VS2008_DEVENV_DIR))\devenv" _projects/vs2008/cmftStudio.sln /Build "Debug|x64"
vs2008-release64:
	"$(subst /,\\,$(VS2008_DEVENV_DIR))\devenv" _projects/vs2008/cmftStudio.sln /Build "Release|x64"
vs2008: vs2008-debug32 vs2008-release32 vs2008-debug64 vs2008-release64

_projects/vs2010:
	$(GENIE) --file=scripts/main.lua vs2010
vs2010-debug32:
	"$(subst /,\\,$(VS2010_DEVENV_DIR))\devenv" _projects/vs2010/cmftStudio.sln /Build "Debug|Win32"
vs2010-release32:
	"$(subst /,\\,$(VS2010_DEVENV_DIR))\devenv" _projects/vs2010/cmftStudio.sln /Build "Release|Win32"
vs2010-debug64:
	"$(subst /,\\,$(VS2010_DEVENV_DIR))\devenv" _projects/vs2010/cmftStudio.sln /Build "Debug|x64"
vs2010-release64:
	"$(subst /,\\,$(VS2010_DEVENV_DIR))\devenv" _projects/vs2010/cmftStudio.sln /Build "Release|x64"

_projects/vs2012:
	$(GENIE) --file=scripts/main.lua vs2012
vs2012-debug32:
	"$(subst /,\\,$(VS2012_DEVENV_DIR))\devenv" _projects/vs2012/cmftStudio.sln /Build "Debug|Win32"
vs2012-release32:
	"$(subst /,\\,$(VS2012_DEVENV_DIR))\devenv" _projects/vs2012/cmftStudio.sln /Build "Release|Win32"
vs2012-debug64:
	"$(subst /,\\,$(VS2012_DEVENV_DIR))\devenv" _projects/vs2012/cmftStudio.sln /Build "Debug|x64"
vs2012-release64:
	"$(subst /,\\,$(VS2012_DEVENV_DIR))\devenv" _projects/vs2012/cmftStudio.sln /Build "Release|x64"

_projects/vs2013:
	$(GENIE) --file=scripts/main.lua vs2013
vs2013-debug32:
	"$(subst /,\\,$(VS2013_DEVENV_DIR))\devenv" _projects/vs2013/cmftStudio.sln /Build "Debug|Win32"
vs2013-release32:
	"$(subst /,\\,$(VS2013_DEVENV_DIR))\devenv" _projects/vs2013/cmftStudio.sln /Build "Release|Win32"
vs2013-debug64:
	"$(subst /,\\,$(VS2013_DEVENV_DIR))\devenv" _projects/vs2013/cmftStudio.sln /Build "Debug|x64"
vs2013-release64:
	"$(subst /,\\,$(VS2013_DEVENV_DIR))\devenv" _projects/vs2013/cmftStudio.sln /Build "Release|x64"

_projects/gmake-linux:
	$(GENIE) --file=scripts/main.lua --gcc=linux-gcc gmake
linux-debug32: _projects/gmake-linux
	make -R -C _projects/gmake-linux config=debug32
linux-release32: _projects/gmake-linux
	make -R -C _projects/gmake-linux config=release32
linux-debug64: _projects/gmake-linux
	make -R -C _projects/gmake-linux config=debug64
linux-release64: _projects/gmake-linux
	make -R -C _projects/gmake-linux config=release64
linux: linux-debug32 linux-release32 linux-debug64 linux-release64

_projects/gmake-osx:
	$(GENIE) --file=scripts/main.lua --gcc=osx gmake
osx-debug32: _projects/gmake-osx
	make -R -C _projects/gmake-osx config=debug32
osx-release32: _projects/gmake-osx
	make -R -C _projects/gmake-osx config=release32
osx-debug64: _projects/gmake-osx
	make -R -C _projects/gmake-osx config=debug64
osx-release64: _projects/gmake-osx
	make -R -C _projects/gmake-osx config=release64
osx: osx-debug32 osx-release32 osx-debug64 osx-release64

_projects/gmake-mingw-gcc:
	$(GENIE) --file=scripts/main.lua --gcc=mingw-gcc gmake
mingw-gcc-debug32: _projects/gmake-mingw-gcc
	make -R -C _projects/gmake-mingw-gcc config=debug32
mingw-gcc-release32: _projects/gmake-mingw-gcc
	make -R -C _projects/gmake-mingw-gcc config=release32
mingw-gcc-debug64: _projects/gmake-mingw-gcc
	make -R -C _projects/gmake-mingw-gcc config=debug64
mingw-gcc-release64: _projects/gmake-mingw-gcc
	make -R -C _projects/gmake-mingw-gcc config=release64
mingw: mingw-gcc-debug32 mingw-gcc-release32 mingw-gcc-debug64 mingw-gcc-release64

ICONS_DIR            = ~/.icons/
RES_DIR              = res/
CMFTSTUDIO_DESKTOP   = ~/Desktop/cmftStudio.desktop
CMFTSTUDIO_ICON_PNG  = cmftstudio_icon.png
CMFTSTUDIO_ICON_ICON = cmftstudio_icon.icon

CMFTSTUDIO_BIN_SRC = _build/linux64_gcc/bin/cmftStudioRelease
CMFTSTUDIO_BIN_DST = /usr/local/bin/cmftStudio

CMFTSTUDIO_ICON_PNG_SRC  = $(addprefix $(RES_DIR),   $(CMFTSTUDIO_ICON_PNG))
CMFTSTUDIO_ICON_PNG_DST  = $(addprefix $(ICONS_DIR), $(CMFTSTUDIO_ICON_PNG))
CMFTSTUDIO_ICON_ICON_DST = $(addprefix $(ICONS_DIR), $(CMFTSTUDIO_ICON_ICON))

.PHONY: linux-install
linux-install: all linux-release64
	$(SILENT) sudo cp $(CMFTSTUDIO_BIN_SRC) $(CMFTSTUDIO_BIN_DST)      2> /dev/null
	$(SILENT) sudo chmod 755 $(CMFTSTUDIO_BIN_DST)                     2> /dev/null
	$(SILENT) mkdir -p $(ICONS_DIR)                                    2> /dev/null
	$(SILENT) cp $(CMFTSTUDIO_ICON_PNG_SRC) $(CMFTSTUDIO_ICON_PNG_DST) 2> /dev/null
	$(SILENT) echo $(ICON_CONTENTS) > $(CMFTSTUDIO_ICON_ICON_DST)      2> /dev/null
	$(SILENT) echo $(DESKTOP_CONTENTS) > $(CMFTSTUDIO_DESKTOP)         2> /dev/null
	$(SILENT) chmod +x $(CMFTSTUDIO_DESKTOP)                           2> /dev/null
	$(SILENT) echo "Install: cmftStudio successfully installed!"
	$(SILENT) echo "Install: Desktop shortcut created."

.PHONY: linux-uninstall
linux-uninstall:
	$(SILENT) sudo rm -f $(CMFTSTUDIO_BIN_DST)  2> /dev/null
	$(SILENT) rm -f $(CMFTSTUDIO_DESKTOP)       2> /dev/null
	$(SILENT) rm -f $(CMFTSTUDIO_ICON_PNG_DST)  2> /dev/null
	$(SILENT) rm -f $(CMFTSTUDIO_ICON_ICON_DST) 2> /dev/null
	$(SILENT) echo "Uninstall: cmftStudio successfully uninstalled."

DESKTOP_CONTENTS = "[Desktop Entry]\nName=cmftStudio\nComment=Cubemap Filtering Tool Studio\nExec=cmftStudio\nIcon=cmftstudio_icon\nTerminal=false\nType=Application\nCategories=Productivity"
ICON_CONTENTS = "[Icon Data]\n\nDisplayName=cmftstudio_icon"

