/***********************
 * bed_mesh_screen.cpp *
 ***********************/

/****************************************************************************
 *   Written By Marcio Teixeira 2020                                        *
 *                                                                          *
 *   This program is free software: you can redistribute it and/or modify   *
 *   it under the terms of the GNU General Public License as published by   *
 *   the Free Software Foundation, either version 3 of the License, or      *
 *   (at your option) any later version.                                    *
 *                                                                          *
 *   This program is distributed in the hope that it will be useful,        *
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of         *
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the          *
 *   GNU General Public License for more details.                           *
 *                                                                          *
 *   To view a copy of the GNU General Public License, go to the following  *
 *   location: <http://www.gnu.org/licenses/>.                              *
 ****************************************************************************/

#include "../config.h"

#if ENABLED(TOUCH_UI_FTDI_EVE) && HAS_MESH

#include "screens.h"
#include "screen_data.h"

using namespace FTDI;
using namespace Theme;
using namespace ExtUI;

#ifdef TOUCH_UI_PORTRAIT
  #define GRID_COLS 2
  #define GRID_ROWS 10

  #define MESH_POS    BTN_POS(1, 2), BTN_SIZE(2,5)
  #define Z_LABEL_POS BTN_POS(1, 8), BTN_SIZE(1,1)
  #define Z_VALUE_POS BTN_POS(2, 8), BTN_SIZE(1,1)
  #define WAIT_POS    BTN_POS(1, 8), BTN_SIZE(2,1)
  #define BACK_POS    BTN_POS(1,10), BTN_SIZE(2,1)
#else
  #define GRID_COLS 5
  #define GRID_ROWS 5

  #define MESH_POS       BTN_POS(2,1), BTN_SIZE(4,5)
  #define Z_LABEL_POS    BTN_POS(1,3), BTN_SIZE(1,1)
  #define Z_VALUE_POS    BTN_POS(1,4), BTN_SIZE(2,1)
  #define WAIT_POS       BTN_POS(1,3), BTN_SIZE(2,2)
  #define BACK_POS       BTN_POS(1,5), BTN_SIZE(2,1)
#endif

void BedMeshScreen::drawMesh(int16_t x, int16_t y, int16_t w, int16_t h, ExtUI::bed_mesh_t data, uint8_t opts) {
  CommandProcessor cmd;

  #define TRANSFORM_2(X,Y,Z)  (X), (Y)                                                             // No transform
  #define TRANSFORM_1(X,Y,Z)  TRANSFORM_2((X) + (Y) * slant, (Y) - (Z), 0)                         // Perspective
  #define TRANSFORM(X,Y,Z)    TRANSFORM_1(float(X)/(cols-1) - 0.5, float(Y)/(rows-1)  - 0.5, (Z))  // Normalize

  constexpr uint8_t rows   = GRID_MAX_POINTS_Y;
  constexpr uint8_t cols   = GRID_MAX_POINTS_X;
  const float slant        = 0.5;
  const float bounds_min[] = {TRANSFORM(0   ,0   ,0)};
  const float bounds_max[] = {TRANSFORM(cols,rows,0)};
  const float scale_x      = float(w)/(bounds_max[0] - bounds_min[0]);
  const float scale_y      = float(h)/(bounds_max[1] - bounds_min[1]);
  const float center_x     = x + w/2;
  const float center_y     = y + h/2;

  float   val_mean = 0;
  float   val_max  = -INFINITY;
  float   val_min  =  INFINITY;
  uint8_t val_cnt  = 0;

  if (opts & USE_AUTOSCALE) {
    // Compute the mean
    for (uint8_t y = 0; y < rows; y++) {
      for (uint8_t x = 0; x < cols; x++) {
        const float val = data[x][y];
        if (!isnan(val)) {
          val_mean += val;
          val_max   = max(val_max, val);
          val_min   = min(val_min, val);
          val_cnt++;
        }
      }
    }
    if (val_cnt) {
      val_mean /= val_cnt;
      val_min  -= val_mean;
      val_max  -= val_mean;
    } else {
      val_mean = 0;
      val_min  = 0;
      val_max  = 0;
    }
  }

  const float scale_z = ((val_max == val_min) ? 1 : 1/(val_max - val_min)) * 0.1;

  #undef  TRANSFORM_2
  #define TRANSFORM_2(X,Y,Z)  center_x + (X) * scale_x, center_y + (Y) * scale_y      // Scale and position
  #define VALUE(X,Y)         ((data && ISVAL(X,Y)) ? data[X][Y] : 0)
  #define ISVAL(X,Y)         (data ? !isnan(data[X][Y]) : true)
  #define HEIGHT(X,Y)        (VALUE(X,Y) * scale_z)

  uint16_t basePointSize = min(scale_x,scale_y) / max(cols,rows);

  cmd.cmd(SAVE_CONTEXT())
     .cmd(VERTEX_FORMAT(0))
     .cmd(TAG_MASK(false))
     .cmd(SAVE_CONTEXT());

  for (uint8_t y = 0; y < rows; y++) {
    for (uint8_t x = 0; x < cols; x++) {
      if (ISVAL(x,y)) {
       const bool hasLeftSegment  = x < cols - 1 && ISVAL(x+1,y);
       const bool hasRightSegment = y < rows - 1 && ISVAL(x,y+1);
       if (hasLeftSegment || hasRightSegment) {
         cmd.cmd(BEGIN(LINE_STRIP));
         if (hasLeftSegment)  cmd.cmd(VERTEX2F(TRANSFORM(x + 1, y    , HEIGHT(x + 1, y    ))));
         cmd.cmd(                     VERTEX2F(TRANSFORM(x    , y    , HEIGHT(x    , y    ))));
         if (hasRightSegment) cmd.cmd(VERTEX2F(TRANSFORM(x    , y + 1, HEIGHT(x    , y + 1))));
       }
      }
    }

    if (opts & USE_POINTS) {
      cmd.cmd(POINT_SIZE(basePointSize * 2));
      cmd.cmd(BEGIN(POINTS));
      for (uint8_t x = 0; x < cols; x++) {
        if (ISVAL(x,y)) {
          if (opts & USE_COLORS) {
            const float   val_dev  = VALUE(x, y) - val_mean;
            const uint8_t neg_byte = sq(val_dev) / sq(val_dev < 0 ? val_min : val_max) * 0xFF;
            const uint8_t pos_byte = 255 - neg_byte;
            cmd.cmd(COLOR_RGB(pos_byte, pos_byte, 0xFF));
          }
          cmd.cmd(VERTEX2F(TRANSFORM(x, y, HEIGHT(x, y))));
        }
      }
      if (opts & USE_COLORS) {
        cmd.cmd(RESTORE_CONTEXT())
           .cmd(SAVE_CONTEXT());
      }
    }
  }
  cmd.cmd(RESTORE_CONTEXT())
     .cmd(TAG_MASK(true));

  if (opts & USE_TAGS) {
    cmd.cmd(COLOR_MASK(false, false, false, false))
       .cmd(POINT_SIZE(basePointSize * 10))
       .cmd(BEGIN(POINTS));
    for (uint8_t y = 0; y < rows; y++) {
      for (uint8_t x = 0; x < cols; x++) {
        const uint8_t tag = pointToTag(x, y);
        cmd.tag(tag).cmd(VERTEX2F(TRANSFORM(x, y, HEIGHT(x, y))));
      }
    }
    cmd.cmd(COLOR_MASK(true, true, true, true));
  }

  if (opts & USE_HIGHLIGHT) {
    const uint8_t tag = screen_data.BedMeshScreen.highlightedTag;
    uint8_t x, y;
    tagToPoint(tag, x, y);
    cmd.cmd(COLOR_A(128))
       .cmd(POINT_SIZE(basePointSize * 6))
       .cmd(BEGIN(POINTS))
       .tag(tag).cmd(VERTEX2F(TRANSFORM(x, y, HEIGHT(x, y))));
  }
  cmd.cmd(END());
  cmd.cmd(RESTORE_CONTEXT());
}

uint8_t BedMeshScreen::pointToTag(uint8_t x, uint8_t y) {
  return y * (GRID_MAX_POINTS_X) + x + 10;
}

void BedMeshScreen::tagToPoint(uint8_t tag, uint8_t &x, uint8_t &y) {
  x = (tag - 10) % (GRID_MAX_POINTS_X);
  y = (tag - 10) / (GRID_MAX_POINTS_X);
}

void BedMeshScreen::onEntry() {
  screen_data.BedMeshScreen.highlightedTag = 0;
  screen_data.BedMeshScreen.count = 0;
  BaseScreen::onEntry();
}

float BedMeshScreen::getHightlightedValue() {
  if (screen_data.BedMeshScreen.highlightedTag) {
    xy_uint8_t pt;
    tagToPoint(screen_data.BedMeshScreen.highlightedTag, pt.x, pt.y);
    return ExtUI::getMeshPoint(pt);
  }
  return NAN;
}

void BedMeshScreen::drawHighlightedPointValue() {
  char str[16];
  const float val = getHightlightedValue();
  const bool isGood = !isnan(val);
  if (isGood)
    dtostrf(val, 5, 3, str);
  else
    strcpy_P(str, PSTR("-"));

  CommandProcessor cmd;
  cmd.font(Theme::font_medium)
     .text(Z_LABEL_POS, GET_TEXT_F(MSG_MESH_EDIT_Z))
     .text(Z_VALUE_POS, str)
     .colors(action_btn)
     .tag(1).button( BACK_POS, GET_TEXT_F(MSG_BACK))
     .tag(0);
}

void BedMeshScreen::onRedraw(draw_mode_t what) {
  if (what & BACKGROUND) {
    CommandProcessor cmd;
    cmd.cmd(CLEAR_COLOR_RGB(bg_color))
       .cmd(CLEAR(true,true,true));

    // Draw the shadow and tags
    cmd.cmd(COLOR_RGB(0x444444));
    BedMeshScreen::drawMesh(MESH_POS, nullptr, USE_POINTS | USE_TAGS);
    cmd.cmd(COLOR_RGB(bg_text_enabled));
  }

  if (what & FOREGROUND) {
    const bool levelingFinished = screen_data.BedMeshScreen.count >= GRID_MAX_POINTS;
    if (levelingFinished) drawHighlightedPointValue();

    BedMeshScreen::drawMesh(MESH_POS, ExtUI::getMeshArray(),
      USE_POINTS | USE_HIGHLIGHT | USE_AUTOSCALE | (levelingFinished ? USE_COLORS : 0));
  }
}

bool BedMeshScreen::onTouchStart(uint8_t tag) {
  screen_data.BedMeshScreen.highlightedTag = tag;
  return true;
}

bool BedMeshScreen::onTouchEnd(uint8_t tag) {
  switch(tag) {
    case 1:
      GOTO_PREVIOUS();
      return true;
    default:
      return false;
  }
}

void BedMeshScreen::onMeshUpdate(const int8_t, const int8_t, const float) {
  if (AT_SCREEN(BedMeshScreen))
    onRefresh();
}

void BedMeshScreen::onMeshUpdate(const int8_t x, const int8_t y, const ExtUI::probe_state_t state) {
  if (state == ExtUI::PROBE_FINISH) {
    screen_data.BedMeshScreen.highlightedTag = pointToTag(x, y);
    screen_data.BedMeshScreen.count++;
  }
  BedMeshScreen::onMeshUpdate(x, y, 0);
}

#endif // TOUCH_UI_FTDI_EVE
