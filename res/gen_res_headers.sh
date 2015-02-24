#!/bin/sh
#
#  Copyright 2014-2015 Dario Manesku. All rights reserved.
#  License: http://www.opensource.org/licenses/BSD-2-Clause
#

if [ $(uname -o) == "Darwin" ]; then
    OS="darwin"
elif [ $(uname -o) == "Linux" ]; then
    OS="linux"
else
    OS="windows"
fi

BXDIR=../../bx
BIN2C=$BXDIR/tools/bin/$OS/bin2c
RAWCOMPRESS=../tools/bin/$OS/rawcompress
TMPFN=".tmp_file_6029bf40ad3811e4ab270800200c9a66"
OUTDIR=../src/res

# $1 - input
# $2 - output
# $3 - name
compressedToC()
{
    echo Compressing $1 ...
    $($RAWCOMPRESS -i $1 -o $TMPFN)
    echo Creating $OUTDIR/$2 \($3\) ...
    $($BIN2C -f $TMPFN -o $OUTDIR/$2 -n $3)
    $(rm $TMPFN)
    echo Done.
}

# $1 - input
# $2 - output
# $3 - name
rawToC()
{
    echo Creating $OUTDIR/$2 \($3\) ...
    $($BIN2C -f $1 -o $OUTDIR/$2 -n $3)
    echo Done.
}

# Uncompressed:
rawToC Sphere.bin sphere.h sc_sphereMesh

# Cubemaps:
compressedToC CmftStudio_skybox.dds logo_skybox.h sc_logoSkyboxCompressed
compressedToC CmftStudio_pmrem.dds  logo_pmrem.h  sc_logoPmremCompressed
compressedToC CmftStudio_iem.dds    logo_iem.h    sc_logoIemCompressed

# Textures:
compressedToC LoadingScreen.dds loading_screen.h sc_loadingScreenCompressed
compressedToC SunIcon.dds       sunicon.h        sc_sunIconCompressed
compressedToC Stripes_s.dds     stripes_s.h      sc_stripesSCompressed
compressedToC Bricks_n.dds      brick_n.h        sc_brickNCompressed
compressedToC Bricks_ao.dds     brick_ao.h       sc_brickAoCompressed

# Fonts:
compressedToC DroidSans.ttf     droidsans.h     sc_droidSansCompressed
compressedToC DroidSansMono.ttf droidsansmono.h sc_droidSansMonoCompressed
