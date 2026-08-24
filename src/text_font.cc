#include "text_font.h"

#include <stdio.h>
#include <string.h>

#include "color.h"
#include "db.h"
#include "memory.h"
#include "platform_compat.h"

namespace fallout {

// The maximum number of text fonts.
#define TEXT_FONT_MAX (10)

// The maximum number of font managers.
#define FONT_MANAGER_MAX (10)

typedef struct TextFontGlyph {
    int width;
    int dataOffset;
} TextFontGlyph;

typedef struct TextFontDescriptor {
    int glyphCount;
    int lineHeight;
    int letterSpacing;
    TextFontGlyph* glyphs;
    unsigned char* data;
} TextFontDescriptor;

static void textFontSetCurrentImpl(int font);
static bool fontManagerFind(int font, FontManager** fontManagerPtr);
static void textFontDrawImpl(unsigned char* buf, const char* string, int length, int pitch, int color);
static int textFontGetLineHeightImpl();
static int textFontGeStringWidthImpl(const char* string);
static int textFontGetCharacterWidthImpl(int ch);
static int textFontGetMonospacedStringWidthImpl(const char* string);
static int textFontGetLetterSpacingImpl();
static int textFontGetBufferSizeImpl(const char* string);
static int textFontGetMonospacedCharacterWidthImpl();

FontManager gTextFontManager = {
    0,
    9,
    textFontSetCurrentImpl,
    textFontDrawImpl,
    textFontGetLineHeightImpl,
    textFontGeStringWidthImpl,
    textFontGetCharacterWidthImpl,
    textFontGetMonospacedStringWidthImpl,
    textFontGetLetterSpacingImpl,
    textFontGetBufferSizeImpl,
    textFontGetMonospacedCharacterWidthImpl,
};

int gCurrentFont = -1;
int gFontManagersCount = 0;
FontManagerDrawTextProc* fontDrawText = nullptr;
FontManagerGetLineHeightProc* fontGetLineHeight = nullptr;
FontManagerGetStringWidthProc* fontGetStringWidth = nullptr;
FontManagerGetCharacterWidthProc* fontGetCharacterWidth = nullptr;
FontManagerGetMonospacedStringWidthProc* fontGetMonospacedStringWidth = nullptr;
FontManagerGetLetterSpacingProc* fontGetLetterSpacing = nullptr;
FontManagerGetBufferSizeProc* fontGetBufferSize = nullptr;
FontManagerGetMonospacedCharacterWidth* fontGetMonospacedCharacterWidth = nullptr;

static TextFontDescriptor gTextFontDescriptors[TEXT_FONT_MAX];
static FontManager gFontManagers[FONT_MANAGER_MAX];
static TextFontDescriptor* gCurrentTextFontDescriptor;

int textFontsInit()
{
    int currentFont = -1;

    FontManager fontManager;
    memcpy(&fontManager, &gTextFontManager, sizeof(fontManager));

    for (int font = 0; font < TEXT_FONT_MAX; font++) {
        if (textFontLoad(font) == -1) {
            gTextFontDescriptors[font].glyphCount = 0;
        } else if (currentFont == -1) {
            currentFont = font;
        }
    }

    if (currentFont == -1) {
        return -1;
    }

    if (fontManagerAdd(&fontManager) == -1) {
        return -1;
    }

    fontSetCurrent(currentFont);
    return 0;
}

void textFontsExit()
{
    for (int index = 0; index < TEXT_FONT_MAX; index++) {
        TextFontDescriptor* textFontDescriptor = &(gTextFontDescriptors[index]);
        if (textFontDescriptor->glyphCount != 0) {
            internal_free(textFontDescriptor->glyphs);
            internal_free(textFontDescriptor->data);
        }
    }
}

int textFontLoad(int font)
{
    int rc = -1;

    char path[COMPAT_MAX_PATH];
    snprintf(path, sizeof(path), "font%d.fon", font);

    TextFontDescriptor* textFontDescriptor = &(gTextFontDescriptors[font]);
    textFontDescriptor->data = nullptr;
    textFontDescriptor->glyphs = nullptr;

    File* stream = fileOpen(path, "rb");
    int dataSize;
    if (stream == nullptr) {
        goto out;
    }

    if (fileRead(&(textFontDescriptor->glyphCount), 4, 1, stream) != 1) goto out;
    if (fileRead(&(textFontDescriptor->lineHeight), 4, 1, stream) != 1) goto out;
    if (fileRead(&(textFontDescriptor->letterSpacing), 4, 1, stream) != 1) goto out;

    int glyphsPtr;
    if (fileRead(&glyphsPtr, 4, 1, stream) != 1) goto out;

    int dataPtr;
    if (fileRead(&dataPtr, 4, 1, stream) != 1) goto out;

    textFontDescriptor->glyphs = (TextFontGlyph*)internal_malloc(textFontDescriptor->glyphCount * sizeof(TextFontGlyph));
    if (textFontDescriptor->glyphs == nullptr) {
        goto out;
    }

    if (fileRead(textFontDescriptor->glyphs, sizeof(TextFontGlyph), textFontDescriptor->glyphCount, stream) != textFontDescriptor->glyphCount) {
        goto out;
    }

    dataSize = textFontDescriptor->lineHeight * ((textFontDescriptor->glyphs[textFontDescriptor->glyphCount - 1].width + 7) >> 3) + textFontDescriptor->glyphs[textFontDescriptor->glyphCount - 1].dataOffset;
    textFontDescriptor->data = (unsigned char*)internal_malloc(dataSize);
    if (textFontDescriptor->data == nullptr) {
        goto out;
    }

    if (fileRead(textFontDescriptor->data, 1, dataSize, stream) != dataSize) {
        goto out;
    }

    rc = 0;

out:
    if (rc != 0) {
        if (textFontDescriptor->data != nullptr) {
            internal_free(textFontDescriptor->data);
            textFontDescriptor->data = nullptr;
        }

        if (textFontDescriptor->glyphs != nullptr) {
            internal_free(textFontDescriptor->glyphs);
            textFontDescriptor->glyphs = nullptr;
        }
    }

    if (stream != nullptr) {
        fileClose(stream);
    }

    return rc;
}

int fontManagerAdd(FontManager* fontManager)
{
    if (fontManager == nullptr) {
        return -1;
    }

    if (gFontManagersCount >= FONT_MANAGER_MAX) {
        return -1;
    }

    for (int index = fontManager->minFont; index < fontManager->maxFont; index++) {
        FontManager* existingFontManager;
        if (fontManagerFind(index, &existingFontManager)) {
            return -1;
        }
    }

    memcpy(&(gFontManagers[gFontManagersCount]), fontManager, sizeof(*fontManager));
    gFontManagersCount++;
    return 0;
}

static void textFontSetCurrentImpl(int font)
{
    if (font >= TEXT_FONT_MAX) {
        return;
    }

    TextFontDescriptor* textFontDescriptor = &(gTextFontDescriptors[font]);
    if (textFontDescriptor->glyphCount == 0) {
        return;
    }

    gCurrentTextFontDescriptor = textFontDescriptor;
}

int fontGetCurrent()
{
    return gCurrentFont;
}

void fontSetCurrent(int font)
{
    FontManager* fontManager;

    if (fontManagerFind(font, &fontManager)) {
        fontDrawText = fontManager->drawTextProc;
        fontGetLineHeight = fontManager->getLineHeightProc;
        fontGetStringWidth = fontManager->getStringWidthProc;
        fontGetCharacterWidth = fontManager->getCharacterWidthProc;
        fontGetMonospacedStringWidth = fontManager->getMonospacedStringWidthProc;
        fontGetLetterSpacing = fontManager->getLetterSpacingProc;
        fontGetBufferSize = fontManager->getBufferSizeProc;
        fontGetMonospacedCharacterWidth = fontManager->getMonospacedCharacterWidthProc;

        gCurrentFont = font;
        fontManager->setCurrentProc(font);
    }
}

static bool fontManagerFind(int font, FontManager** fontManagerPtr)
{
    for (int index = 0; index < gFontManagersCount; index++) {
        FontManager* fontManager = &(gFontManagers[index]);
        if (font >= fontManager->minFont && font <= fontManager->maxFont) {
            *fontManagerPtr = fontManager;
            return true;
        }
    }

    return false;
}

static void textFontDrawImpl(unsigned char* buf, const char* string, int length, int pitch, int color)
{
    if ((color & FONT_SHADOW) != 0) {
        color &= ~FONT_SHADOW;
        fontDrawText(buf + pitch + 1, string, length, pitch, _colorTable[0]);
    }

    int monospacedCharacterWidth;
    if ((color & FONT_MONO) != 0) {
        monospacedCharacterWidth = fontGetMonospacedCharacterWidth();
    }

    unsigned char* ptr = buf;
    while (*string != '\0') {
        unsigned char ch = static_cast<unsigned char>(*string++);
        if (ch < gCurrentTextFontDescriptor->glyphCount) {
            TextFontGlyph* glyph = &(gCurrentTextFontDescriptor->glyphs[ch]);

            unsigned char* end;
            if ((color & FONT_MONO) != 0) {
                end = ptr + monospacedCharacterWidth;
                ptr += (monospacedCharacterWidth - gCurrentTextFontDescriptor->letterSpacing - glyph->width) / 2;
            } else {
                end = ptr + glyph->width + gCurrentTextFontDescriptor->letterSpacing;
            }

            if (end - buf > length) {
                break;
            }

            unsigned char* glyphData = gCurrentTextFontDescriptor->data + glyph->dataOffset;
            for (int y = 0; y < gCurrentTextFontDescriptor->lineHeight; y++) {
                int bits = 0x80;
                for (int x = 0; x < glyph->width; x++) {
                    if (bits == 0) {
                        bits = 0x80;
                        glyphData++;
                    }

                    if ((*glyphData & bits) != 0) {
                        *ptr = color & 0xFF;
                    }

                    bits >>= 1;
                    ptr++;
                }
                glyphData++;
                ptr += pitch - glyph->width;
            }

            ptr = end;
        }
    }

    if ((color & FONT_UNDERLINE) != 0) {
        int length = ptr - buf;
        unsigned char* underlinePtr = buf + pitch * (gCurrentTextFontDescriptor->lineHeight - 1);
        for (int pix = 0; pix < length; pix++) {
            *underlinePtr++ = color & 0xFF;
        }
    }
}

static int textFontGetLineHeightImpl()
{
    return gCurrentTextFontDescriptor->lineHeight;
}

static int textFontGeStringWidthImpl(const char* string)
{
    int width = 0;

    while (*string != '\0') {
        unsigned char ch = static_cast<unsigned char>(*string++);
        if (ch < gCurrentTextFontDescriptor->glyphCount) {
            width += gCurrentTextFontDescriptor->letterSpacing + gCurrentTextFontDescriptor->glyphs[ch].width;
        }
    }

    return width;
}

static int textFontGetCharacterWidthImpl(int ch)
{
    return gCurrentTextFontDescriptor->glyphs[ch & 0xFF].width;
}

static int textFontGetMonospacedStringWidthImpl(const char* string)
{
    return fontGetMonospacedCharacterWidth() * strlen(string);
}

static int textFontGetLetterSpacingImpl()
{
    return gCurrentTextFontDescriptor->letterSpacing;
}

static int textFontGetBufferSizeImpl(const char* string)
{
    return fontGetStringWidth(string) * fontGetLineHeight();
}

static int textFontGetMonospacedCharacterWidthImpl()
{
    int width = 0;

    for (int index = 0; index < gCurrentTextFontDescriptor->glyphCount; index++) {
        TextFontGlyph* glyph = &(gCurrentTextFontDescriptor->glyphs[index]);
        if (width < glyph->width) {
            width = glyph->width;
        }
    }

    return width + gCurrentTextFontDescriptor->letterSpacing;
}

} // namespace fallout
