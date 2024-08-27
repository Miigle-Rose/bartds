# SPDX-License-Identifier: CC0-1.0
#
# SPDX-FileContributor: Michele Di Giorgio, 2024

BLOCKSDS	?= /opt/blocksds/core

# User config

NAME		:= bart
GAME_ICON	:= $(CURDIR)/graphics/icon/icon.bmp
GAME_TITLE	:= Click The Bart
GAME_SUBTITLE	:= By Miigle
GAME_SUBTITLE2	:= Built with BlocksDS

# Source code paths

AUDIODIRS	:= audio
NITROFSDIR  := nitrofs
GFXDIRS		:= graphics

# Libraries

LIBS		:= -lnds9 -lmm9
LIBDIRS		:= $(BLOCKSDS)/libs/maxmod

include $(BLOCKSDS)/sys/default_makefiles/rom_arm9/Makefile