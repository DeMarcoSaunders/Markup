# Fonts Directory

This directory is used by the MarkUp UI library to load custom fonts for your application.

## Supported Font Formats

- `.ttf` (TrueType Font)
- `.otf` (OpenType Font)
- `.woff` (Web Open Font Format)

## How to Add Fonts

1. Place your font files in this directory
2. The font system will automatically scan for supported font files
3. Fonts will appear in the font selection dropdown in the General settings tab

## Example Fonts

The system looks for these common font files:

- `Roboto-Regular.ttf`
- `OpenSans-Regular.ttf`
- `Lato-Regular.ttf`
- `Montserrat-Regular.ttf`
- `SourceSansPro-Regular.ttf`

## Usage in Code

```c
// Initialize font system (done automatically in Theme_Init)
Theme_InitFontManager("fonts");

// Get available fonts
int font_count = Theme_GetFontCount();
for (int i = 0; i < font_count; i++) {
    printf("Font %d: %s\n", i, Theme_GetFontName(i));
}

// Set current font
Theme_SetCurrentFont(1); // Set to second font in list

// Get current font for drawing
Font current_font = Theme_GetCurrentFont();
DrawTextEx(current_font, "Hello World", (Vector2){100, 100}, 20, 1.0f, BLACK);
```

## Font Management Functions

- `Theme_GetFontCount()` - Get number of available fonts
- `Theme_GetFontName(index)` - Get font name by index
- `Theme_SetCurrentFont(index)` - Set active font
- `Theme_GetCurrentFont()` - Get current font for drawing
- `Theme_LoadFont(index, size)` - Load font at specific size
- `Theme_AddCustomFont(name, path)` - Add font programmatically

## Notes

- Fonts are loaded on-demand when first used
- The default system font is always available at index 0
- Font loading may take a moment for large font files
- Only one size per font is loaded at a time (reloading changes size)