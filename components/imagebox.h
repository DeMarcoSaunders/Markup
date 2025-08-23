#ifndef IMAGEBOX_H
#define IMAGEBOX_H

#include "raylib.h"
#include <stdbool.h>

typedef enum {
    IMAGE_SCALE_STRETCH,    // Stretch to fill the entire bounds
    IMAGE_SCALE_FIT,        // Scale to fit within bounds while maintaining aspect ratio
    IMAGE_SCALE_FILL,       // Scale to fill bounds while maintaining aspect ratio (may crop)
    IMAGE_SCALE_NONE        // Display at original size (may be clipped)
} ImageScaleMode;

typedef enum {
    IMAGE_ALIGN_TOP_LEFT,
    IMAGE_ALIGN_TOP_CENTER,
    IMAGE_ALIGN_TOP_RIGHT,
    IMAGE_ALIGN_CENTER_LEFT,
    IMAGE_ALIGN_CENTER,
    IMAGE_ALIGN_CENTER_RIGHT,
    IMAGE_ALIGN_BOTTOM_LEFT,
    IMAGE_ALIGN_BOTTOM_CENTER,
    IMAGE_ALIGN_BOTTOM_RIGHT
} ImageAlignment;

typedef struct {
    Rectangle bounds;
    Texture2D texture;
    bool owns_texture;      // Whether this component should unload the texture
    ImageScaleMode scale_mode;
    ImageAlignment alignment;
    Color tint;
    float opacity;
    bool is_visible;
    bool is_clickable;
    bool is_hovered;
    bool is_clicked;
    
    // Border and styling
    bool show_border;
    Color border_color;
    float border_width;
    float border_radius;
    
    // Background (useful for icons with transparency)
    bool show_background;
    Color background_color;
    
    // Rotation and effects
    float rotation;
    Vector2 origin;
    bool flip_horizontal;
    bool flip_vertical;
} ImageBox;

// Function declarations
ImageBox ImageBox_Create(Rectangle bounds);
ImageBox ImageBox_CreateFromFile(Rectangle bounds, const char* image_path);
ImageBox ImageBox_CreateFromTexture(Rectangle bounds, Texture2D texture, bool take_ownership);
void ImageBox_Update(ImageBox* imagebox);
void ImageBox_Draw(const ImageBox* imagebox);
void ImageBox_Destroy(ImageBox* imagebox);

// Image management
void ImageBox_SetTexture(ImageBox* imagebox, Texture2D texture, bool take_ownership);
void ImageBox_LoadFromFile(ImageBox* imagebox, const char* image_path);
void ImageBox_ClearTexture(ImageBox* imagebox);

// Styling functions
void ImageBox_SetScaleMode(ImageBox* imagebox, ImageScaleMode mode);
void ImageBox_SetAlignment(ImageBox* imagebox, ImageAlignment alignment);
void ImageBox_SetTint(ImageBox* imagebox, Color tint);
void ImageBox_SetOpacity(ImageBox* imagebox, float opacity);
void ImageBox_SetBorder(ImageBox* imagebox, bool show, Color color, float width);
void ImageBox_SetBorderRadius(ImageBox* imagebox, float radius);
void ImageBox_SetBackground(ImageBox* imagebox, bool show, Color color);
void ImageBox_SetRotation(ImageBox* imagebox, float rotation, Vector2 origin);
void ImageBox_SetFlip(ImageBox* imagebox, bool horizontal, bool vertical);

// Interaction
void ImageBox_SetClickable(ImageBox* imagebox, bool clickable);
void ImageBox_SetVisible(ImageBox* imagebox, bool visible);
bool ImageBox_IsClicked(const ImageBox* imagebox);
bool ImageBox_IsHovered(const ImageBox* imagebox);

#endif // IMAGEBOX_H