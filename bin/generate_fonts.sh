#!/bin/bash

abs_path () {
  echo "$(cd -- "$(dirname -- "$1")"; pwd)/$(basename -- "$1")"
}

BIN_DIR="$(abs_path "`dirname $0`")"
OUT_DIR="$(abs_path "$BIN_DIR/../src/roo_fonts")"

SIZES='8,10,12,15,18,27,40,60,90'

FONTS=(
  "NotoSansMono-Regular"
  "NotoSans-Regular"
  "NotoSans-Bold"
  "NotoSerif-Italic"
)

mkdir -p ${OUT_DIR}

(cd ${BIN_DIR};
git clone https://github.com/dejwk/roo_display_font_importer.git;
cd roo_display_font_importer;
git checkout v2;

for font in "${FONTS[@]}"; do
  ./gradlew run --args="-font=$font -sizes=$SIZES -output-dir=$OUT_DIR"
done
)
