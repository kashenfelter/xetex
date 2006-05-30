/****************************************************************************\
 Part of the XeTeX typesetting system
 copyright (c) 1994-2005 by SIL International
 written by Jonathan Kew

 This software is distributed under the terms of the Common Public License,
 version 1.0.
 For details, see <http://www.opensource.org/licenses/cpl1.0.php> or the file
 cpl1.0.txt included with the software.
\****************************************************************************/

/* XeTeX_mac.c
 * additional plain C extensions for XeTeX - MacOS-specific routines
 */

#define EXTERN extern
#include "xetexd.h"

#undef input /* this is defined in texmfmp.h, but we don't need it and it confuses the carbon headers */
#include <Carbon/Carbon.h>
#include <Quicktime/Quicktime.h>

#include "TECkit_Engine.h"
#include "XeTeX_ext.h"

#include "XeTeXswap.h"

static ATSUTextLayout	sTextLayout = 0;

static void
InitializeLayout()
{
	OSStatus	status = ATSUCreateTextLayout(&sTextLayout);
	ATSUFontFallbacks fallbacks;
	status = ATSUCreateFontFallbacks(&fallbacks);
	status = ATSUSetObjFontFallbacks(fallbacks, 0, 0, kATSULastResortOnlyFallback);
	ATSUAttributeTag		tag = kATSULineFontFallbacksTag;
	ByteCount				valueSize = sizeof(fallbacks);
	ATSUAttributeValuePtr	value = &fallbacks;
	status = ATSUSetLayoutControls(sTextLayout, 1, &tag, &valueSize, &value);
	status = ATSUSetTransientFontMatching(sTextLayout, true);
}

static inline double
TeXtoPSPoints(double pts)
{
	return pts * 72.0 / 72.27;
}

static inline double
PStoTeXPoints(double pts)
{
	return pts * 72.27 / 72.0;
}

static inline Fixed
FixedTeXtoPSPoints(Fixed pts)
{
	return X2Fix(TeXtoPSPoints(Fix2X(pts)));
}

static inline Fixed
FixedPStoTeXPoints(Fixed pts)
{
	return X2Fix(PStoTeXPoints(Fix2X(pts)));
}

void
DoAtsuiLayout(void* p, int justify)
{
	memoryword*	node = (memoryword*)p;

	unsigned	f = native_font(node);
	if (fontarea[f] != AAT_FONT_FLAG) {
		fprintf(stderr, "internal error: do_atsui_layout called for non-ATSUI font\n");
		exit(1);
	}

	if (sTextLayout == 0)
		InitializeLayout();

	OSStatus	status = noErr;
	
	long		txtLen = native_length(node);
	const UniChar*	txtPtr = (UniChar*)(node + native_node_size);

	status = ATSUSetTextPointerLocation(sTextLayout, txtPtr, 0, txtLen, txtLen);
	
	// we're using this font in AAT mode, so fontlayoutengine[f] is actually an ATSUStyle
	ATSUStyle	style = (ATSUStyle)(fontlayoutengine[native_font(node)]);
	status = ATSUSetRunStyle(sTextLayout, style, 0, txtLen);

	ATSUAttributeTag		tags[] = { kATSULineWidthTag, kATSULineJustificationFactorTag };
	ItemCount				numTags = sizeof(tags) / sizeof(ATSUAttributeTag);
	if (justify) {
		ByteCount				valSizes[] = { sizeof(Fixed), sizeof(Fract) };
		Fixed					wid = FixedTeXtoPSPoints(node_width(node));
		Fract					just = fract1;
		ATSUAttributeValuePtr	valPtrs[] = { &wid, &just };
		status = ATSUSetLayoutControls(sTextLayout, numTags, tags, valSizes, valPtrs);
	}
	
	ItemCount	count;
	ATSLayoutRecord*	layoutRec = NULL;
	status = ATSUDirectGetLayoutDataArrayPtrFromTextLayout(sTextLayout, 0,
		kATSUDirectDataLayoutRecordATSLayoutRecordCurrent, (void*)&layoutRec, &count);

	int i;
	int	realGlyphCount = 0;
	int lastRealGlyph = 0;
	for (i = 0; i < count; ++i)
		if (layoutRec[i].glyphID < 0xfffe) {
			lastRealGlyph = i;
			++realGlyphCount;
		}

	void*		glyph_info = xmalloc(realGlyphCount * native_glyph_info_size);
	FixedPoint*	locations = (FixedPoint*)glyph_info;
	UInt16*		glyphIDs = (UInt16*)(locations + realGlyphCount);

	realGlyphCount = 0;
	for (i = 0; i < count; ++i) {
		if (layoutRec[i].glyphID < 0xfffe) {
			glyphIDs[realGlyphCount] = layoutRec[i].glyphID;
			locations[realGlyphCount].x = FixedPStoTeXPoints(layoutRec[i].realPos);
			locations[realGlyphCount].y = 0;	/* FIXME: won't handle baseline offsets */
			++realGlyphCount;
		}
	}

	native_glyph_count(node) = realGlyphCount;
	native_glyph_info_ptr(node) = (long)glyph_info;
	
	if (!justify)
		node_width(node) = FixedPStoTeXPoints(layoutRec[count-1].realPos);

	ATSUDirectReleaseLayoutDataArrayPtr(NULL, kATSUDirectDataLayoutRecordATSLayoutRecordCurrent, (void*)&layoutRec);

	if (justify)
		ATSUClearLayoutControls(sTextLayout, numTags, tags);
}

static OSStatus QuadraticClosePath(void *callBackDataPtr)
{
	return 0;
}

static OSStatus QuadraticCurve(const Float32Point *pt1, const Float32Point *controlPt, const Float32Point *pt2, void *callBackDataPtr)
{
	GlyphBBox*	data = (GlyphBBox*)callBackDataPtr;
	
	if (pt1->x < data->xMin)
		data->xMin = pt1->x;
	if (pt1->x > data->xMax)
		data->xMax = pt1->x;
	if (pt1->y < data->yMin)
		data->yMin = pt1->y;
	if (pt1->y > data->yMax)
		data->yMax = pt1->y;

	if (pt2->x < data->xMin)
		data->xMin = pt2->x;
	if (pt2->x > data->xMax)
		data->xMax = pt2->x;
	if (pt2->y < data->yMin)
		data->yMin = pt2->y;
	if (pt2->y > data->yMax)
		data->yMax = pt2->y;

	if (controlPt->x < data->xMin)
		data->xMin = controlPt->x;
	if (controlPt->x > data->xMax)
		data->xMax = controlPt->x;
	if (controlPt->y < data->yMin)
		data->yMin = controlPt->y;
	if (controlPt->y > data->yMax)
		data->yMax = controlPt->y;

	return 0;
}

static OSStatus QuadraticLine(const Float32Point *pt1, const Float32Point *pt2, void *callBackDataPtr)
{
	GlyphBBox*	data = (GlyphBBox*)callBackDataPtr;
	
	if (pt1->x < data->xMin)
		data->xMin = pt1->x;
	if (pt1->x > data->xMax)
		data->xMax = pt1->x;
	if (pt1->y < data->yMin)
		data->yMin = pt1->y;
	if (pt1->y > data->yMax)
		data->yMax = pt1->y;

	if (pt2->x < data->xMin)
		data->xMin = pt2->x;
	if (pt2->x > data->xMax)
		data->xMax = pt2->x;
	if (pt2->y < data->yMin)
		data->yMin = pt2->y;
	if (pt2->y > data->yMax)
		data->yMax = pt2->y;

	return 0;
}

static OSStatus QuadraticNewPath(void *callBackDataPtr)
{
	return 0;
}

static OSStatus CubicMoveTo(const Float32Point *pt, void *callBackDataPtr)
{
	GlyphBBox*	data = (GlyphBBox*)callBackDataPtr;
	
	if (pt->x < data->xMin)
		data->xMin = pt->x;
	if (pt->x > data->xMax)
		data->xMax = pt->x;
	if (pt->y < data->yMin)
		data->yMin = pt->y;
	if (pt->y > data->yMax)
		data->yMax = pt->y;

	return 0;
}

static OSStatus CubicLineTo(const Float32Point *pt, void *callBackDataPtr)
{
	GlyphBBox*	data = (GlyphBBox*)callBackDataPtr;
	
	if (pt->x < data->xMin)
		data->xMin = pt->x;
	if (pt->x > data->xMax)
		data->xMax = pt->x;
	if (pt->y < data->yMin)
		data->yMin = pt->y;
	if (pt->y > data->yMax)
		data->yMax = pt->y;

	return 0;
}

static OSStatus CubicCurveTo(const Float32Point *pt1, const Float32Point *pt2, const Float32Point *pt3, void *callBackDataPtr)
{
	GlyphBBox*	data = (GlyphBBox*)callBackDataPtr;

	if (pt1->x < data->xMin)
		data->xMin = pt1->x;
	if (pt1->x > data->xMax)
		data->xMax = pt1->x;
	if (pt1->y < data->yMin)
		data->yMin = pt1->y;
	if (pt1->y > data->yMax)
		data->yMax = pt1->y;

	if (pt2->x < data->xMin)
		data->xMin = pt2->x;
	if (pt2->x > data->xMax)
		data->xMax = pt2->x;
	if (pt2->y < data->yMin)
		data->yMin = pt2->y;
	if (pt2->y > data->yMax)
		data->yMax = pt2->y;

	if (pt3->x < data->xMin)
		data->xMin = pt3->x;
	if (pt3->x > data->xMax)
		data->xMax = pt3->x;
	if (pt3->y < data->yMin)
		data->yMin = pt3->y;
	if (pt3->y > data->yMax)
		data->yMax = pt3->y;

	return 0;
}

static OSStatus CubicClosePath(void *callBackDataPtr)
{
	return 0;
}

void GetGlyphBBox_AAT(ATSUStyle style, UInt16 gid, GlyphBBox* bbox)
	/* returns glyph bounding box in TeX points */
{
	ATSCurveType	curveType;
	OSStatus		status;

	bbox->xMin = 65536.0;
	bbox->yMin = 65536.0;
	bbox->xMax = -65536.0;
	bbox->yMax = -65536.0;

	status = ATSUGetNativeCurveType(style, &curveType);
	if (status == noErr) {
		OSStatus	cbStatus;

		if (curveType == kATSCubicCurveType) {
			static ATSCubicMoveToUPP cubicMoveToProc;
			static ATSCubicLineToUPP cubicLineToProc;
			static ATSCubicCurveToUPP cubicCurveToProc;
			static ATSCubicClosePathUPP cubicClosePathProc;
			if (cubicMoveToProc == NULL) {
				cubicMoveToProc = NewATSCubicMoveToUPP(&CubicMoveTo);
				cubicLineToProc = NewATSCubicLineToUPP(&CubicLineTo);
				cubicCurveToProc = NewATSCubicCurveToUPP(&CubicCurveTo);
				cubicClosePathProc = NewATSCubicClosePathUPP(&CubicClosePath);
			}
			status = ATSUGlyphGetCubicPaths(style, gid,
						cubicMoveToProc, cubicLineToProc, cubicCurveToProc, cubicClosePathProc, 
						bbox, &cbStatus);
		}
		else {
			static ATSQuadraticNewPathUPP quadraticNewPathProc;
			static ATSQuadraticLineUPP quadraticLineProc;
			static ATSQuadraticCurveUPP quadraticCurveProc;
			static ATSQuadraticClosePathUPP quadraticClosePathProc;
			if (quadraticNewPathProc == NULL) {
				quadraticNewPathProc = NewATSQuadraticNewPathUPP(&QuadraticNewPath);
				quadraticLineProc = NewATSQuadraticLineUPP(&QuadraticLine);
				quadraticCurveProc = NewATSQuadraticCurveUPP(&QuadraticCurve);
				quadraticClosePathProc = NewATSQuadraticClosePathUPP(&QuadraticClosePath);
			}
			status = ATSUGlyphGetQuadraticPaths(style, gid,
						quadraticNewPathProc, quadraticLineProc, quadraticCurveProc, quadraticClosePathProc,
						bbox, &cbStatus);
		}
	}

	if (status != noErr || bbox->xMin == 65536.0)
		bbox->xMin = bbox->yMin = bbox->xMax = bbox->yMax = 0;
	else {
		// convert PS to TeX points and flip y-axis
		float	tmp = bbox->yMin;
		bbox->yMin = -bbox->yMax * 72.27 / 72.0;
		bbox->yMax = -tmp * 72.27 / 72.0;
		bbox->xMin *= 72.27 / 72.0;
		bbox->xMax *= 72.27 / 72.0;
	}
}

void GetGlyphHeightDepth_AAT(ATSUStyle style, UInt16 gid, float* ht, float* dp)
	/* returns TeX points */
{
	GlyphBBox	bbox;
	
	GetGlyphBBox_AAT(style, gid, &bbox);

	*ht = bbox.yMax;
	*dp = -bbox.yMin;
}

void GetGlyphSidebearings_AAT(ATSUStyle style, UInt16 gid, float* lsb, float* rsb)
	/* returns TeX points */
{
	ATSGlyphIdealMetrics	metrics;
	OSStatus	status = ATSUGlyphGetIdealMetrics(style, 1, &gid, 0, &metrics);
#if 0
	*lsb = metrics.sideBearing.x;
	*rsb = metrics.otherSideBearing.x;
#else
	GlyphBBox	bbox;
	GetGlyphBBox_AAT(style, gid, &bbox);
	*lsb = bbox.xMin;
	*rsb = bbox.xMax - PStoTeXPoints(metrics.advance.x);
#endif
}

float GetGlyphItalCorr_AAT(ATSUStyle style, UInt16 gid)
{
	float	rval = 0.0;
	ATSGlyphIdealMetrics	metrics;
	OSStatus	status = ATSUGlyphGetIdealMetrics(style, 1, &gid, 0, &metrics);
#if 0
	/* this doesn't seem to work for OT/CFF fonts */
	if (metrics.otherSideBearing.x > 0.0)
		rval = metrics.otherSideBearing.x;
#else
	GlyphBBox	bbox;
	GetGlyphBBox_AAT(style, gid, &bbox);
	if (bbox.xMax > metrics.advance.x)
		rval = bbox.xMax - PStoTeXPoints(metrics.advance.x);
#endif
	return rval;
}

int MapCharToGlyph_AAT(ATSUStyle style, UInt32 ch)
{
	UniChar	txt[2];
	int		len = 1;
	
	if (sTextLayout == 0)
		InitializeLayout();

	if (ch > 0xffff) {
		ch -= 0x10000;
		txt[0] = 0xd800 + ch / 1024;
		txt[1] = 0xdc00 + ch % 1024;
		len = 2;
	}
	else
		txt[0] = ch;

	OSStatus	status = ATSUSetTextPointerLocation(sTextLayout, &txt[0], 0, len, len);
	status = ATSUSetRunStyle(sTextLayout, style, 0, len);
	
	ByteCount	bufferSize = sizeof(ATSUGlyphInfoArray);
	ATSUGlyphInfoArray	info;
	status = ATSUGetGlyphInfo(sTextLayout, 0, len, &bufferSize, &info);
	if (bufferSize > 0 && info.numGlyphs > 0)
		return info.glyphs[0].glyphID;
	else
		return 0;
}

ATSUFontVariationAxis
find_axis_by_name(ATSUFontID fontID, const char* name, int nameLength)
{
	ATSUFontVariationAxis	result = 0;

	Str255	inName;
	inName[0] = nameLength;
	int i;
	for (i = 0; i < inName[0]; ++i)
		inName[i + 1] = name[i];

	ItemCount	varCount;
	ATSUCountFontVariations(fontID, &varCount);
	if (varCount > 0) {
		for (i = 0; i < varCount; ++i) {
			ATSUFontVariationAxis	axis;
			ATSUGetIndFontVariation(fontID, i, &axis, 0, 0, 0);
			FontNameCode	nameCode;
			ATSUGetFontVariationNameCode(fontID, axis, &nameCode);
			Str255		name;
			ByteCount	nameLen;
			ATSUFindFontName(fontID, nameCode, kFontMacintoshPlatform, kFontNoScript, kFontNoLanguage,
                                 255, (Ptr)&name[1], &nameLen, 0);
			name[0] = nameLen;
			if (EqualString(inName, name, false, true)) {
				result = axis;
				break;
			}
		}
	}
	
	return result;
}

ATSUFontFeatureType
find_feature_by_name(ATSUFontID fontID, const char* name, int nameLength)
{
	ATSUFontFeatureType	result = 0x0000FFFF;

	Str255	inName;
	inName[0] = nameLength;
	int i;
	for (i = 0; i < inName[0]; ++i)
		inName[i + 1] = name[i];

	ItemCount	typeCount;
	ATSUCountFontFeatureTypes(fontID, &typeCount);
	if (typeCount > 0) {
		ATSUFontFeatureType*	types = (ATSUFontFeatureType*)xmalloc(typeCount * sizeof(ATSUFontFeatureType));
		ATSUGetFontFeatureTypes(fontID, typeCount, types, 0);
		for (i = 0; i < typeCount; ++i) {
			FontNameCode	nameCode;
			ATSUGetFontFeatureNameCode(fontID, types[i], 0x0000FFFF, &nameCode);
			Str255		name;
			ByteCount	nameLen;
			ATSUFindFontName(fontID, nameCode, kFontMacintoshPlatform, kFontNoScript, kFontNoLanguage,
                                 255, (Ptr)&name[1], &nameLen, 0);
			name[0] = nameLen;
			if (EqualString(inName, name, false, true)) {
				result = types[i];
				break;
			}
		}
		free((char*)types);
	}
	
	return result;
}

ATSUFontFeatureSelector
find_selector_by_name(ATSUFontID fontID, ATSUFontFeatureType featureType, const char* name, int nameLength)
{
	ATSUFontFeatureSelector	result = 0x0000FFFF;

	Str255	inName;
	inName[0] = nameLength;
	int i;
	for (i = 0; i < inName[0]; ++i)
		inName[i + 1] = name[i];

	ItemCount	selectorCount;
	ATSUCountFontFeatureSelectors(fontID, featureType, &selectorCount);
	if (selectorCount > 0) {
		ATSUFontFeatureSelector*	selectors = (ATSUFontFeatureSelector*)xmalloc(selectorCount * sizeof(ATSUFontFeatureSelector));
		ATSUGetFontFeatureSelectors(fontID, featureType, selectorCount, selectors, 0, 0, 0);
		for (i = 0; i < selectorCount; ++i) {
			FontNameCode	nameCode;
			ATSUGetFontFeatureNameCode(fontID, featureType, selectors[i], &nameCode);
			Str255		name;
			ByteCount	nameLen;
			ATSUFindFontName(fontID, nameCode, kFontMacintoshPlatform, kFontNoScript, kFontNoLanguage,
                                 255, (Ptr)&name[1], &nameLen, 0);
			name[0] = nameLen;
			if (EqualString(inName, name, false, true)) {
				result = selectors[i];
				break;
			}
		}
		free((char*)selectors);
	}
	
	return result;
}

void*
loadAATfont(ATSFontRef fontRef, long scaled_size, const char* cp1)
{
	ATSUFontID	fontID = FMGetFontFromATSFontRef(fontRef);
	ATSUStyle	style = 0;
	OSStatus	status = ATSUCreateStyle(&style);
	if (status == noErr) {
		UInt32	rgbValue;
		Fixed	atsuSize = FixedTeXtoPSPoints(scaled_size);
		
		ATSStyleRenderingOptions	options = kATSStyleNoHinting;
		Fract						hangInhibit = fract1;
		ATSUAttributeTag		tags[] = { kATSUFontTag, kATSUSizeTag, kATSUStyleRenderingOptionsTag, kATSUHangingInhibitFactorTag };
		ByteCount				sizes[] = { sizeof(ATSUFontID), sizeof(Fixed), sizeof(ATSStyleRenderingOptions), sizeof(Fract) };
		ATSUAttributeValuePtr	attrs[] = { &fontID, &atsuSize, &options, &hangInhibit };
		ATSUSetAttributes(style, sizeof(tags) / sizeof(ATSUAttributeTag), tags, sizes, attrs);
		
#define FEAT_ALLOC_CHUNK	8
#define VAR_ALLOC_CHUNK		4

		if (cp1 != NULL) {
			int	allocFeats = FEAT_ALLOC_CHUNK;
			UInt16*	featureTypes = (UInt16*)xmalloc(allocFeats * sizeof(UInt16));
			UInt16*	selectorValues = (UInt16*)xmalloc(allocFeats * sizeof(UInt16));
			int	numFeatures = 0;
			
			int	allocVars = VAR_ALLOC_CHUNK;
			UInt32*	axes = (UInt32*)xmalloc(allocVars * sizeof(UInt32));
			SInt32*	values = (SInt32*)xmalloc(allocVars * sizeof(SInt32));
			int	numVariations = 0;
			
			// interpret features & variations following ":"
			while (*cp1) {
				// locate beginning of name=value pair
				if (*cp1 == ':' || *cp1 == ';')	// skip over separator
					++cp1;
				while (*cp1 == ' ' || *cp1 == '\t')	// skip leading whitespace
					++cp1;
				if (*cp1 == 0)	// break if end of string
					break;
	
				// scan to end of pair
				const char*	cp2 = cp1;
				while (*cp2 && *cp2 != ';' && *cp2 != ':')
					++cp2;
	
				// look for the '=' separator
				const char*	cp3 = cp1;
				while (cp3 < cp2 && *cp3 != '=')
					++cp3;
				if (cp3 == cp2)
					goto bad_option;
	
				// now cp1 points to option name, cp3 to '=', cp2 to ';' or null
				
				// first try for a feature by this name
				ATSUFontFeatureType	featureType;
				featureType = find_feature_by_name(fontID, cp1, cp3 - cp1);
				if (featureType != 0x0000FFFF) {
					// look past the '=' separator for setting names
					int	featLen = cp3 - cp1;
					++cp3;
					while (cp3 < cp2) {
						// skip leading whitespace
						while (*cp3 == ' ' || *cp3 == '\t')
							++cp3;
					
						// possibly multiple settings...
						int	disable = 0;
						if (*cp3 == '!') {	// check for negation
							disable = 1;
							++cp3;
						}
						
						// scan for end of setting name
						const char*	cp4 = cp3;
						while (cp4 < cp2 && *cp4 != ',')
							++cp4;
						
						// now cp3 points to name, cp4 to ',' or ';' or null
						ATSUFontFeatureSelector	selectorValue = find_selector_by_name(fontID, featureType, cp3, cp4 - cp3);
						if (selectorValue != 0x0000FFFF) {
							if (numFeatures == allocFeats) {
								allocFeats += FEAT_ALLOC_CHUNK;
								featureTypes = xrealloc(featureTypes, allocFeats * sizeof(UInt16));
								selectorValues = xrealloc(selectorValues, allocFeats * sizeof(UInt16));
							}
							featureTypes[numFeatures] = featureType;
							selectorValues[numFeatures] = selectorValue + disable;
							++numFeatures;
						}
						else {
							fontfeaturewarning(cp1, featLen, cp3, cp4 - cp3);
						}
						
						// point beyond setting name terminator
						cp3 = cp4 + 1;
					}
					
					goto next_option;
				}
				
				// try to find a variation by this name
				ATSUFontVariationAxis	axis;
				axis = find_axis_by_name(fontID, cp1, cp3 - cp1);
				if (axis != 0) {
					// look past the '=' separator for the value
					++cp3;
					double	value = 0.0, decimal = 1.0;
					bool		negate = false;
					if (*cp3 == '-') {
						++cp3;
						negate = true;
					}
					while (cp3 < cp2) {
						int	v = *cp3 - '0';
						if (v >= 0 && v <= 9) {
							if (decimal != 1.0) {
								value += v / decimal;
								decimal *= 10.0;
							}
							else
								value = value * 10.0 + v;
						}
						else if (*cp3 == '.') {
							if (decimal != 1.0)
								break;
							decimal = 10.0;
						}
						else
							break;
						++cp3;
					}
					if (negate)
						value = -value;
					if (numVariations == allocVars) {
						allocVars += VAR_ALLOC_CHUNK;
						axes = xrealloc(axes, allocVars * sizeof(UInt32));
						values = xrealloc(values, allocVars * sizeof(SInt32));
					}
					axes[numVariations] = axis;
					values[numVariations] = value * 65536.0;	//	X2Fix(value);
					++numVariations;
					
					goto next_option;
				}
				
				// didn't find feature or variation, try other options....
	
				if (strncmp(cp1, "mapping", 7) == 0) {
					cp3 = cp1 + 7;
					if (*cp3 != '=')
						goto bad_option;
					loadedfontmapping = load_mapping_file(cp3 + 1, cp2);
					goto next_option;
				}
				
				if (strncmp(cp1, "color", 5) == 0) {
					cp3 = cp1 + 5;
					if (*cp3 != '=')
						goto bad_option;
					++cp3;
					rgbValue = 0;
					unsigned	alpha = 0;
					int i;
					for (i = 0; i < 6; ++i) {
						if (*cp3 >= '0' && *cp3 <= '9')
							rgbValue = (rgbValue << 4) + *cp3 - '0';
						else if (*cp3 >= 'A' && *cp3 <= 'F')
							rgbValue = (rgbValue << 4) + *cp3 - 'A' + 10;
						else if (*cp3 >= 'a' && *cp3 <= 'f')
							rgbValue = (rgbValue << 4) + *cp3 - 'a' + 10;
						else
							goto bad_option;
						++cp3;
					}
					rgbValue <<= 8;
					for (i = 0; i < 2; ++i) {
						if (*cp3 >= '0' && *cp3 <= '9')
							alpha = (alpha << 4) + *cp3 - '0';
						else if (*cp3 >= 'A' && *cp3 <= 'F')
							alpha = (alpha << 4) + *cp3 - 'A' + 10;
						else if (*cp3 >= 'a' && *cp3 <= 'f')
							alpha = (alpha << 4) + *cp3 - 'a' + 10;
						else
							break;
						++cp3;
					}
					if (i == 2)
						rgbValue += alpha;
					else
						rgbValue += 0xFF;
					
					loadedfontflags |= FONT_FLAGS_COLORED;
					
					goto next_option;
				}
				
			bad_option:
				// not a name=value pair, or not recognized.... 
				// check for plain "vertical" before complaining
				if (strncmp(cp1, "vertical", 8) == 0) {
					cp3 = cp2;
					if (*cp3 == ';' || *cp3 == ':')
						--cp3;
					while (*cp3 == ' ' || *cp3 == '\t')
						--cp3;
					if (*cp3)
						++cp3;
					if (cp3 == cp1 + 8) {
						ATSUVerticalCharacterType	vert = kATSUStronglyVertical;
						tags[0] = kATSUVerticalCharacterTag;
						sizes[0] = sizeof(ATSUVerticalCharacterType);
						attrs[0] = &vert;
						ATSUSetAttributes(style, 1, tags, sizes, attrs);
						goto next_option;
					}
				}
			
				fontfeaturewarning(cp1, cp2 - cp1, 0, 0);
				
			next_option:
				// go to next name=value pair
				cp1 = cp2;
			}
		
			if (numFeatures > 0)
				ATSUSetFontFeatures(style, numFeatures, featureTypes, selectorValues);
	
			if (numVariations > 0)
				ATSUSetVariations(style, numVariations, axes, values);
	
			if ((loadedfontflags & FONT_FLAGS_COLORED) != 0) {
				ATSURGBAlphaColor	rgba;
				rgba.red	= ((rgbValue & 0xFF000000) >> 24) / 255.0;
				rgba.green	= ((rgbValue & 0x00FF0000) >> 16) / 255.0;
				rgba.blue	= ((rgbValue & 0x0000FF00) >> 8 ) / 255.0;
				rgba.alpha	= ((rgbValue & 0x000000FF)      ) / 255.0;
				tags[0] = kATSURGBAlphaColorTag;
				sizes[0] = sizeof(ATSURGBAlphaColor);
				attrs[0] = &rgba;
				ATSUSetAttributes(style, 1, tags, sizes, attrs);
			}
			
			free((char*)featureTypes);
			free((char*)selectorValues);
			free((char*)axes);
			free((char*)values);
		}
	}

	nativefonttypeflag = AAT_FONT_FLAG;
	return style;
}

OSErr
find_pic_file(char** path, realrect* bounds, int isPDF, int page)
{
	*path = NULL;

	OSStatus	result = fnfErr;
    char*		pic_path = kpse_find_file((char*)nameoffile + 1, kpse_pict_format, 1);
	CFURLRef	picFileURL = NULL;
	if (pic_path) {
		picFileURL = CFURLCreateFromFileSystemRepresentation(kCFAllocatorDefault, (UInt8*)pic_path, strlen(pic_path), false);

		if (picFileURL != NULL) {
			/* get an FSRef for the URL we constructed */
			FSRef	picFileRef;
			CFURLGetFSRef(picFileURL, &picFileRef);
			
			if (isPDF) {
				CGPDFDocumentRef	document = CGPDFDocumentCreateWithURL(picFileURL);
				if (document != NULL) {
					CGRect	mediaBox;
					int	nPages = CGPDFDocumentGetNumberOfPages(document);
					if (page < 0)
						page = nPages + 1 + page;
					if (page < 1)
						page = 1;
					if (page > nPages)
						page = nPages;
					CGRect r = CGPDFDocumentGetMediaBox(document, page);
					bounds->x = r.origin.x;
					bounds->y = r.origin.y;
					bounds->wd = r.size.width;
					bounds->ht = r.size.height;
					CGPDFDocumentRelease(document);
					result = noErr;
				}
			}
			else {
				/* make an FSSpec for the file, and try to import it as a pic */
				FSSpec	picFileSpec;
				result = FSGetCatalogInfo(&picFileRef, kFSCatInfoNone, NULL, NULL, &picFileSpec, NULL);
				if (result == noErr) {
					ComponentInstance	ci;
					result = GetGraphicsImporterForFile(&picFileSpec, &ci);
					if (result == noErr) {
						ImageDescriptionHandle	desc = NULL;
						result = GraphicsImportGetImageDescription(ci, &desc);
						bounds->x = 0;
						bounds->y = 0;
						bounds->wd = (*desc)->width * 72.0 / Fix2X((*desc)->hRes);
						bounds->ht = (*desc)->height * 72.0 / Fix2X((*desc)->vRes);
						DisposeHandle((Handle)desc);
						(void)CloseComponent(ci);
					}
				}
			}
			
			CFRelease(picFileURL);
		}

		/* if we couldn't import it, toss the pathname as we'll be returning an error */
		if (result != noErr)
			free(pic_path);
		else
			*path = (char*)pic_path;
	}
	
	return result;
}