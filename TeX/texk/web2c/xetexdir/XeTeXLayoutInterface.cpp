/****************************************************************************\
 Part of the XeTeX typesetting system
 copyright (c) 1994-2005 by SIL International
 written by Jonathan Kew

 This software is distributed under the terms of the Common Public License,
 version 1.0.
 For details, see <http://www.opensource.org/licenses/cpl1.0.php> or the file
 cpl1.0.txt included with the software.
\****************************************************************************/

#include "XeTeXLayoutInterface.h"

#include "XeTeXOTLayoutEngine.h"
#include "XeTeXFontInst.h"
#ifdef XETEX_MAC
#include "XeTeXFontInst_Mac.h"
#include "XeTeXFontManager_Mac.h"
#else
#include "XeTeXFontInst_FC.h"
#include "XeTeXFontManager_FC.h"
#endif

#include "XeTeXswap.h"

#include "Features.h"
#include "ScriptAndLanguage.h"

#include "unicode/ubidi.h"

struct XeTeXLayoutEngine_rec
{
	LayoutEngine*	layoutEngine;
	XeTeXFontInst*	font;
	UInt32			scriptTag;
	UInt32			languageTag;
	UInt32*			addedFeatures;
	UInt32*			removedFeatures;
	UInt32			rgbValue;
};

#ifdef XETEX_MAC
XeTeXFont createFont(ATSFontRef atsFont, Fixed pointSize)
{
	LEErrorCode status = LE_NO_ERROR;
	XeTeXFontInst* font = new XeTeXFontInst_Mac(atsFont, Fix2X(pointSize), status);
	if (LE_FAILURE(status)) {
		delete font;
		return NULL;
	}
	return (XeTeXFont)font;
}

ATSFontRef getFontRef(XeTeXLayoutEngine engine)
{
	return ((XeTeXFontInst_Mac*)(engine->font))->getATSFont();
}

ATSFontRef findFontByName(const char* name, double size)
{
	return (ATSFontRef)(XeTeXFontManager::GetFontManager()->findFont(name, size));
}
#else
XeTeXFont createFont(void* fontRef, Fixed pointSize)
{
	LEErrorCode status = LE_NO_ERROR;
	XeTeXFontInst* font = new XeTeXFontInst_FC((FcPattern*)fontRef, Fix2X(pointSize), status);
	if (LE_FAILURE(status)) {
		delete font;
		return NULL;
	}
	return (XeTeXFont)font;
}

void* findFontByName(const char* name, double size)
{
	return (void*)(XeTeXFontManager::GetFontManager()->findFont(name, size));
}
#endif

void deleteFont(XeTeXFont font)
{
	delete (XeTeXFontInst*)font;
}

void* getFontTablePtr(XeTeXFont font, UInt32 tableTag)
{
	return const_cast<void*>(((XeTeXFontInst*)font)->getFontTable(tableTag));
}

UInt32 countScripts(XeTeXFont font)
{
    const GlyphSubstitutionTableHeader* gsubTable = (const GlyphSubstitutionTableHeader*)((XeTeXFontInst*)font)->getFontTable(kGSUB);
	if (gsubTable == NULL)
		return 0;

	const ScriptListTable* scriptList = (const ScriptListTable*)((const char*)gsubTable + SWAP(gsubTable->scriptListOffset));
	UInt32  scriptCount = SWAP(scriptList->scriptCount);

	return scriptCount;
}

UInt32 getIndScript(XeTeXFont font, UInt32 index)
{
    const GlyphSubstitutionTableHeader* gsubTable = (const GlyphSubstitutionTableHeader*)((XeTeXFontInst*)font)->getFontTable(kGSUB);
	if (gsubTable == NULL)
		return 0;

	const ScriptListTable* scriptList = (const ScriptListTable*)((const char*)gsubTable + SWAP(gsubTable->scriptListOffset));
	if (index < SWAP(scriptList->scriptCount))
		return SWAP(*(UInt32*)(scriptList->scriptRecordArray[index].tag));

	return 0;
}

UInt32 countScriptLanguages(XeTeXFont font, UInt32 script)
{
    const GlyphSubstitutionTableHeader* gsubTable = (const GlyphSubstitutionTableHeader*)((XeTeXFontInst*)font)->getFontTable(kGSUB);
	if (gsubTable == NULL)
		return 0;
	
	const ScriptListTable* scriptList = (const ScriptListTable*)((const char*)gsubTable + SWAP(gsubTable->scriptListOffset));
	const ScriptTable*  scriptTable = scriptList->findScript(script);
	if (scriptTable == NULL)
		return 0;
	
	UInt32  langCount = SWAP(scriptTable->langSysCount);
	
	return langCount;
}

UInt32 getIndScriptLanguage(XeTeXFont font, UInt32 script, UInt32 index)
{
    const GlyphSubstitutionTableHeader* gsubTable = (const GlyphSubstitutionTableHeader*)((XeTeXFontInst*)font)->getFontTable(kGSUB);
	if (gsubTable == NULL)
		return 0;

	const ScriptListTable* scriptList = (const ScriptListTable*)((const char*)gsubTable + SWAP(gsubTable->scriptListOffset));
	const ScriptTable*  scriptTable = scriptList->findScript(script);
	if (scriptTable == NULL)
		return 0;

	if (index < SWAP(scriptTable->langSysCount))
		return SWAP(*(UInt32*)(scriptTable->langSysRecordArray[index].tag));

	return 0;
}

UInt32 countFeatures(XeTeXFont font, UInt32 script, UInt32 language)
{
	UInt32  total = 0;
    const GlyphLookupTableHeader* table;
	for (int i = 0; i < 2; ++i) {
		table = (const GlyphLookupTableHeader*)((XeTeXFontInst*)font)->getFontTable(i == 0 ? kGSUB : kGPOS);
		if (table != NULL) {
			const ScriptListTable* scriptList = (const ScriptListTable*)((const char*)table + table->scriptListOffset);
			const ScriptTable*  scriptTable = scriptList->findScript(script);
			if (scriptTable != NULL) {
				const LangSysTable* langTable = scriptTable->findLanguage(language, (language != 0));
				if (langTable != NULL) {
					total += langTable->featureCount;
					if (langTable->reqFeatureIndex != 0xffff)
						total += 1;
				}
			}
		}
	}
	
	return total;
}

UInt32 getIndFeature(XeTeXFont font, UInt32 script, UInt32 language, UInt32 index)
{
    const GlyphLookupTableHeader* table;
	UInt16  featureIndex = 0xffff;
	for (int i = 0; i < 2; ++i) {
		table = (const GlyphLookupTableHeader*)((XeTeXFontInst*)font)->getFontTable(i == 0 ? kGSUB : kGPOS);
		if (table != NULL) {
			const ScriptListTable* scriptList = (const ScriptListTable*)((const char*)table + SWAP(table->scriptListOffset));
			const ScriptTable*  scriptTable = scriptList->findScript(script);
			if (scriptTable != NULL) {
				const LangSysTable* langTable = scriptTable->findLanguage(language, (language != 0));
				if (langTable != NULL) {
					if (SWAP(langTable->reqFeatureIndex) != 0xffff)
						if (index == 0)
							featureIndex = SWAP(langTable->reqFeatureIndex);
						else
							index -= 1;
					if (index < SWAP(langTable->featureCount))
						featureIndex = langTable->featureIndexArray[index];
					index -= SWAP(langTable->featureCount);
				}
			}
			if (featureIndex != 0xffff) {
				LETag   featureTag;
				const FeatureListTable* featureListTable = (const FeatureListTable*)((const char*)table + SWAP(table->featureListOffset));
				(void)featureListTable->getFeatureTable(featureIndex, &featureTag);
				return featureTag;
			}
		}
	}
	
	return 0;
}

float getGlyphWidth(XeTeXFont font, UInt32 gid)
{
	LEPoint	adv;
	((XeTeXFontInst*)font)->getGlyphAdvance(gid, adv);
	return adv.fX;
}

UInt32
countGlyphs(XeTeXFont font)
{
	return ((XeTeXFontInst*)font)->getNumGlyphs();
}

XeTeXFont getFont(XeTeXLayoutEngine engine)
{
	return (XeTeXFont)(engine->font);
}

char* getFontPSName(XeTeXLayoutEngine engine)
{
	return engine->font->getPSName();
}

XeTeXLayoutEngine createLayoutEngine(XeTeXFont font, UInt32 scriptTag, UInt32 languageTag,
										UInt32* addFeatures, UInt32* removeFeatures, UInt32 rgbValue)
{
	LEErrorCode status = LE_NO_ERROR;
	XeTeXLayoutEngine result = new XeTeXLayoutEngine_rec;
	result->font = (XeTeXFontInst*)font;
	result->scriptTag = scriptTag;
	result->languageTag = languageTag;
	result->addedFeatures = addFeatures;
	result->removedFeatures = removeFeatures;
	result->rgbValue = rgbValue;
	result->layoutEngine = XeTeXOTLayoutEngine::LayoutEngineFactory((XeTeXFontInst*)font,
						scriptTag, languageTag, (LETag*)addFeatures, (LETag*)removeFeatures, status);
	if (LE_FAILURE(status) || result->layoutEngine == NULL) {
		delete result;
		return NULL;
	}
	return result;
}

void deleteLayoutEngine(XeTeXLayoutEngine engine)
{
	delete engine->layoutEngine;
	delete engine->font;
}

SInt32 layoutChars(XeTeXLayoutEngine engine, UInt16 chars[], SInt32 offset, SInt32 count, SInt32 max,
						char rightToLeft, float x, float y, SInt32* status)
{
	LEErrorCode success = (LEErrorCode)*status;
	le_int32	glyphCount = engine->layoutEngine->layoutChars(chars, offset, count, max, rightToLeft, x, y, success);
	*status = success;
	return glyphCount;
}

void getGlyphs(XeTeXLayoutEngine engine, UInt32 glyphs[], SInt32* status)
{
	LEErrorCode success = (LEErrorCode)*status;
	engine->layoutEngine->getGlyphs((LEGlyphID*)glyphs, success);
	*status = success;
}

void getGlyphPositions(XeTeXLayoutEngine engine, float positions[], SInt32* status)
{
	LEErrorCode success = (LEErrorCode)*status;
	engine->layoutEngine->getGlyphPositions(positions, success);
	*status = success;
}

void getGlyphPosition(XeTeXLayoutEngine engine, SInt32 index, float* x, float* y, SInt32* status)
{
	LEErrorCode success = (LEErrorCode)*status;
	engine->layoutEngine->getGlyphPosition(index, *x, *y, success);
	*status = success;
}

UInt32 getScriptTag(XeTeXLayoutEngine engine)
{
	return engine->scriptTag;
}

UInt32 getLanguageTag(XeTeXLayoutEngine engine)
{
	return engine->languageTag;
}

float getPointSize(XeTeXLayoutEngine engine)
{
	return engine->font->getXPixelsPerEm();
}

void getAscentAndDescent(XeTeXLayoutEngine engine, float* ascent, float* descent)
{
	*ascent = engine->font->getExactAscent();
	*descent = engine->font->getExactDescent();
}

UInt32* getAddedFeatures(XeTeXLayoutEngine engine)
{
	return engine->addedFeatures;
}

UInt32* getRemovedFeatures(XeTeXLayoutEngine engine)
{
	return engine->removedFeatures;
}

int getDefaultDirection(XeTeXLayoutEngine engine)
{
	switch (engine->scriptTag) {
		case kArabic:
		case kSyriac:
		case kThaana:
		case kHebrew:
			return UBIDI_DEFAULT_RTL;
	}
	return UBIDI_DEFAULT_LTR;
}

UInt32 getRgbValue(XeTeXLayoutEngine engine)
{
	return engine->rgbValue;
}

