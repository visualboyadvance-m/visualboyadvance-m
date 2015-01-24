#!/bin/bash
#Unfortunately, the svg files don't actually scale well,
#  and some X windows systems still don't like it when they don't have png icons for all the sizes
#  This script allows for auto generation.
#NOTE:  This does not have to be done at every make, ONLY when an icon is changed

#
SIZES=( 16 22 24 32 48 64 96 128 256 )

for asize in ${SIZES[*]}
do

    SIZENAME="$asize"x"$asize"
    OUTDIR="./hicolor/$SIZENAME/apps/"
    OUTFILE="$OUTDIR$vbam.png"

    #Handle really tiny images
    if (( $asize < 32 ))
    then
        asize=32;
    fi;
    INFILE="./src/vbam$asize.svg"

    mkdir -p "$OUTDIR"
    convert "$INFILE" -size "$SIZENAME" "$OUTFILE";
done;

#The largest size becomes the SVG used by the system
LARGESTFILE="./src/vbam""${SIZES[-1]}"".svg"
cp "$LARGESTFILE" ./hicolor/scalable/vbam.svg

#Make nice icons for use by wx
SIZENAME="${SIZES[-1]}"x"${SIZES[-1]}"
convert "$LARGESTFILE" -size "$SIZENAME" vbam.png
convert "$LARGESTFILE" -size "$SIZENAME" wxvbam.icns
convert "$LARGESTFILE" -size "$SIZENAME" wxvbam.xpm
