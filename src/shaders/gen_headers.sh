#!/bin/bash
#
# Copyright 2014-2015 Dario Manesku. All rights reserved.
# License: http://www.opensource.org/licenses/BSD-2-Clause
#

FILENAME="headers/shaders.h"

echo "Generating '$FILENAME'..."

# Erase contents.
> $FILENAME

echo "#ifndef CMFTSTUDIO_SHADERS_H_HEADER_GUARD" >> $FILENAME
echo "#define CMFTSTUDIO_SHADERS_H_HEADER_GUARD" >> $FILENAME
echo "" >> $FILENAME

# Append each header.
for HEADER in $(ls headers/vs_*.h headers/fs_*.h)
do
	echo "#include \"$(basename $HEADER)\"" >> $FILENAME
done

echo "" >> $FILENAME
echo "#endif // CMFTSTUDIO_SHADERS_H_HEADER_GUARD" >> $FILENAME

echo "Done."
