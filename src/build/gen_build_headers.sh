#!/bin/sh
#
#  Copyright 2014-2015 Dario Manesku. All rights reserved.
#  License: http://www.opensource.org/licenses/BSD-2-Clause
#

SRC=(../*_cpp.h ../common/*_cpp.h ../common/allocator/*_cpp.h)
APP=cmftStudioApp.cpp
RES=cmftStudioApp_static_resources.cpp

echo "Generating '$APP' and '$RES'..."
echo -n > $APP
echo -n > $RES

for file in ${SRC[*]}; do
    if [ $file == "../staticres_cpp.h" ]; then
        echo "#include \"$file\"" >> $RES
    else
        echo "#include \"$file\"" >> $APP
    fi
done

echo "Done."
