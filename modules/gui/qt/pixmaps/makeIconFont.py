#! /usr/bin/env python3
# -*- coding: utf-8 -*-
import sys
import json
import fontforge
import os
import re

DOC= """
transform a batch of SVG to a font usable in QML. The glyphs can be referred directly based on their names,
which requires support for ligatures. Any non standard ASCII letter is to be encoded to their respective
glpyh name specified in Adobe Latin 1: "_" becomes "underscore", "1" becomes "one"...

known issues:

  - SVG should not have overlapping
  - SVG should have a viewbox equal to the size of the SVG, ie:
         <svg width="48" height="48" viewBox="0 0 48 48">

Input files must respect the format described by CONFIG_SCHEMA
"""

CONFIG_SCHEMA = {
    "type": "object",
    "properties": {
	"font_file": { "type" : "string", "description" : "output font file" },
	"font_name": { "type" : "string", "description": "font family name" },
	"glyphs" : {
            "type" : "array",
            "items" : {
                "type": "object",
                "properties" : {
                    "key" : { "type" : "string", "description": "keyword of the glyph, should be unique within the list" },
                    "path" : { "type" : "string", "description" : "path of the SVG to use for glyph" },
                    "charcode" : { "type" : "string", "description": "utf8 code" },
                },
                "required": ["key", "path"]
            }
        }
    },
    "required": ["glyphs", "font_name", "font_file"]
}

UTF8_AREA = 0xE000

def validateModel(model):
    try:
        import jsonschema
    except ImportError:
        return True

    jsonschema.validate(model, CONFIG_SCHEMA)
    return True

def extractParanthesesSubString(string):
    return re.search(r'\((.+)\)', string).group(1)

def main(model_fd):
    if model_fd.name.endswith(".json"):
        data = json.load(model_fd)
        if not data:
            return
    else:
        # Any text file containing the following may be used as a model, as an alternative
        # to JSON. In that case, paths are relative to the model and not to the current
        # directory. If there are multiple `FONT_FILE`
        #
        # FONT_FILE(file) : relative file path must point to the ttf file.
        # FONT_NAME(name) : name of the font, font family.
        # FONT_ENTRY(name, path) : name of the icon, and relative svg path to the icon.
        #
        # If there are multiple `FONT_FILE` or `FONT_NAME`, only the last one is considered.
        # Naturally, there can be multiple `FONT_ENTRY` entries.
        # The default model file for VLC is in `style/vlcicons.hpp`.
        data = {}
        modelDirectory = os.path.dirname(model_fd.name)
        glyphs = []
        for line in model_fd:
            line = line.strip()
            if (line.startswith("FONT_FILE")):
                fontFile = extractParanthesesSubString(line).strip("\" ")
                data["font_file"] = os.path.abspath(os.path.join(modelDirectory, fontFile))
                print("FONT_FILE:", data["font_file"])
            elif (line.startswith("FONT_NAME")):
                data["font_name"] = extractParanthesesSubString(line).strip("\" ")
                print("FONT_NAME:", data["font_name"])
            elif (line.startswith("FONT_ENTRY")):
                parsed = extractParanthesesSubString(line).strip().split(',')
                name = parsed[0].strip()
                path = os.path.abspath(os.path.join(modelDirectory, parsed[1].strip("\" ")))
                glyph = {"key": name, "path": path}
                glyphs.append(glyph)
                print("FONT_ENTRY:", glyph)
        data["glyphs"] = glyphs

    validateModel(data)

    font = fontforge.font()
    font.familyname = data["font_name"]
    font.fontname = data["font_name"]
    font.design_size = 1024.0

    font.hasvmetrics = True

    font.upos=0
    font.ascent = 1024
    font.descent = 0

    font.hhea_ascent = 1024
    font.hhea_ascent_add = False
    font.hhea_descent = 0
    font.hhea_descent_add = False
    font.hhea_linegap = 0

    font.os2_use_typo_metrics = True
    font.os2_typoascent = 1024
    font.os2_typoascent_add = False
    font.os2_typodescent = 0
    font.os2_typodescent_add = False
    font.os2_typolinegap = 0

    font.os2_winascent = 1024
    font.os2_winascent_add = False
    font.os2_windescent = 0
    font.os2_windescent_add = False

    # Create empty characters for Aa-Zz:
    for i in list(range(65, 91)) + list(range(97,123)):
        char = font.createChar(i)
        char.width = 0
        char = None

    # https://fontforge.org/docs/scripting/python/fontforge.html#fontforge.font.addLookup
    ligatureSubTable = "ligature-1"
    font.addLookup('ligature', 'gsub_ligature', (), (('liga', (('latn', ('dflt')), )), ))
    font.addLookupSubtable('ligature', ligatureSubTable)

    createdChars = set()

    for i, glyph in enumerate(data["glyphs"]):
        name = glyph["key"]

        assert name not in createdChars, f"Icons can not have the same name ('{name}' used multiple times)!"
        createdChars.add(name)

        # Some common symbols are listed here, if you get the following error:
        # "Lookup subtable contains unused glyph <glyph> making the whole subtable invalid"
        # Add the glyph here, based on Adobe Latin 1 character set:
        # https://adobe-type-tools.github.io/adobe-latin-charsets/adobe-latin-1.html
        name = name.translate(str.maketrans({'_': 'underscore',
                                             '-': 'hyphen',
                                             '1': 'one',
                                             '2': 'two',
                                             '3': 'three',
                                             '4': 'four',
                                             '5': 'five',
                                             '6': 'six',
                                             '7': 'seven',
                                             '8': 'eight',
                                             '9': 'nine'}))

        charcode = UTF8_AREA + i
        c = font.createChar(charcode, name)
        c.addPosSub(ligatureSubTable, tuple(name))
        glyph["charcode"]  = "\\u{:x}".format(charcode)
        c.importOutlines(glyph["path"])
        #scale glyph to fit between 200 (base line) and 800 (x 0.6)
        #c.transform((0.6, 0.0, 0.0, 0.6, 200, 200.))
        c.vwidth = 1024
        c.width = 1024

    font.generate(data["font_file"])

if __name__ == "__main__":
    import argparse
    parser = argparse.ArgumentParser(description="generate an icon font for QML from SVG")
    parser.add_argument("model", metavar="model",type=argparse.FileType("r"), default=sys.stdin,
                        help="the input model")
    args = parser.parse_args()
    main(args.model)
