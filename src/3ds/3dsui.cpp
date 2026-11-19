//=============================================================================
// Basic user interface framework for low-level drawing operations to
// the bottom screen.
//=============================================================================

#include <cstdio>
#include <cstring>

#include <stdarg.h>
#include <string.h>
#include <stdio.h>
#include <3ds.h>
extern "C" {
#include <3ds/font.h>
}

#include "3dsinterface.h"
#include "3dstypes.h"
#include "3dsui.h"
#include "3dsfont.h"
#include "3dsgbk.h"
#include "3dsutf8togbk.h"

int foreColor = 0xffffff;
int backColor = 0x000000;

int translateX = 0;
int translateY = 0;
int viewportX1 = 0;
int viewportY1 = 0;
int viewportX2 = 320;
int viewportY2 = 240;

int fontHeight = 13;
static const int cjkGlyphWidth = 13;
static const int cjkGlyphAdvance = 14;
static CFNT_s *systemFont = NULL;

int viewportStackCount = 0;
int viewportStack[20][4];

// gbk things
const int gbkRowStart = 0xa1;
const int gbkRowEnd = 0xf7;
const int gbkColStart = 0xa1;
const int gbkColEnd = 0xfe;

#define MAX_ALPHA 8
typedef struct
{
    int red[MAX_ALPHA + 1][32];
    int green[MAX_ALPHA + 1][32];
    int blue[MAX_ALPHA + 1][32];
} SAlpha;

SAlpha alphas;

uint8 *fontWidthArray[] = { fontTempestaWidth, fontRondaWidth, fontArialWidth };
uint8 *fontBitmapArray[] = { fontTempestaBitmap, fontRondaBitmap, fontArialBitmap };

uint8 *fontBitmap;
uint8 *fontWidth;

//---------------------------------------------------------------
// Initialize this library
//---------------------------------------------------------------
void ui3dsInitialize()
{
    systemFont = fontGetSystemFont();
    // Precompute alphas
    //
    for (int i = 0; i < 32; i++)
    {
        for (int a = 0; a <= MAX_ALPHA; a++)
        {
            int f = i * a / MAX_ALPHA;
            alphas.red[a][i] = f << 11;
            alphas.green[a][i] = f << 6;
            alphas.blue[a][i] = f;
        }
    }

    // Initialize fonts
    //
    for (int f = 0; f < 3; f++)
    {
        fontBitmap = fontBitmapArray[f];
        fontWidth = fontWidthArray[f];
        for (int i = 0; i < 65536; i++)
        {
            uint8 c = fontBitmap[i];
            if (c == ' ')
                fontBitmap[i] = 0;
            else
                fontBitmap[i] = c - '0';
        }

        fontWidth[1] = 10;
        fontWidth[10] = 0;
        fontWidth[13] = 0;
        fontWidth[248] = 10;
        fontWidth[249] = 10;
        fontWidth[250] = 9;
        fontWidth[251] = 1;
        fontWidth[253] = 10;
        fontWidth[254] = 10;
        fontWidth[255] = 7;
    }

    fontBitmap = fontTempestaBitmap;
    fontWidth = fontTempestaWidth;

    viewportX1 = 0;
    viewportY1 = 0;
    viewportX2 = 320;
    viewportY2 = 240;
}

//---------------------------------------------------------------
// Get the target screen width
//---------------------------------------------------------------
int ui3dsGetScreenWidth(gfxScreen_t targetScreen) {
    return (targetScreen == GFX_TOP) ? SCREEN_TOP_WIDTH : SCREEN_BOTTOM_WIDTH;
}

//---------------------------------------------------------------
// this is called on init and if game screen has swapped
//---------------------------------------------------------------
void ui3dsUpdateScreenSettings(gfxScreen_t gameScreen) {
    screenSettings.GameScreen = gameScreen;
    screenSettings.SecondScreen = (screenSettings.GameScreen == GFX_TOP) ? GFX_BOTTOM : GFX_TOP;
    screenSettings.GameScreenWidth  = ui3dsGetScreenWidth(screenSettings.GameScreen);
    screenSettings.SecondScreenWidth  = ui3dsGetScreenWidth(screenSettings.SecondScreen);

    // reset menu viewport
    viewportX1 = 0;
    viewportY1 = 0;
    viewportX2 = screenSettings.SecondScreenWidth;
    viewportY2 = 240;
    viewportStackCount = 0;
}

//---------------------------------------------------------------
// Sets the font to be used to display text to the screen.
//---------------------------------------------------------------
void ui3dsSetFont(int fontIndex, bool init)
{
    if (fontIndex >= 0 && fontIndex < 3)
    {
        fontBitmap = fontBitmapArray[fontIndex];
        fontWidth = fontWidthArray[fontIndex];
        if (!init)
        {
            settings3DS.Font = fontIndex;
            impl3dsReadWriteSettingsGlobal(true);
        }
    }
}

//---------------------------------------------------------------
// Sets the menu theme.
//---------------------------------------------------------------
void ui3dsSetTheme(int themeIndex)
{
    if (themeIndex >= 0 && themeIndex < 2)
    {
        settings3DS.Theme = themeIndex;
        impl3dsReadWriteSettingsGlobal(true);
    }
}

//---------------------------------------------------------------
// Sets the ui language.
//---------------------------------------------------------------
void ui3dsSetLanguage(int langIndex, bool init)
{
    if (!init)
    {
        if (langIndex >= 0 && langIndex < 2)
        {
            settings3DS.Language = langIndex;
            impl3dsReadWriteSettingsGlobal(true);
        }
    }

    switch(langIndex)
    {
        case 1:
            setLanguage("en_US");
            break;
        default:
            setLanguage("zh_Hans");
            break;
    }
}

//---------------------------------------------------------------
// Sets the global viewport for all drawing
//---------------------------------------------------------------
void ui3dsSetViewport(int x1, int y1, int x2, int y2)
{
    viewportX1 = x1;
    viewportX2 = x2;
    viewportY1 = y1;
    viewportY2 = y2;

    if (viewportX1 < 0) viewportX1 = 0;
    if (viewportX2 > screenSettings.SecondScreenWidth) viewportX2 = screenSettings.SecondScreenWidth;
    if (viewportY1 < 0) viewportY1 = 0;
    if (viewportY2 > 240) viewportY2 = 240;
}

//---------------------------------------------------------------
// Push the global viewport for all drawing
//---------------------------------------------------------------
void ui3dsPushViewport(int x1, int y1, int x2, int y2)
{
    if (viewportStackCount < 10)
    {
        if (x1 < viewportX1) x1 = viewportX1;
        if (x2 > viewportX2) x2 = viewportX2;
        if (y1 < viewportY1) y1 = viewportY1;
        if (y2 > viewportY2) y2 = viewportY2;

        viewportStack[viewportStackCount][0] = viewportX1;
        viewportStack[viewportStackCount][1] = viewportX2;
        viewportStack[viewportStackCount][2] = viewportY1;
        viewportStack[viewportStackCount][3] = viewportY2;
        viewportStackCount++;

        ui3dsSetViewport(x1, y1, x2, y2);
    }
}

//---------------------------------------------------------------
// Pop the global viewport
//---------------------------------------------------------------
void ui3dsPopViewport()
{
    if (viewportStackCount > 0)
    {
        viewportStackCount--;
        viewportX1 = viewportStack[viewportStackCount][0];
        viewportX2 = viewportStack[viewportStackCount][1];
        viewportY1 = viewportStack[viewportStackCount][2];
        viewportY2 = viewportStack[viewportStackCount][3];
    }
}


//---------------------------------------------------------------
// Applies alpha to a given colour.
// NOTE: Alpha is a value from 0 to 10. (0 = transparent, 10 = opaque)
//---------------------------------------------------------------
inline uint16 __attribute__((always_inline)) ui3dsApplyAlphaToColour565(int color565, int alpha)
{
    int red = (color565 >> 11) & 0x1f;
    int green = (color565 >> 6) & 0x1f; // drop the LSB of the green colour
    int blue = (color565) & 0x1f;

    int result = alphas.red[alpha][red] | alphas.blue[alpha][blue] | alphas.green[alpha][green];
    //printf ("result %x * %d = (%x | %x | %x) = %x\n", color565, alpha,
    //   alphas.red[alpha][red], alphas.green[alpha][green], alphas.blue[alpha][blue], result);
    return result;
}


//---------------------------------------------------------------
// Multiplies the alpha to the color in RGBA8 format,
// and returns the results.
//---------------------------------------------------------------
int ui3dsApplyAlphaToColor(int color, float alpha)
{
    if (alpha < 0)      alpha = 0;
    if (alpha > 1.0f)   alpha = 1.0f;

    int a = (int)(alpha * 255);

    return
        ((((color >> 16) & 0xff) * a / 255) << 16) |
        ((((color >> 8) & 0xff) * a / 255) << 8) |
        ((((color >> 0) & 0xff) * a / 255) << 0);
}


// overlay blending mode: returns a color in RGB888 format
// may have more performance impact than simple blending mode 
// but will provide more vibrant colors
// TODO: add alpha value support
int ui3dsOverlayBlendColor(int backgroundColor, int foregroundColor) {
    // Extract the red, green, and blue components of the colors
    float baseR = ((backgroundColor >> 16) & 0xFF) / 255.0f;
    float baseG = ((backgroundColor >> 8) & 0xFF) / 255.0f;
    float baseB = (backgroundColor & 0xFF) / 255.0f;
    
    float blendR = ((foregroundColor >> 16) & 0xFF) / 255.0f;
    float blendG = ((foregroundColor >> 8) & 0xFF) / 255.0f;
    float blendB = (foregroundColor & 0xFF) / 255.0f;

    float resultR = baseR <= 0.5f ? 2 * baseR * blendR : 1 - 2 * (1 - baseR) * (1 - blendR);
    float resultG = baseG <= 0.5f ? 2 * baseG * blendG : 1 - 2 * (1 - baseG) * (1 - blendG);
    float resultB = baseB <= 0.5f ? 2 * baseB * blendB : 1 - 2 * (1 - baseB) * (1 - blendB);

    unsigned char r = static_cast<unsigned int>(resultR * 255);
    unsigned char g = static_cast<unsigned int>(resultG * 255);
    unsigned char b = static_cast<unsigned int>(resultB * 255);

    return (r << 16) | (g << 8) | b;
}


//---------------------------------------------------------------
// Sets the global translate for all drawing
//---------------------------------------------------------------
void ui3dsSetTranslate(int tx, int ty)
{
    translateX = tx;
    translateY = ty;
}


//---------------------------------------------------------------
// Computes the frame buffer offset given the x, y
// coordinates.
//---------------------------------------------------------------
inline int __attribute__((always_inline)) ui3dsComputeFrameBufferOffset(int x, int y)
{
    return ((x) * 240 + (239 - y));
}


//---------------------------------------------------------------
// Gets a pixel colour.
//---------------------------------------------------------------
inline uint16 __attribute__((always_inline)) ui3dsGetPixelInline(uint16 *frameBuffer, int x, int y)
{
    return frameBuffer[ui3dsComputeFrameBufferOffset((x), (y))];
}


//---------------------------------------------------------------
// Sets a pixel colour.
//---------------------------------------------------------------
inline void __attribute__((always_inline)) ui3dsSetPixelInline(uint16 *frameBuffer, int x, int y, int color)
{
    if (color < 0) return;
    if ((x) >= viewportX1 && (x) < viewportX2 &&
        (y) >= viewportY1 && (y) < viewportY2)
    {
        frameBuffer[ui3dsComputeFrameBufferOffset((x), (y))] = color;
    }
}


//---------------------------------------------------------------
// Draws a single character to the screen
//---------------------------------------------------------------
void ui3dsDrawChar(uint16 *frameBuffer, int x, int y, int color565, uint8 c)
{
    // Draws a character to the screen at (x,y)
    // (0,0) is at the top left of the screen.
    //
    int wid = fontWidth[c];
    uint8 alpha;
    //printf ("d %c (%d)\n", c, bmofs);

    if ((y) >= viewportY1 && (y) < viewportY2)
    {
        for (int x1 = 0; x1 < wid; x1++)
        {
            #define GETFONTBITMAP(c, x, y) fontBitmap[c * 256 + x + (y)*16]

            #define SETPIXELFROMBITMAP(y1) \
                alpha = GETFONTBITMAP(c,x1,y1); \
                ui3dsSetPixelInline(frameBuffer, cx, cy + y1, \
                alpha == MAX_ALPHA ? color565 : \
                alpha == 0x0 ? -1 : \
                    ui3dsApplyAlphaToColour565(color565, alpha) + \
                    ui3dsApplyAlphaToColour565(ui3dsGetPixelInline(frameBuffer, cx, cy + y1), MAX_ALPHA - alpha));

            int cx = (x + x1);
            int cy = (y);
            if (cx >= viewportX1 && cx < viewportX2)
            {
                for (int h = 0; h < fontHeight; h++)
                {
                    SETPIXELFROMBITMAP(h);
                }
            }
        }
    }
}


//---------------------------------------------------------------
// Draws a gbk character to the screen
//---------------------------------------------------------------
void ui3dsDrawGBK(uint16 *frameBuffer, int x, int y, int color565, uint16 ch) {
    const int rowCount = (gbkRowEnd - gbkRowStart + 1);
    const int colCount = (gbkColEnd - gbkColStart + 1);

    int r = ch >> 8;
    int c = ch & 0xff;

    if(fontGBK == NULL
        || r < gbkRowStart || r > gbkRowEnd
        || c < gbkColStart || c > gbkColEnd){
        ui3dsDrawChar(frameBuffer, x, y, color565, '?');
        return;
    }

    // Draws a character to the screen at (x,y)
    // (0,0) is at the top left of the screen.
    int wid = cjkGlyphWidth;
    uint8 alpha;

    int idx = (r - gbkRowStart) * colCount + (c - gbkColStart);

    if ((y) >= viewportY1 && (y) < viewportY2)
    {
        for (int x1 = 0; x1 < wid; x1++)
        {
            #define GETFONTBITMAP_GBK(c, x, y) fontGBK[c * 256 + x + (y+1)*16]

            #define SETPIXELFROMBITMAP_GBK(y1) \
                alpha = GETFONTBITMAP_GBK(idx,x1,y1); \
                ui3dsSetPixelInline(frameBuffer, cx, cy + y1, \
                alpha == MAX_ALPHA ? color565 : \
                alpha == 0x0 ? -1 : \
                    ui3dsApplyAlphaToColour565(color565, alpha) + \
                    ui3dsApplyAlphaToColour565(ui3dsGetPixelInline(frameBuffer, cx, cy + y1), MAX_ALPHA - alpha));

            int cx = (x + x1);
            int cy = (y);
            if (cx >= viewportX1 && cx < viewportX2)
            {
                for (int h = 0; h < fontHeight; h++)
                {
                    SETPIXELFROMBITMAP_GBK(h);
                }
            }
        }
    }
}



static bool ui3dsDecodeUtf8At(const char *text, int length, int *offset, u32 *codePoint)
{
    int i = *offset; u8 lead = (u8)text[i];
    int extra = (lead >= 0xC2 && lead <= 0xDF) ? 1 : (lead >= 0xE0 && lead <= 0xEF) ? 2 : (lead >= 0xF0 && lead <= 0xF4) ? 3 : 0;
    if (!extra || i + extra >= length) { *codePoint = lead; *offset = i + 1; return false; }
    u32 cp = lead & (extra == 1 ? 0x1F : extra == 2 ? 0x0F : 0x07);
    for (int j = 1; j <= extra; ++j) {
        u8 tail = (u8)text[i+j];
        if ((tail & 0xC0) != 0x80) { *codePoint = lead; *offset = i + 1; return false; }
        cp = (cp << 6) | (tail & 0x3F);
    }
    if ((extra == 1 && cp < 0x80) || (extra == 2 && cp < 0x800) || (extra == 3 && cp < 0x10000)
        || cp > 0x10FFFF || (cp >= 0xD800 && cp <= 0xDFFF)) { *codePoint = lead; *offset = i + 1; return false; }
    *codePoint = cp; *offset = i + extra + 1; return true;
}
static int ui3dsSystemGlyphWidth(u32 cp)
{
    if (!systemFont || cp < 0x80) return fontWidth[(u8)(cp < 0x80 ? cp : '?')];
    return fontGlyphIndexFromCodePoint(systemFont, cp) < 0 ? fontWidth[(u8)'?'] : cjkGlyphAdvance;
}
static u8 ui3dsReadSystemGlyphPixel(const u8 *sheet, int width, int x, int y)
{
    int bit = (x&1)+(((x>>1)&1)*4)+(((x>>2)&1)*16)+((x>>3)*64)
            +((y&1)*2)+(((y>>1)&1)*8)+(((y>>2)&1)*32)+((y>>3)*width*8);
    return (sheet[bit>>1] >> ((bit&1)*4)) & 0x0F;
}
static int ui3dsDrawSystemGlyph(uint16 *fb, int x, int y, int color, u32 cp)
{
    if (!systemFont || cp < 0x80) return 0;
    int glyph = fontGlyphIndexFromCodePoint(systemFont, cp);
    TGLP_s *tglp = fontGetGlyphInfo(systemFont);
    if (glyph < 0 || !tglp || tglp->sheetFmt != GPU_A4 || !tglp->sheetData
        || !tglp->nRows || !tglp->nLines || !tglp->cellWidth || !tglp->cellHeight) return 0;
    charWidthInfo_s *info = fontGetCharWidthInfo(systemFont, glyph);
    if (!info || !info->glyphWidth) return 0;
    int perSheet = tglp->nRows * tglp->nLines, sheetIndex = glyph / perSheet;
    if (sheetIndex >= tglp->nSheets) return 0;
    int idx = glyph % perSheet;
    int srcX = (idx % tglp->nRows) * (tglp->cellWidth + 1) + 1;
    int srcY = (idx / tglp->nRows) * (tglp->cellHeight + 1) + 1;
    const u8 *sheet = (const u8 *)fontGetGlyphSheetTex(systemFont, sheetIndex);
    if (!sheet) return 0;
    for (int dy=0; dy<fontHeight; ++dy) for (int dx=0; dx<cjkGlyphWidth; ++dx) {
        int px=x+dx, py=y+dy;
        if (px<viewportX1 || px>=viewportX2 || py<viewportY1 || py>=viewportY2) continue;
        int sx=srcX+dx*info->glyphWidth/cjkGlyphWidth, sy=srcY+dy*tglp->cellHeight/fontHeight;
        int alpha=(ui3dsReadSystemGlyphPixel(sheet,tglp->sheetWidth,sx,sy)*MAX_ALPHA+7)/15;
        if (!alpha) continue;
        ui3dsSetPixelInline(fb,px,py,alpha==MAX_ALPHA ? color
            : ui3dsApplyAlphaToColour565(color,alpha)
             + ui3dsApplyAlphaToColour565(ui3dsGetPixelInline(fb,px,py),MAX_ALPHA-alpha));
    }
    return cjkGlyphAdvance;
}

//---------------------------------------------------------------
// Computes width of the string
//---------------------------------------------------------------
int ui3dsGetStringWidth(const char *s, int startPos = 0, int endPos = 0xffff)
{
    int width = 0, length = strlen(s);
    for (int i = startPos; i <= endPos && i < length; ) {
        uint8 c = (uint8)s[i];
        if (!c) break;
        if (c >= 0x80) {
            u32 cp; int next = i;
            if (ui3dsDecodeUtf8At(s,length,&next,&cp)) {
                uint16 gbk = cp <= 0xFFFF ? getGbkChar((uint16)cp) : 0;
                uint8 hi=gbk>>8, lo=gbk&0xFF;
                if (fontGBK && hi>=gbkRowStart && hi<=gbkRowEnd && lo>=gbkColStart && lo<=gbkColEnd)
                    width += cjkGlyphAdvance;
                else width += ui3dsSystemGlyphWidth(cp);
                i=next; continue;
            }
        }
        width += fontWidth[c]; ++i;
    }
    return width;
}

#define CONVERT_TO_565(x)    (((x & 0xf8) >> 3) | (((x >> 8) & 0xf8) << 3) | (((x >> 16) & 0xf8) << 8))

//---------------------------------------------------------------
// Colors are in the 888 (RGB) format.
//---------------------------------------------------------------
void ui3dsSetColor(int newForeColor, int newBackColor)
{
    foreColor = newForeColor;
    backColor = newBackColor;
}


//---------------------------------------------------------------
// Draws a rectangle with the color (in RGBA8 format).
//
// Note: x0,y0 are inclusive. x1,y1 are exclusive.
//---------------------------------------------------------------
static void ui3dsDraw32BitRect(uint32 * fb, int x0, int y0, int x1, int y1, int color, float alpha)
{
    uint32 c = ui3dsApplyAlphaToColor(color, alpha) << 8;
    
    for (int y = y0; y < y1; y++) {
        for (int x = x0; x < x1; x++) {
            int fbofs = x * 240 + (239 - y);
            fb[fbofs] = c;
        }
    }
}

//---------------------------------------------------------------
// Draws a rectangle with the colour (in RGB888 format).
//
// Note: x0,y0 are inclusive. x1,y1 are exclusive.
//---------------------------------------------------------------
void ui3dsDrawRect(int x0, int y0, int x1, int y1, int color, float alpha)
{
    if (color < 0)
        return;

    x0 += translateX;
    x1 += translateX;
    y0 += translateY;
    y1 += translateY;

    if (x0 < viewportX1) x0 = viewportX1;
    if (x1 > viewportX2) x1 = viewportX2;
    if (y0 < viewportY1) y0 = viewportY1;
    if (y1 > viewportY2) y1 = viewportY2;

    if (alpha < 0) alpha = 0;
    if (alpha > 1.0f) alpha = 1.0f;

    uint16* fb = (uint16 *) gfxGetFramebuffer(screenSettings.SecondScreen, GFX_LEFT, NULL, NULL);
    if (gfxGetScreenFormat(screenSettings.SecondScreen) == GSP_RGBA8_OES) {
        return ui3dsDraw32BitRect((uint32 *)fb, x0, y0, x1, y1, color, alpha);
    }
    color = CONVERT_TO_565(color);

    if (alpha == 1.0f)
    {
        for (int x = x0; x < x1; x++)
        {
            int fbofs = (x) * 240 + (239 - y0);
            for (int y = y0; y < y1; y++)
                fb[fbofs--] = color;
        }
    }
    else if (alpha == 0.0)
    {
        return;
    }
    else
    {
        int iAlpha = alpha * MAX_ALPHA;
        for (int x = x0; x < x1; x++)
        {
            int fbofs = (x) * 240 + (239 - y0);
            for (int y = y0; y < y1; y++)
            {
                fb[fbofs] =
                    ui3dsApplyAlphaToColour565(color, iAlpha) +
                    ui3dsApplyAlphaToColour565(fb[fbofs], MAX_ALPHA - iAlpha);
                fbofs --;
            }
        }
    }
}



//---------------------------------------------------------------
// Draws a rectangle with the back colour
//
// Note: x0,y0 are inclusive. x1,y1 are exclusive.
//---------------------------------------------------------------
void ui3dsDrawRect(int x0, int y0, int x1, int y1)
{
    ui3dsDrawRect(x0, y0, x1, y1, backColor, 1.0f);
}


//---------------------------------------------------------------
// Draws a string at the given position without translation.
//---------------------------------------------------------------
void ui3dsDrawStringOnly(uint16 *fb, int absoluteX, int absoluteY, int color, const char *buffer, int startPos = 0, int endPos = 0xffff)
{
    int x=absoluteX, y=absoluteY;
    if (color<0 || y<viewportY1-16 || y>viewportY2) return;
    color=CONVERT_TO_565(color);
    int length=strlen(buffer);
    for (int i=startPos; i<=endPos && i<length; ) {
        uint8 c=(uint8)buffer[i];
        if (!c) break;
        if (c>=0x80) {
            u32 cp; int next=i;
            if (ui3dsDecodeUtf8At(buffer,length,&next,&cp)) {
                uint16 gbk=cp<=0xFFFF ? getGbkChar((uint16)cp) : 0;
                uint8 hi=gbk>>8, lo=gbk&0xFF;
                int advance;
                if (fontGBK && hi>=gbkRowStart && hi<=gbkRowEnd && lo>=gbkColStart && lo<=gbkColEnd) {
                    ui3dsDrawGBK(fb,x,y,color,gbk);
                    advance=cjkGlyphAdvance;
                } else {
                    advance=ui3dsDrawSystemGlyph(fb,x,y,color,cp);
                    if (advance<=0) advance=fontWidth[(u8)'?'];
                }
                x += advance; i=next; continue;
            }
        }
        if (c!=32) ui3dsDrawChar(fb,x,y,color,c);
        x+=fontWidth[c]; ++i;
    }
}

//---------------------------------------------------------------
// Draws a string with the forecolor, with wrapping
//---------------------------------------------------------------
void ui3dsDrawStringWithWrapping(int x0, int y0, int x1, int y1, int color, int horizontalAlignment, const char *buffer_in, bool isTextCFormat)
{
    int strLineCount = 0;
    int strLineStart[30];
    int strLineEnd[30];

    x0 += translateX;
    x1 += translateX;
    y0 += translateY;
    y1 += translateY;

    ui3dsPushViewport(x0, y0, x1, y1);
    //ui3dsDrawRect(x0, y0, x1, y1, backColor);  // Draw the background color

    if (buffer_in != NULL)
    {
        char *buffer = (settings3DS.Language != 0 && !isTextCFormat) ? (char *) getTextFromMap(buffer_in) : (char *) buffer_in;
        int maxWidth = x1 - x0;
        int slen = strlen(buffer);

        int curStartPos = 0;
        int curEndPos = slen;
        uint16 lineWidth = 0;
        for (int i = 0; i <= slen; )
        {
            if (i != curStartPos)
            {
                if (buffer[i] == ' ' && i > 0 && buffer[i-1] != ' ')
                    curEndPos = i - 1;
                else if (buffer[i] == '-')  // use space or dash as line breaks
                    curEndPos = i;
                else if (buffer[i] == '\n')  // \n as line breaks.
                {
                    curEndPos = i - 1;
                    lineWidth = 65535;     // force the line break.
                }
            }

            if ((uint8)buffer[i] >= 0x80) {
                u32 cp; int next=i;
                if (ui3dsDecodeUtf8At(buffer,slen,&next,&cp)) {
                    uint16 gbk=cp<=0xFFFF ? getGbkChar((uint16)cp) : 0;
                    uint8 hi=gbk>>8, lo=gbk&0xFF;
                    lineWidth += (fontGBK && hi>=gbkRowStart && hi<=gbkRowEnd && lo>=gbkColStart && lo<=gbkColEnd)
                        ? cjkGlyphAdvance : ui3dsSystemGlyphWidth(cp);
                    i=next-1;
                } else lineWidth += fontWidth[(uint8)buffer[i]];
            } else lineWidth += fontWidth[(uint8)buffer[i]];

            if (lineWidth > maxWidth)
            {
                // Break the line here
                strLineStart[strLineCount] = curStartPos;
                strLineEnd[strLineCount] = curEndPos;
                strLineCount++;

                if (strLineCount >= 30) break;

                if (lineWidth != 65535)
                {
                    i = curEndPos + 1;
                    while (buffer[i] == ' ')
                        i++;
                } else
                    i = curEndPos + 2;
                curStartPos = i;
                curEndPos = slen;
                lineWidth = 0;
            } else
                i++;
        }

        // Output the last line.
        curEndPos = slen;
        if (curStartPos <= curEndPos)
        {
            strLineStart[strLineCount] = curStartPos;
            strLineEnd[strLineCount] = curEndPos;
            strLineCount++;
        }

        uint16* fb = (uint16 *) gfxGetFramebuffer(screenSettings.SecondScreen, GFX_LEFT, NULL, NULL);
        for (int i = 0; i < strLineCount; i++)
        {
            int x = x0;
            if (horizontalAlignment >= 0)
            {
                int sWidth = ui3dsGetStringWidth(buffer, strLineStart[i], strLineEnd[i]);

                if (horizontalAlignment == 0)   // center aligned
                    x = (maxWidth - sWidth) / 2 + x0;
                else                            // right aligned
                    x = maxWidth - sWidth + x0;
            }

            ui3dsDrawStringOnly(fb, x, y0, color, buffer, strLineStart[i], strLineEnd[i]);
            y0 += 12;
        }
    }

    ui3dsPopViewport();
}


//---------------------------------------------------------------
// Draws a string with the forecolor, with no wrapping
//---------------------------------------------------------------
void ui3dsDrawStringWithNoWrapping(int x0, int y0, int x1, int y1, int color, int horizontalAlignment, const char *buffer_in, int startPos, bool isTextCFormat)
{
    x0 += translateX;
    x1 += translateX;
    y0 += translateY;
    y1 += translateY;

    ui3dsPushViewport(x0, y0, x1, y1);
    //ui3dsDrawRect(x0, y0, x1, y1, backColor);  // Draw the background color

    if (buffer_in != NULL)
    {
        char *buffer = (char *) ((settings3DS.Language != 0 && !isTextCFormat) ? &getTextFromMap(buffer_in)[startPos] : &buffer_in[startPos]);
        uint16* fb = (uint16 *) gfxGetFramebuffer(screenSettings.SecondScreen, GFX_LEFT, NULL, NULL);
        int maxWidth = x1 - x0;
        int x = x0;
        if (horizontalAlignment >= HALIGN_CENTER)
        {
            int sWidth = ui3dsGetStringWidth(buffer);

            if (horizontalAlignment == HALIGN_CENTER)   // center aligned
                x = (maxWidth - sWidth) / 2 + x0;
            else                                        // right aligned
                x = maxWidth - sWidth + x0;
        }
        ui3dsDrawStringOnly(fb, x, y0, color, buffer);
    }

    ui3dsPopViewport();
}


//---------------------------------------------------------------
// Copies pixel data from the frame buffer to another buffer
//---------------------------------------------------------------
void ui3dsCopyFromFrameBuffer(uint16 *destBuffer)
{
    uint16* fb = (uint16 *) gfxGetFramebuffer(screenSettings.SecondScreen, GFX_LEFT, NULL, NULL);
    memcpy(destBuffer, fb, screenSettings.SecondScreenWidth * 240 * 2);
}


//---------------------------------------------------------------
// Copies pixel data from the buffer to the framebuffer
//---------------------------------------------------------------
void ui3dsBlitToFrameBuffer(uint16 *srcBuffer, float alpha)
{
    uint16* fb = (uint16 *) gfxGetFramebuffer(screenSettings.SecondScreen, GFX_LEFT, NULL, NULL);

    int a = (int)(alpha * MAX_ALPHA);
    for (int x = viewportX1; x < viewportX2; x++)
        for (int y = viewportY1; y < viewportY2; y++)
        {
            int ofs = ui3dsComputeFrameBufferOffset(x,y);
            int color = ui3dsApplyAlphaToColour565(srcBuffer[ofs], a);
            fb[ofs] = color;
        }
}
