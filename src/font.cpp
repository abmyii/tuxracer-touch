/* --------------------------------------------------------------------
EXTREME TUXRACER

Copyright (C) 1999-2001 Jasmin F. Patry (Tuxracer)
Copyright (C) 2004-2005 Volker Stroebel (Planetpenguin Racer)
Copyright (C) 2010 Extreme Tuxracer Team

This program is free software; you can redistribute it and/or
modify it under the terms of the GNU General Public License
as published by the Free Software Foundation; either version 2
of the License, or (at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.
---------------------------------------------------------------------*/

#ifdef HAVE_CONFIG_H
#include <etr_config.h>
#endif

#include "font.h"
#include "ft_font.h"
#include "spx.h"
#include "ogl.h"
#include "winsys.h"

#define USE_UNICODE 1

// --------------------------------------------------------------------
// First some common function used for textboxes and called by
// CFont::MakeLineList. This bundle of functions generates
// a vector<string> from a textstring and adapts the lines to the textbox

static void MakeWordList (vector<string>& wordlist, const char *s) {
	size_t start = 0;
	for (size_t i = 0; s[i] != '\0'; i++) {
		if (s[i] == ' ') {
			if (i != start)
				wordlist.push_back(string(s+start, i-start));
			while (s[i+1] == ' ')
				i++;
			start = i+1;
		}
	}
	if (s[start] != '0')
		wordlist.push_back(string(s+start));
}

static size_t MakeLine (size_t first, const vector<string>& wordlist, vector<string>& linelist, float width) {
	if (first >= wordlist.size()) return wordlist.size()-1;

	size_t last = first;
	float lng = 0;

	float spacelng = FT.GetTextWidth("a a") - FT.GetTextWidth("aa");;
	while (last < wordlist.size()) {
		float wordlng = FT.GetTextWidth(wordlist[last]);
		lng += wordlng;
		lng += spacelng;
		if (lng >= width && first != last) // If first == last, we write beyond line
			break;
		last++;
	}

	string line;
	for (size_t j=first; j<last; j++) {
		line += wordlist[j];
		if (j < last)
			line += ' ';
	}
	linelist.push_back(line);
	return last-1;
}



// --------------------------------------------------------------------
//				CFont
// --------------------------------------------------------------------

CFont FT;

CFont::CFont () {
	forientation = OR_TOP;

	// setting default values
	curr_col.r = 0.0;	// default color: black
	curr_col.g = 0.0;
	curr_col.b = 0.0;
	curr_col.a = 1.0;	// default: no transparency
	curr_size   = 20;	// default size: 20 px
	curr_fact   = 0;
	curr_font   = 0;
}

CFont::~CFont() {
	Clear();
}

void CFont::Clear () {
	for(size_t i = 0; i < fonts.size(); i++)
	{
		map<float,FTFont*>::iterator it = fonts[i]->fonts.begin();
		while (it != fonts[i]->fonts.end())
		{
			delete it->second;
			it++;
		}
	}
	fonts.clear();
	fontindex.clear();
}

// --------------------------------------------------------------------
//				private
// --------------------------------------------------------------------

wstring CFont::UnicodeStr (const char *s) {
	size_t len = strlen (s);
	wstring res;
	res.resize(len);

	for (size_t i=0, j=0; i < len; ++i, ++j) {
		wchar_t ch = ((const unsigned char *)s)[i];
		if (ch >= 0xF0) {
			ch  =  (wchar_t) (s[i] & 0x07)   << 18;
			ch |=  (wchar_t) (s[++i] & 0x3F) << 12;
			ch |=  (wchar_t) (s[++i] & 0x3F) << 6;
			ch |=  (wchar_t) (s[++i] & 0x3F);
		} else if (ch >= 0xE0) {
			ch  =  (wchar_t) (s[i] & 0x0F)   << 12;
			ch |=  (wchar_t) (s[++i] & 0x3F) << 6;
			ch |=  (wchar_t) (s[++i] & 0x3F);
		} else if (ch >= 0xC0) {
			ch  =  (wchar_t) (s[i] & 0x1F) << 6;
			ch |=  (wchar_t) (s[++i] & 0x3F);
		}
		res[j] = ch;
	}
	return res;
}

// --------------------------------------------------------------------
//					public
// --------------------------------------------------------------------

int CFont::LoadFont (const string& name, const char *path,float size) {
	fontinfo* newfont = NULL;
	if (fontindex.count(name) > 0)
		newfont = fonts[GetFontIdx(name)];
	else
	{
		newfont = new fontinfo;
		newfont->fontpath=path;
		newfont->fontname=name;
		fonts.push_back(newfont);
		fontindex[name] = fonts.size()-1;
	}
	int pos = GetFontIdx(name);
#ifdef USE_GLES1
	newfont->fonts[size] = new FTGLTextureFont (path);
#else
	newfont->fonts[size] = new FTGLPixmapFont (path);
#endif
	if (newfont->fonts[size]->Error()) {
		Message ("Failed to open font");
		return -1;
	}
	newfont->fonts[size]->FaceSize (size);
	newfont->fonts[size]->CharMap (ft_encoding_unicode);

	return pos;
}

int CFont::LoadFont(const string& name, const string& dir, const string& filename, float size) {
	string path = dir;
	path += SEP;
	path += filename;
	return LoadFont (name, path.c_str(),size);
}

bool CFont::LoadFontlist () {
	CSPList list(MAX_FONTS);
	if (!list.Load ( param.font_dir, "fonts.lst")) return false;
	for (size_t i=0; i<list.Count(); i++) {
		const string& line = list.Line(i);
		string fontfile = SPStrN (line, "file");
		string name = SPStrN (line, "name");

		int ftidx = LoadFont (name, param.font_dir, fontfile,18.0f);
		if (ftidx < 0) {
			Message ("couldn't load font", name);
		}
	}
	return true;
}
FTFont* CFont::GetFont(const size_t findex,const float size)
{
	if (fonts[findex]->fonts.count(size) == 0)
		LoadFont(fonts[findex]->fontname, fonts[findex]->fontpath.c_str(),size);
	return fonts[findex]->fonts[size];
}
size_t CFont::GetFontIdx (const string &name) const {
	return fontindex.at(name);
}

void CFont::SetProps (const string &fontname, float size, const TColor& col) {
	SetProps(fontname, size);
	curr_col  = col;
}

void CFont::SetProps (const string &fontname, float size) {
	curr_font = (int)GetFontIdx (fontname);
	curr_size = size;
}

void CFont::SetFont (const string& fontname) {
	try {
		curr_font = (int)fontindex[fontname];
	} catch (...) {
		curr_font = -1;
	}

	if (fontname == "pc20") curr_fact = 1.25;
	else curr_fact = 1.0;
}

// -------------------- auto ------------------------------------------

int CFont::AutoSizeN (int rel_val) {
	float size = (rel_val + 2) * 4;
	size *= curr_fact;
	size *= Winsys.scale;
	SetSize (size);
	return (int)size;
}

int CFont::AutoDistanceN (int rel_val) {
	float fact = (rel_val + 5) * 0.2;
	float dist = curr_size * fact;
	return (int) dist;
}

// -------------------- draw (x, y, text) -----------------------------

template<typename T>
void CFont::DrawText(float x, float y, const T* text, size_t font, float size) {
	if (font >= fonts.size()) return;

	glPushMatrix();
	FTFont* cf = GetFont(font,size);
	glColor(curr_col);

	float left;
	if (x >= 0) left = x;
	else left = (Winsys.resolution.width - GetTextWidth(text)) / 2;
	if (left < 0) left = 0;

#ifdef USE_GLES1
	if (forientation == OR_TOP) {
		glTranslatef (left, Winsys.resolution.height - curr_size - y, 0.0f);
	} else {
		glTranslatef (left, y, 0.0f);
	}
#else
	if (forientation == OR_TOP) {
		glRasterPos2i ((int)left, (int)(Winsys.resolution.height - curr_size - y));
	} else {
		glRasterPos2i ((int)left, (int)y);
	}
#endif
	cf->Render (text);
	glPopMatrix();
}
template void CFont::DrawText<char>(float x, float y, const char* text, size_t font, float size); // instanciate
template void CFont::DrawText<wchar_t>(float x, float y, const wchar_t* text, size_t font, float size); // instanciate

void CFont::DrawText (float x, float y, const char *text) {
#if USE_UNICODE
	DrawString(x, y, UnicodeStr(text));
#else
	DrawText(x, y, text, curr_font, curr_size);
#endif
}

void CFont::DrawText (float x, float y, const wchar_t *text) {
	DrawText(x, y, text, curr_font, curr_size);
}

void CFont::DrawString (float x, float y, const string &s) {
	DrawText (x, y, s.c_str());
}

void CFont::DrawString (float x, float y, const wstring &s) {
	DrawText (x, y, s.c_str());
}

void CFont::DrawText(float x, float y, const char *text, const string &fontname, float size) {
	size_t temp_font = GetFontIdx (fontname);
#if USE_UNICODE
	DrawText(x, y, UnicodeStr(text).c_str(), temp_font, size);
#else
	DrawText(x, y, text, temp_font, size);
#endif
}

void CFont::DrawText
(float x, float y, const wchar_t *text, const string &fontname, float size) {
	size_t temp_font = GetFontIdx (fontname);
	DrawText(x, y, text, temp_font, size);
}

void CFont::DrawString (
    float x, float y, const string &s, const string &fontname, float size) {
	DrawText (x, y, s.c_str(), fontname, size);
}

void CFont::DrawString (
    float x, float y, const wstring &s, const string &fontname, float size) {
	DrawText (x, y, s.c_str(), fontname, size);
}

// --------------------- metrics --------------------------------------

void CFont::GetTextSize (const wchar_t *text, float &x, float &y, size_t font, float size) {
	if (font >= fonts.size()) { x = 0; y = 0; return; }

	float llx, lly, llz, urx, ury, urz;
	FTFont* cf = GetFont(font,size);
	cf->BBox (text, llx, lly, llz, urx, ury, urz);
	x = urx - llx;
	y = ury - lly;
}

void CFont::GetTextSize (const char *text, float &x, float &y, size_t font, float size) {
#if USE_UNICODE
	GetTextSize(UnicodeStr(text).c_str(), x, y, font, size);
#else
	if (font >= fonts.size()) { x = 0; y = 0; return; }

	float llx, lly, llz, urx, ury, urz;
	FTFont* cf = GetFont(font,size);
	cf->BBox (text, llx, lly, llz, urx, ury, urz);
	x = urx - llx;
	y = ury - lly;
#endif
}

void CFont::GetTextSize (const char *text, float &x, float &y) {
	GetTextSize(text, x, y, curr_font, curr_size);
}

void CFont::GetTextSize (const char *text, float &x, float &y, const string &fontname, float size) {
	size_t temp_font = GetFontIdx (fontname);
	GetTextSize(text, x, y, temp_font, size);
}

float CFont::GetTextWidth (const char *text) {
	float x, y;
	GetTextSize(text, x, y, curr_font, curr_size);
	return x;
}

float CFont::GetTextWidth (const string& text) {
	return GetTextWidth (text.c_str());
}

float CFont::GetTextWidth (const wchar_t *text) {
	float x, y;
	GetTextSize(text, x, y, curr_font, curr_size);
	return x;
}

float CFont::GetTextWidth (const char *text, const string &fontname, float size) {
	size_t temp_font = GetFontIdx (fontname);
	float x, y;
	GetTextSize(text, x, y, temp_font, size);
	return x;
}

float CFont::GetTextWidth (const wchar_t *text, const string &fontname, float size) {
	size_t temp_font = GetFontIdx (fontname);
	float x, y;
	GetTextSize(text, x, y, temp_font, size);
	return x;
}

float CFont::CenterX (const char *text) {
	return (Winsys.resolution.width - GetTextWidth (text)) / 2.0;
}

vector<string> CFont::MakeLineList (const char *source, float width) {
	vector<string> wordlist;
	MakeWordList(wordlist, source);
	vector<string> linelist;

	for (size_t last = 0; last < wordlist.size();)
		last = MakeLine(last, wordlist, linelist, width)+1;

	return linelist;
}
