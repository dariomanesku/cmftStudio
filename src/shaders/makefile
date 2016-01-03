#
# Copyright 2014-2015 Dario Manesku. All rights reserved.
# License: http://www.opensource.org/licenses/BSD-2-Clause
#

BGFX_DIR=../../dependency/bgfx
RUNTIME_DIR=../../runtime
BUILD_DIR=../../_build

include $(BGFX_DIR)/scripts/shader.mk

rebuild:
	@make -s --no-print-directory TARGET=0 clean all
	@make -s --no-print-directory TARGET=1 clean all
	@make -s --no-print-directory TARGET=2 clean all
	@make -s --no-print-directory TARGET=3 clean all
	@make -s --no-print-directory TARGET=4 clean all
