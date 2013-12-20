/*
** Taiga, a lightweight client for MyAnimeList
** Copyright (C) 2010-2012, Eren Okka
** 
** This program is free software: you can redistribute it and/or modify
** it under the terms of the GNU General Public License as published by
** the Free Software Foundation, either version 3 of the License, or
** (at your option) any later version.
** 
** This program is distributed in the hope that it will be useful,
** but WITHOUT ANY WARRANTY; without even the implied warranty of
** MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
** GNU General Public License for more details.
** 
** You should have received a copy of the GNU General Public License
** along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

#include "base/std.h"

#include "theme.h"

#include "base/gfx.h"
#include "base/string.h"
#include "taiga/path.h"
#include "taiga/taiga.h"
#include "base/xml.h"

Theme UI;

// =============================================================================

Theme::Theme() {
  // Create image lists
  ImgList16.Create(16, 16);
  ImgList24.Create(24, 24);
}

bool Theme::Load() {
  xml_document document;
  wstring path = taiga::GetPath(taiga::kPathThemeCurrent);
  xml_parse_result parse_result = document.load_file(path.c_str());

  if (parse_result.status != pugi::status_ok) {
    MessageBox(nullptr, L"Could not read theme file.", path.c_str(),
               MB_OK | MB_ICONERROR);
    return false;
  }
  
  // Read theme
  xml_node theme = document.child(L"theme");
  xml_node icons16 = theme.child(L"icons").child(L"set_16px");
  xml_node icons24 = theme.child(L"icons").child(L"set_24px");
  xml_node image = theme.child(L"list").child(L"background").child(L"image");
  xml_node progress = theme.child(L"list").child(L"progress");

  // Read icons
  icons16_.clear();
  icons24_.clear();
  for (xml_node icon = icons16.child(L"icon"); icon; icon = icon.next_sibling(L"icon"))
    icons16_.push_back(icon.attribute(L"name").value());
  for (xml_node icon = icons24.child(L"icon"); icon; icon = icon.next_sibling(L"icon"))
    icons24_.push_back(icon.attribute(L"name").value());

  // Read list
  if (list_background.bitmap) {
    DeleteObject(list_background.bitmap);
    list_background.bitmap = nullptr;
  }
  list_background.name = image.attribute(L"name").value();
  list_background.flags = image.attribute(L"flags").as_int();
  list_background.offset_x = image.attribute(L"offset_x").as_int();
  list_background.offset_y = image.attribute(L"offset_y").as_int();
  if (!list_background.name.empty()) {
    wstring path = GetPathOnly(path) + list_background.name + L".png";
    Gdiplus::Bitmap bmp(path.c_str());
    bmp.GetHBITMAP(NULL, &list_background.bitmap);
  }
  #define READ_PROGRESS_DATA(x, name) \
    list_progress.x.type = progress.child(name).attribute(L"type").value(); \
    list_progress.x.value[0] = HexToARGB(progress.child(name).attribute(L"value_1").value()); \
    list_progress.x.value[1] = HexToARGB(progress.child(name).attribute(L"value_2").value()); \
    list_progress.x.value[2] = HexToARGB(progress.child(name).attribute(L"value_3").value());
  READ_PROGRESS_DATA(aired,      L"aired");
  READ_PROGRESS_DATA(available,  L"available");
  READ_PROGRESS_DATA(background, L"background");
  READ_PROGRESS_DATA(border,     L"border");
  READ_PROGRESS_DATA(button,     L"button");
  READ_PROGRESS_DATA(completed,  L"completed");
  READ_PROGRESS_DATA(dropped,    L"dropped");
  READ_PROGRESS_DATA(separator,  L"separator");
  READ_PROGRESS_DATA(watching,   L"watching");
  #undef READ_PROGRESS_DATA

  return true;
}

bool Theme::LoadImages() {
  // Clear image lists
  ImgList16.Remove(-1);
  ImgList24.Remove(-1);

  wstring path = GetPathOnly(taiga::GetPath(taiga::kPathThemeCurrent));
  
  // Populate image lists
  HBITMAP hBitmap;
  for (size_t i = 0; i < ICONCOUNT_16PX && i < icons16_.size(); i++) {
    hBitmap = GdiPlus.LoadImage(path + L"16px\\" + icons16_.at(i) + L".png");
    ImgList16.AddBitmap(hBitmap, CLR_NONE);
    DeleteObject(hBitmap);
  }
  for (size_t i = 0; i < ICONCOUNT_24PX && i < icons24_.size(); i++) {
    hBitmap = GdiPlus.LoadImage(path + L"24px\\" + icons24_.at(i) + L".png");
    ImgList24.AddBitmap(hBitmap, CLR_NONE);
    DeleteObject(hBitmap);
  }

  return true;
}

// =============================================================================

Font::Font()
    : font_(nullptr) {
}

Font::Font(HFONT font)
    : font_(font) {
}

Font::~Font() {
  Set(nullptr);
}

HFONT Font::Get() const {
  return font_;
}

void Font::Set(HFONT font) {
  if (font_)
    ::DeleteObject(font_);
  font_ = font;
}

Font::operator HFONT() const {
  return font_;
}

bool Theme::CreateFonts(HDC hdc) {
  LOGFONT lFont = {0};
  lFont.lfCharSet = DEFAULT_CHARSET;
  lFont.lfOutPrecision = OUT_STRING_PRECIS;
  lFont.lfClipPrecision = CLIP_STROKE_PRECIS;
  lFont.lfQuality = PROOF_QUALITY;
  lFont.lfPitchAndFamily = VARIABLE_PITCH;

  // Bold font
  lFont.lfHeight = -MulDiv(9, GetDeviceCaps(hdc, LOGPIXELSY), 72);
  lFont.lfWeight = FW_BOLD;
  lstrcpy(lFont.lfFaceName, L"Segoe UI");
  font_bold.Set(::CreateFontIndirect(&lFont));

  // Header font
  lFont.lfHeight = -MulDiv(12, GetDeviceCaps(hdc, LOGPIXELSY), 72);
  lFont.lfWeight = FW_NORMAL;
  lstrcpy(lFont.lfFaceName, L"Segoe UI");
  font_header.Set(::CreateFontIndirect(&lFont));

  return true;
}

// =============================================================================

Theme::ListBackground::ListBackground()
    : bitmap(nullptr), flags(0), offset_x(0), offset_y(0) {
}

Theme::ListBackground::~ListBackground() {
  if (bitmap) {
    ::DeleteObject(bitmap);
    bitmap = nullptr;
  }
}

void Theme::ListProgress::Item::Draw(HDC hdc, const LPRECT rect) {
  // Solid
  if (type == L"solid") {
    HBRUSH hbrSolid = CreateSolidBrush(value[0]);
    FillRect(hdc, rect, hbrSolid);
    DeleteObject(hbrSolid);

  // Gradient
  } else if (type == L"gradient") {
    GradientRect(hdc, rect, value[0], value[1], value[2] > 0);

  // Progress bar
  } else if (type == L"progress") {
    DrawProgressBar(hdc, rect, value[0], value[1], value[2]);
  }
}