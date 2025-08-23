#include "imagebox.h"
#include "theme.h"
#include <math.h>
#include <string.h>

// Helper function to calculate scaled rectangle maintaining aspect ratio
static Rectangle CalculateScaledRect(Rectangle bounds, Vector2 image_size, ImageScaleMode scale_mode, ImageAlignment alignment) {
    Rectangle result = bounds;
    
    if (image_size.x <= 0 || image_size.y <= 0) {
        return result;
    }
    
    float bounds_aspect = bounds.width / bounds.height;
    float image_aspect = image_size.x / image_size.y;
    
    switch (scale_mode) {
        case IMAGE_SCALE_STRETCH:
            // Use full bounds - no calculation needed
            break;
            
        case IMAGE_SCALE_FIT: {
            // Scale to fit within bounds
            if (image_aspect > bounds_aspect) {
                // Image is wider - fit to width
                result.width = bounds.width;
                result.height = bounds.width / image_aspect;
            } else {
                // Image is taller - fit to height
                result.height = bounds.height;
                result.width = bounds.height * image_aspect;
            }
            break;
        }
        
        case IMAGE_SCALE_FILL: {
            // Scale to fill bounds (may crop)
            if (image_aspect > bounds_aspect) {
                // Image is wider - fit to height
                result.height = bounds.height;
                result.width = bounds.height * image_aspect;
            } else {
                // Image is taller - fit to width
                result.width = bounds.width;
                result.height = bounds.width / image_aspect;
            }
            break;
        }
        
        case IMAGE_SCALE_NONE:
            // Use original image size
            result.width = image_size.x;
            result.height = image_size.y;
            break;
    }
    
    // Apply alignment
    float offset_x = 0, offset_y = 0;
    
    switch (alignment) {
        case IMAGE_ALIGN_TOP_LEFT:
            offset_x = 0;
            offset_y = 0;
            break;
        case IMAGE_ALIGN_TOP_CENTER:
            offset_x = (bounds.width - result.width) * 0.5f;
            offset_y = 0;
            break;
        case IMAGE_ALIGN_TOP_RIGHT:
            offset_x = bounds.width - result.width;
            offset_y = 0;
            break;
        case IMAGE_ALIGN_CENTER_LEFT:
            offset_x = 0;
            offset_y = (bounds.height - result.height) * 0.5f;
            break;
        case IMAGE_ALIGN_CENTER:
            offset_x = (bounds.width - result.width) * 0.5f;
            offset_y = (bounds.height - result.height) * 0.5f;
            break;
        case IMAGE_ALIGN_CENTER_RIGHT:
            offset_x = bounds.width - result.width;
            offset_y = (bounds.height - result.height) * 0.5f;
            break;
        case IMAGE_ALIGN_BOTTOM_LEFT:
            offset_x = 0;
            offset_y = bounds.height - result.height;
            break;
        case IMAGE_ALIGN_BOTTOM_CENTER:
            offset_x = (bounds.width - result.width) * 0.5f;
            offset_y = bounds.height - result.height;
            break;
        case IMAGE_ALIGN_BOTTOM_RIGHT:
            offset_x = bounds.width - result.width;
            offset_y = bounds.height - result.height;
            break;
    }
    
    result.x = bounds.x + offset_x;
    result.y = bounds.y + offset_y;
    
    return result;
}

ImageBox ImageBox_Create(Rectangle bounds) {
    ImageBox imagebox = {0};
    
    imagebox.bounds = bounds;
    imagebox.texture = (Texture2D){0}; // Empty texture
    imagebox.owns_texture = false;
    imagebox.scale_mode = IMAGE_SCALE_FIT;
    imagebox.alignment = IMAGE_ALIGN_CENTER;
    imagebox.tint = WHITE;
    imagebox.opacity = 1.0f;
    imagebox.is_visible = true;
    imagebox.is_clickable = false;
    imagebox.is_hovered = false;
    imagebox.is_clicked = false;
    
    imagebox.show_border = false;
    imagebox.border_color = GRAY;
    imagebox.border_width = 1.0f;
    imagebox.border_radius = 0.0f;
    
    imagebox.show_background = false;
    imagebox.background_color = WHITE;
    
    imagebox.rotation = 0.0f;
    imagebox.origin = (Vector2){0.5f, 0.5f}; // Center origin
    imagebox.flip_horizontal = false;
    imagebox.flip_vertical = false;
    
    return imagebox;
}

ImageBox ImageBox_CreateFromFile(Rectangle bounds, const char* image_path) {
    ImageBox imagebox = ImageBox_Create(bounds);
    ImageBox_LoadFromFile(&imagebox, image_path);
    return imagebox;
}

ImageBox ImageBox_CreateFromTexture(Rectangle bounds, Texture2D texture, bool take_ownership) {
    ImageBox imagebox = ImageBox_Create(bounds);
    ImageBox_SetTexture(&imagebox, texture, take_ownership);
    return imagebox;
}

void ImageBox_Update(ImageBox* imagebox) {
    if (!imagebox || !imagebox->is_visible) {
        return;
    }
    
    Vector2 mouse_pos = GetMousePosition();
    bool mouse_over = CheckCollisionPointRec(mouse_pos, imagebox->bounds);
    
    imagebox->is_hovered = mouse_over;
    imagebox->is_clicked = false;
    
    if (imagebox->is_clickable && mouse_over && IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
        imagebox->is_clicked = true;
    }
}

void ImageBox_Draw(const ImageBox* imagebox) {
    if (!imagebox || !imagebox->is_visible) {
        return;
    }
    
    // Draw background if enabled
    if (imagebox->show_background) {
        if (imagebox->border_radius > 0.0f) {
            // Check if this is a circular background (radius >= half of smallest dimension)
            float min_dimension = fminf(imagebox->bounds.width, imagebox->bounds.height);
            bool is_circular = imagebox->border_radius >= min_dimension / 2.0f;
            
            if (is_circular) {
                // Draw circular background
                Vector2 center = {
                    imagebox->bounds.x + imagebox->bounds.width / 2.0f,
                    imagebox->bounds.y + imagebox->bounds.height / 2.0f
                };
                float radius = min_dimension / 2.0f;
                DrawCircle((int)center.x, (int)center.y, radius, imagebox->background_color);
            } else {
                // Draw rounded rectangle background
                float roundness = imagebox->border_radius / (imagebox->bounds.height / 2.0f);
                roundness = fminf(roundness, 1.0f); // Clamp to max roundness
                DrawRectangleRounded(imagebox->bounds, roundness, 8, imagebox->background_color);
            }
        } else {
            DrawRectangleRec(imagebox->bounds, imagebox->background_color);
        }
    }
    
    // Draw image if texture is valid
    if (imagebox->texture.id > 0) {
        Vector2 image_size = {(float)imagebox->texture.width, (float)imagebox->texture.height};
        Rectangle image_rect = CalculateScaledRect(imagebox->bounds, image_size, imagebox->scale_mode, imagebox->alignment);
        
        // Apply opacity to tint
        Color draw_tint = imagebox->tint;
        draw_tint.a = (unsigned char)(draw_tint.a * imagebox->opacity);
        
        // Calculate source rectangle for potential cropping
        Rectangle source_rect = {0, 0, image_size.x, image_size.y};
        
        // Handle flipping
        if (imagebox->flip_horizontal) {
            source_rect.width = -source_rect.width;
        }
        if (imagebox->flip_vertical) {
            source_rect.height = -source_rect.height;
        }
        
        // For FILL mode, we might need to crop the image
        if (imagebox->scale_mode == IMAGE_SCALE_FILL) {
            // Calculate which part of the image to show
            Rectangle clipped_rect = {
                fmaxf(image_rect.x, imagebox->bounds.x),
                fmaxf(image_rect.y, imagebox->bounds.y),
                fminf(image_rect.x + image_rect.width, imagebox->bounds.x + imagebox->bounds.width) - fmaxf(image_rect.x, imagebox->bounds.x),
                fminf(image_rect.y + image_rect.height, imagebox->bounds.y + imagebox->bounds.height) - fmaxf(image_rect.y, imagebox->bounds.y)
            };
            
            if (clipped_rect.width > 0 && clipped_rect.height > 0) {
                // Adjust source rectangle for cropping
                float scale_x = image_size.x / image_rect.width;
                float scale_y = image_size.y / image_rect.height;
                
                source_rect.x = (clipped_rect.x - image_rect.x) * scale_x;
                source_rect.y = (clipped_rect.y - image_rect.y) * scale_y;
                source_rect.width = clipped_rect.width * scale_x;
                source_rect.height = clipped_rect.height * scale_y;
                
                image_rect = clipped_rect;
            }
        }
        
        if (imagebox->rotation != 0.0f) {
            // Draw with rotation
            Vector2 origin = {
                image_rect.width * imagebox->origin.x,
                image_rect.height * imagebox->origin.y
            };
            Vector2 position = {
                image_rect.x + origin.x,
                image_rect.y + origin.y
            };
            
            DrawTexturePro(imagebox->texture, source_rect, 
                          (Rectangle){position.x, position.y, image_rect.width, image_rect.height},
                          origin, imagebox->rotation, draw_tint);
        } else {
            // Draw normally
            DrawTexturePro(imagebox->texture, source_rect, image_rect, (Vector2){0, 0}, 0.0f, draw_tint);
        }
    }
    
    // Draw border if enabled
    if (imagebox->show_border && imagebox->border_width > 0.0f) {
        if (imagebox->border_radius > 0.0f) {
            // Check if this is a circular border (radius >= half of smallest dimension)
            float min_dimension = fminf(imagebox->bounds.width, imagebox->bounds.height);
            bool is_circular = imagebox->border_radius >= min_dimension / 2.0f;
            
            if (is_circular) {
                // Draw circular border
                Vector2 center = {
                    imagebox->bounds.x + imagebox->bounds.width / 2.0f,
                    imagebox->bounds.y + imagebox->bounds.height / 2.0f
                };
                float radius = min_dimension / 2.0f;
                
                // Draw multiple circles for border thickness
                for (int i = 0; i < (int)imagebox->border_width; i++) {
                    DrawCircleLines((int)center.x, (int)center.y, radius - i, imagebox->border_color);
                }
            } else {
                // Draw rounded rectangle border with matching radius
                float roundness = imagebox->border_radius / (imagebox->bounds.height / 2.0f);
                roundness = fminf(roundness, 1.0f); // Clamp to max roundness
                DrawRectangleRoundedLines(imagebox->bounds, roundness, 8, imagebox->border_color);
            }
        } else {
            DrawRectangleLinesEx(imagebox->bounds, imagebox->border_width, imagebox->border_color);
        }
    }
}

void ImageBox_Destroy(ImageBox* imagebox) {
    if (!imagebox) return;
    
    // Unload texture if we own it
    if (imagebox->owns_texture && imagebox->texture.id > 0) {
        UnloadTexture(imagebox->texture);
    }
    
    // Reset to default state
    *imagebox = (ImageBox){0};
}

void ImageBox_SetTexture(ImageBox* imagebox, Texture2D texture, bool take_ownership) {
    if (!imagebox) return;
    
    // Clean up previous texture if we owned it
    if (imagebox->owns_texture && imagebox->texture.id > 0) {
        UnloadTexture(imagebox->texture);
    }
    
    imagebox->texture = texture;
    imagebox->owns_texture = take_ownership;
}

void ImageBox_LoadFromFile(ImageBox* imagebox, const char* image_path) {
    if (!imagebox || !image_path) return;
    
    // Clean up previous texture if we owned it
    if (imagebox->owns_texture && imagebox->texture.id > 0) {
        UnloadTexture(imagebox->texture);
    }
    
    // Load new texture
    Texture2D new_texture = LoadTexture(image_path);
    imagebox->texture = new_texture;
    imagebox->owns_texture = true; // We loaded it, so we own it
}

void ImageBox_ClearTexture(ImageBox* imagebox) {
    if (!imagebox) return;
    
    // Clean up texture if we own it
    if (imagebox->owns_texture && imagebox->texture.id > 0) {
        UnloadTexture(imagebox->texture);
    }
    
    imagebox->texture = (Texture2D){0};
    imagebox->owns_texture = false;
}

void ImageBox_SetScaleMode(ImageBox* imagebox, ImageScaleMode mode) {
    if (imagebox) imagebox->scale_mode = mode;
}

void ImageBox_SetAlignment(ImageBox* imagebox, ImageAlignment alignment) {
    if (imagebox) imagebox->alignment = alignment;
}

void ImageBox_SetTint(ImageBox* imagebox, Color tint) {
    if (imagebox) imagebox->tint = tint;
}

void ImageBox_SetOpacity(ImageBox* imagebox, float opacity) {
    if (imagebox) imagebox->opacity = opacity < 0.0f ? 0.0f : (opacity > 1.0f ? 1.0f : opacity);
}

void ImageBox_SetBorder(ImageBox* imagebox, bool show, Color color, float width) {
    if (!imagebox) return;
    imagebox->show_border = show;
    imagebox->border_color = color;
    imagebox->border_width = width;
}

void ImageBox_SetBorderRadius(ImageBox* imagebox, float radius) {
    if (imagebox) imagebox->border_radius = radius;
}

void ImageBox_SetBackground(ImageBox* imagebox, bool show, Color color) {
    if (!imagebox) return;
    imagebox->show_background = show;
    imagebox->background_color = color;
}

void ImageBox_SetRotation(ImageBox* imagebox, float rotation, Vector2 origin) {
    if (!imagebox) return;
    imagebox->rotation = rotation;
    imagebox->origin = origin;
}

void ImageBox_SetFlip(ImageBox* imagebox, bool horizontal, bool vertical) {
    if (!imagebox) return;
    imagebox->flip_horizontal = horizontal;
    imagebox->flip_vertical = vertical;
}

void ImageBox_SetClickable(ImageBox* imagebox, bool clickable) {
    if (imagebox) imagebox->is_clickable = clickable;
}

void ImageBox_SetVisible(ImageBox* imagebox, bool visible) {
    if (imagebox) imagebox->is_visible = visible;
}

bool ImageBox_IsClicked(const ImageBox* imagebox) {
    return imagebox ? imagebox->is_clicked : false;
}

bool ImageBox_IsHovered(const ImageBox* imagebox) {
    return imagebox ? imagebox->is_hovered : false;
}