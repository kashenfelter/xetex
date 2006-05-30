/****************************************************************************\
 Part of the XeTeX typesetting system
 copyright (c) 1994-2005 by SIL International
 written by Jonathan Kew

 This software is distributed under the terms of the Common Public License,
 version 1.0.
 For details, see <http://www.opensource.org/licenses/cpl1.0.php> or the file
 cpl1.0.txt included with the software.
\****************************************************************************/

#ifdef XETEX_MAC
#include <Carbon/Carbon.h>
#endif

#include "XeTeX_ext.h"
#include "XeTeXFontMgr.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct XeTeXFont_rec* XeTeXFont;
typedef struct XeTeXLayoutEngine_rec* XeTeXLayoutEngine;

extern char	gPrefEngine;

int getCachedGlyphBBox(UInt16 fontID, UInt16 glyphID, GlyphBBox* bbox);
void cacheGlyphBBox(UInt16 fontID, UInt16 glyphID, const GlyphBBox* bbox);

void terminatefontmanager();

#ifdef XETEX_MAC
XeTeXFont createFont(ATSFontRef atsFont, Fixed pointSize);
#else
// appropriate functions for other platforms
XeTeXFont createFont(PlatformFontRef fontRef, Fixed pointSize);
#endif

PlatformFontRef getFontRef(XeTeXLayoutEngine engine);
PlatformFontRef findFontByName(const char* name, char* var, double size);

char getReqEngine();
const char* getFullName(PlatformFontRef fontRef);
const char* getPSName(PlatformFontRef fontRef);
void getNames(PlatformFontRef fontRef, const char** psName, const char** famName, const char** styName);

void deleteFont(XeTeXFont font);

void* getFontTablePtr(XeTeXFont font, UInt32 tableTag);

Fixed getSlant(XeTeXFont font);

UInt32 countScripts(XeTeXFont font);
UInt32 getIndScript(XeTeXFont font, UInt32 index);
UInt32 countScriptLanguages(XeTeXFont font, UInt32 script);
UInt32 getIndScriptLanguage(XeTeXFont font, UInt32 script, UInt32 index);
UInt32 countFeatures(XeTeXFont font, UInt32 script, UInt32 language);
UInt32 getIndFeature(XeTeXFont font, UInt32 script, UInt32 language, UInt32 index);
float getGlyphWidth(XeTeXFont font, UInt32 gid);
UInt32 countGlyphs(XeTeXFont font);

XeTeXLayoutEngine createLayoutEngine(XeTeXFont font, UInt32 scriptTag, UInt32 languageTag,
						UInt32* addFeatures, UInt32* removeFeatures, UInt32 rgbValue);

void deleteLayoutEngine(XeTeXLayoutEngine engine);

XeTeXFont getFont(XeTeXLayoutEngine engine);

SInt32 layoutChars(XeTeXLayoutEngine engine, UInt16* chars, SInt32 offset, SInt32 count, SInt32 max,
						char rightToLeft, float x, float y, SInt32* status);

void getGlyphs(XeTeXLayoutEngine engine, UInt32* glyphs, SInt32* status);

void getGlyphPositions(XeTeXLayoutEngine engine, float* positions, SInt32* status);

void getGlyphPosition(XeTeXLayoutEngine engine, SInt32 index, float* x, float* y, SInt32* status);

UInt32 getScriptTag(XeTeXLayoutEngine engine);

UInt32 getLanguageTag(XeTeXLayoutEngine engine);

float getPointSize(XeTeXLayoutEngine engine);

void getAscentAndDescent(XeTeXLayoutEngine engine, float* ascent, float* descent);

UInt32* getAddedFeatures(XeTeXLayoutEngine engine);

UInt32* getRemovedFeatures(XeTeXLayoutEngine engine);

int getDefaultDirection(XeTeXLayoutEngine engine);

UInt32 getRgbValue(XeTeXLayoutEngine engine);

void getGlyphBounds(XeTeXLayoutEngine engine, UInt32 glyphID, GlyphBBox* bbox);

void getGlyphHeightDepth(XeTeXLayoutEngine engine, UInt32 glyphID, float* height, float* depth);

void getGlyphSidebearings(XeTeXLayoutEngine engine, UInt32 glyphID, float* lsb, float* rsb);

float getGlyphItalCorr(XeTeXLayoutEngine engine, UInt32 glyphID);

UInt32 mapCharToGlyph(XeTeXLayoutEngine engine, UInt32 charCode);

#ifdef __cplusplus
};
#endif