#include "background_image.h"
#include "error_handling.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

BackgroundImage BackgroundImage_Create(void) {
    BackgroundImage bg = {0};
    
    // Initialize with default values
    bg.is_loaded = false;
    bg.is_valid = false;
    bg.error_message[0] = '\0';
    bg.file_path[0] = '\0';
    
    // Default dimensions
    bg.original_width = 0;
    bg.original_height = 0;
    bg.display_width = 0;
    bg.display_height = 0;
    
    // Default scaling and positioning
    bg.scale_mode = BACKGROUND_SCALE_STRETCH;
    bg.position = BACKGROUND_POSITION_CENTER;
    bg.repeat_mode = BACKGROUND_REPEAT_NONE;
    bg.blend_mode = BACKGROUND_BLEND_NORMAL;
    
    // Default offset and tiling
    bg.offset = (Vector2){0, 0};
    bg.tile_size = (Vector2){0, 0};
    bg.tile_offset = (Vector2){0, 0};
    
    // Default color and opacity
    bg.tint_color = WHITE;
    bg.opacity = 1.0f;
    
    // Default animation
    bg.is_animated = false;
    bg.animation_speed = 1.0f;
    bg.current_frame = 0.0f;
    bg.frame_count = 1;
    bg.current_frame_index = 0;
    
    // Default caching
    bg.use_cache = false;
    bg.cache_dirty = true;
    bg.cached_texture.id = 0;
    
    // Memory management
    bg.should_free_texture = false;
    
    return bg;
}

bool BackgroundImage_Init(BackgroundImage* bg) {
    RETURN_IF_NULL(bg);
    
    *bg = BackgroundImage_Create();
    return true;
}

void BackgroundImage_Destroy(BackgroundImage* bg) {
    if (!bg) return;
    
    BackgroundImage_Unload(bg);
    
    // Free cached texture if it exists
    if (bg->cached_texture.id != 0) {
        UnloadRenderTexture(bg->cached_texture);
        bg->cached_texture.id = 0;
    }
}

bool BackgroundImage_LoadFromFile(BackgroundImage* bg, const char* file_path) {
    RETURN_IF_NULL(bg);
    RETURN_IF_NULL(file_path);
    
    // Unload previous image
    BackgroundImage_Unload(bg);
    
    // Load image
    Image image = LoadImage(file_path);
    if (image.data == NULL) {
        strcpy(bg->error_message, "Failed to load image file");
        return false;
    }
    
    // Convert to texture
    Texture2D texture = LoadTextureFromImage(image);
    UnloadImage(image);
    
    if (texture.id == 0) {
        strcpy(bg->error_message, "Failed to create texture from image");
        return false;
    }
    
    // Set up background image
    bg->texture = texture;
    bg->image = image;
    strncpy(bg->file_path, file_path, sizeof(bg->file_path) - 1);
    bg->file_path[sizeof(bg->file_path) - 1] = '\0';
    
    bg->original_width = texture.width;
    bg->original_height = texture.height;
    bg->display_width = texture.width;
    bg->display_height = texture.height;
    
    bg->is_loaded = true;
    bg->is_valid = true;
    bg->should_free_texture = true;
    bg->cache_dirty = true;
    
    return true;
}

bool BackgroundImage_LoadFromMemory(BackgroundImage* bg, const unsigned char* data, int data_size) {
    RETURN_IF_NULL(bg);
    RETURN_IF_NULL(data);
    
    // Unload previous image
    BackgroundImage_Unload(bg);
    
    // Load image from memory
    Image image = LoadImageFromMemory(".png", data, data_size);
    if (image.data == NULL) {
        strcpy(bg->error_message, "Failed to load image from memory");
        return false;
    }
    
    // Convert to texture
    Texture2D texture = LoadTextureFromImage(image);
    UnloadImage(image);
    
    if (texture.id == 0) {
        strcpy(bg->error_message, "Failed to create texture from memory image");
        return false;
    }
    
    // Set up background image
    bg->texture = texture;
    bg->original_width = texture.width;
    bg->original_height = texture.height;
    bg->display_width = texture.width;
    bg->display_height = texture.height;
    
    bg->is_loaded = true;
    bg->is_valid = true;
    bg->should_free_texture = true;
    bg->cache_dirty = true;
    
    return true;
}

bool BackgroundImage_LoadFromImage(BackgroundImage* bg, Image image) {
    RETURN_IF_NULL(bg);
    
    // Unload previous image
    BackgroundImage_Unload(bg);
    
    // Convert to texture
    Texture2D texture = LoadTextureFromImage(image);
    if (texture.id == 0) {
        strcpy(bg->error_message, "Failed to create texture from image");
        return false;
    }
    
    // Set up background image
    bg->texture = texture;
    bg->image = image;
    bg->original_width = texture.width;
    bg->original_height = texture.height;
    bg->display_width = texture.width;
    bg->display_height = texture.height;
    
    bg->is_loaded = true;
    bg->is_valid = true;
    bg->should_free_texture = true;
    bg->cache_dirty = true;
    
    return true;
}

bool BackgroundImage_LoadFromTexture(BackgroundImage* bg, Texture2D texture) {
    RETURN_IF_NULL(bg);
    
    // Unload previous image
    BackgroundImage_Unload(bg);
    
    // Set up background image
    bg->texture = texture;
    bg->original_width = texture.width;
    bg->original_height = texture.height;
    bg->display_width = texture.width;
    bg->display_height = texture.height;
    
    bg->is_loaded = true;
    bg->is_valid = true;
    bg->should_free_texture = false; // Don't free external texture
    bg->cache_dirty = true;
    
    return true;
}

void BackgroundImage_Unload(BackgroundImage* bg) {
    if (!bg) return;
    
    if (bg->is_loaded && bg->should_free_texture) {
        UnloadTexture(bg->texture);
    }
    
    bg->is_loaded = false;
    bg->is_valid = false;
    bg->texture.id = 0;
    bg->image.data = NULL;
    bg->file_path[0] = '\0';
    bg->error_message[0] = '\0';
}

bool BackgroundImage_IsLoaded(const BackgroundImage* bg) {
    return bg && bg->is_loaded;
}

bool BackgroundImage_IsValid(const BackgroundImage* bg) {
    return bg && bg->is_valid;
}

const char* BackgroundImage_GetError(const BackgroundImage* bg) {
    return bg ? bg->error_message : "Invalid background image";
}

const char* BackgroundImage_GetFilePath(const BackgroundImage* bg) {
    return bg ? bg->file_path : "";
}

int BackgroundImage_GetWidth(const BackgroundImage* bg) {
    return bg ? bg->original_width : 0;
}

int BackgroundImage_GetHeight(const BackgroundImage* bg) {
    return bg ? bg->original_height : 0;
}

Vector2 BackgroundImage_GetSize(const BackgroundImage* bg) {
    return bg ? (Vector2){(float)bg->original_width, (float)bg->original_height} : (Vector2){0, 0};
}

void BackgroundImage_SetScaleMode(BackgroundImage* bg, BackgroundScaleMode mode) {
    if (!bg) return;
    bg->scale_mode = mode;
    bg->cache_dirty = true;
}

void BackgroundImage_SetPosition(BackgroundImage* bg, BackgroundPosition position) {
    if (!bg) return;
    bg->position = position;
    bg->cache_dirty = true;
}

void BackgroundImage_SetRepeatMode(BackgroundImage* bg, BackgroundRepeatMode mode) {
    if (!bg) return;
    bg->repeat_mode = mode;
    bg->cache_dirty = true;
}

void BackgroundImage_SetBlendMode(BackgroundImage* bg, BackgroundBlendMode mode) {
    if (!bg) return;
    bg->blend_mode = mode;
    bg->cache_dirty = true;
}

BackgroundScaleMode BackgroundImage_GetScaleMode(const BackgroundImage* bg) {
    return bg ? bg->scale_mode : BACKGROUND_SCALE_STRETCH;
}

BackgroundPosition BackgroundImage_GetPosition(const BackgroundImage* bg) {
    return bg ? bg->position : BACKGROUND_POSITION_CENTER;
}

BackgroundRepeatMode BackgroundImage_GetRepeatMode(const BackgroundImage* bg) {
    return bg ? bg->repeat_mode : BACKGROUND_REPEAT_NONE;
}

BackgroundBlendMode BackgroundImage_GetBlendMode(const BackgroundImage* bg) {
    return bg ? bg->blend_mode : BACKGROUND_BLEND_NORMAL;
}

void BackgroundImage_SetOffset(BackgroundImage* bg, Vector2 offset) {
    if (!bg) return;
    bg->offset = offset;
    bg->cache_dirty = true;
}

void BackgroundImage_SetTileSize(BackgroundImage* bg, Vector2 tile_size) {
    if (!bg) return;
    bg->tile_size = tile_size;
    bg->cache_dirty = true;
}

void BackgroundImage_SetTileOffset(BackgroundImage* bg, Vector2 tile_offset) {
    if (!bg) return;
    bg->tile_offset = tile_offset;
    bg->cache_dirty = true;
}

Vector2 BackgroundImage_GetOffset(const BackgroundImage* bg) {
    return bg ? bg->offset : (Vector2){0, 0};
}

Vector2 BackgroundImage_GetTileSize(const BackgroundImage* bg) {
    return bg ? bg->tile_size : (Vector2){0, 0};
}

Vector2 BackgroundImage_GetTileOffset(const BackgroundImage* bg) {
    return bg ? bg->tile_offset : (Vector2){0, 0};
}

void BackgroundImage_SetTintColor(BackgroundImage* bg, Color color) {
    if (!bg) return;
    bg->tint_color = color;
    bg->cache_dirty = true;
}

void BackgroundImage_SetOpacity(BackgroundImage* bg, float opacity) {
    if (!bg) return;
    bg->opacity = fmaxf(0.0f, fminf(1.0f, opacity));
    bg->cache_dirty = true;
}

Color BackgroundImage_GetTintColor(const BackgroundImage* bg) {
    return bg ? bg->tint_color : WHITE;
}

float BackgroundImage_GetOpacity(const BackgroundImage* bg) {
    return bg ? bg->opacity : 1.0f;
}

void BackgroundImage_SetAnimated(BackgroundImage* bg, bool animated) {
    if (!bg) return;
    bg->is_animated = animated;
}

void BackgroundImage_SetAnimationSpeed(BackgroundImage* bg, float speed) {
    if (!bg) return;
    bg->animation_speed = fmaxf(0.0f, speed);
}

void BackgroundImage_SetFrameCount(BackgroundImage* bg, int frame_count) {
    if (!bg) return;
    bg->frame_count = fmaxf(1, frame_count);
}

bool BackgroundImage_IsAnimated(const BackgroundImage* bg) {
    return bg && bg->is_animated;
}

float BackgroundImage_GetAnimationSpeed(const BackgroundImage* bg) {
    return bg ? bg->animation_speed : 1.0f;
}

int BackgroundImage_GetFrameCount(const BackgroundImage* bg) {
    return bg ? bg->frame_count : 1;
}

int BackgroundImage_GetCurrentFrame(const BackgroundImage* bg) {
    return bg ? bg->current_frame_index : 0;
}

void BackgroundImage_SetUseCache(BackgroundImage* bg, bool use_cache) {
    if (!bg) return;
    bg->use_cache = use_cache;
}

void BackgroundImage_SetCacheDirty(BackgroundImage* bg, bool dirty) {
    if (!bg) return;
    bg->cache_dirty = dirty;
}

bool BackgroundImage_UseCache(const BackgroundImage* bg) {
    return bg && bg->use_cache;
}

bool BackgroundImage_IsCacheDirty(const BackgroundImage* bg) {
    return bg && bg->cache_dirty;
}

void BackgroundImage_Update(BackgroundImage* bg) {
    if (!bg || !bg->is_loaded || !bg->is_animated) return;
    
    float delta_time = GetFrameTime();
    bg->current_frame += bg->animation_speed * delta_time;
    
    if (bg->current_frame >= (float)bg->frame_count) {
        bg->current_frame = 0.0f;
    }
    
    bg->current_frame_index = (int)bg->current_frame;
    bg->cache_dirty = true;
}

void BackgroundImage_Draw(const BackgroundImage* bg, Rectangle dest_rect) {
    if (!bg || !bg->is_loaded || !bg->is_valid) return;
    
    Rectangle src_rect = {0, 0, (float)bg->original_width, (float)bg->original_height};
    
    // Apply tint and opacity
    Color tint = bg->tint_color;
    tint.a = (unsigned char)(tint.a * bg->opacity);
    
    // Draw based on scale mode
    switch (bg->scale_mode) {
        case BACKGROUND_SCALE_STRETCH:
            DrawTexturePro(bg->texture, src_rect, dest_rect, bg->offset, 0.0f, tint);
            break;
            
        case BACKGROUND_SCALE_FIT:
        case BACKGROUND_SCALE_COVER:
        case BACKGROUND_SCALE_CENTER: {
            Vector2 scale = BackgroundImage_CalculateScale(bg, dest_rect);
            Vector2 pos = BackgroundImage_CalculatePosition(bg, dest_rect, scale);
            Rectangle scaled_rect = {pos.x, pos.y, src_rect.width * scale.x, src_rect.height * scale.y};
            DrawTexturePro(bg->texture, src_rect, scaled_rect, bg->offset, 0.0f, tint);
            break;
        }
        
        case BACKGROUND_SCALE_TILE:
            BackgroundImage_DrawTiled(bg, dest_rect);
            break;
            
        case BACKGROUND_SCALE_NONE:
            DrawTexturePro(bg->texture, src_rect, dest_rect, bg->offset, 0.0f, tint);
            break;
    }
}

void BackgroundImage_DrawScaled(const BackgroundImage* bg, Rectangle dest_rect, Vector2 scale) {
    if (!bg || !bg->is_loaded || !bg->is_valid) return;
    
    Rectangle src_rect = {0, 0, (float)bg->original_width, (float)bg->original_height};
    Rectangle scaled_rect = {
        dest_rect.x + bg->offset.x,
        dest_rect.y + bg->offset.y,
        src_rect.width * scale.x,
        src_rect.height * scale.y
    };
    
    Color tint = bg->tint_color;
    tint.a = (unsigned char)(tint.a * bg->opacity);
    
    DrawTexturePro(bg->texture, src_rect, scaled_rect, (Vector2){0, 0}, 0.0f, tint);
}

void BackgroundImage_DrawTiled(const BackgroundImage* bg, Rectangle dest_rect) {
    if (!bg || !bg->is_loaded || !bg->is_valid) return;
    
    Vector2 tile_size = bg->tile_size.x > 0 && bg->tile_size.y > 0 ? 
                       bg->tile_size : (Vector2){(float)bg->original_width, (float)bg->original_height};
    
    Rectangle src_rect = {0, 0, tile_size.x, tile_size.y};
    Color tint = bg->tint_color;
    tint.a = (unsigned char)(tint.a * bg->opacity);
    
    // Calculate tile positions
    int tiles_x = (int)ceilf(dest_rect.width / tile_size.x);
    int tiles_y = (int)ceilf(dest_rect.height / tile_size.y);
    
    for (int y = 0; y < tiles_y; y++) {
        for (int x = 0; x < tiles_x; x++) {
            Rectangle tile_rect = {
                dest_rect.x + x * tile_size.x + bg->tile_offset.x,
                dest_rect.y + y * tile_size.y + bg->tile_offset.y,
                tile_size.x,
                tile_size.y
            };
            
            DrawTexturePro(bg->texture, src_rect, tile_rect, (Vector2){0, 0}, 0.0f, tint);
        }
    }
}

Rectangle BackgroundImage_CalculateDestRect(const BackgroundImage* bg, Rectangle container_rect) {
    if (!bg || !bg->is_loaded) return container_rect;
    
    switch (bg->scale_mode) {
        case BACKGROUND_SCALE_STRETCH:
        case BACKGROUND_SCALE_TILE:
            return container_rect;
            
        case BACKGROUND_SCALE_FIT: {
            float scale_x = container_rect.width / bg->original_width;
            float scale_y = container_rect.height / bg->original_height;
            float scale = fminf(scale_x, scale_y);
            
            float width = bg->original_width * scale;
            float height = bg->original_height * scale;
            float x = container_rect.x + (container_rect.width - width) * 0.5f;
            float y = container_rect.y + (container_rect.height - height) * 0.5f;
            
            return (Rectangle){x, y, width, height};
        }
        
        case BACKGROUND_SCALE_COVER: {
            float scale_x = container_rect.width / bg->original_width;
            float scale_y = container_rect.height / bg->original_height;
            float scale = fmaxf(scale_x, scale_y);
            
            float width = bg->original_width * scale;
            float height = bg->original_height * scale;
            float x = container_rect.x + (container_rect.width - width) * 0.5f;
            float y = container_rect.y + (container_rect.height - height) * 0.5f;
            
            return (Rectangle){x, y, width, height};
        }
        
        case BACKGROUND_SCALE_CENTER:
        case BACKGROUND_SCALE_NONE: {
            float x = container_rect.x + (container_rect.width - bg->original_width) * 0.5f;
            float y = container_rect.y + (container_rect.height - bg->original_height) * 0.5f;
            
            return (Rectangle){x, y, (float)bg->original_width, (float)bg->original_height};
        }
    }
    
    return container_rect;
}

Vector2 BackgroundImage_CalculateScale(const BackgroundImage* bg, Rectangle container_rect) {
    if (!bg || !bg->is_loaded) return (Vector2){1, 1};
    
    switch (bg->scale_mode) {
        case BACKGROUND_SCALE_STRETCH:
            return (Vector2){
                container_rect.width / bg->original_width,
                container_rect.height / bg->original_height
            };
            
        case BACKGROUND_SCALE_FIT: {
            float scale_x = container_rect.width / bg->original_width;
            float scale_y = container_rect.height / bg->original_height;
            float scale = fminf(scale_x, scale_y);
            return (Vector2){scale, scale};
        }
        
        case BACKGROUND_SCALE_COVER: {
            float scale_x = container_rect.width / bg->original_width;
            float scale_y = container_rect.height / bg->original_height;
            float scale = fmaxf(scale_x, scale_y);
            return (Vector2){scale, scale};
        }
        
        case BACKGROUND_SCALE_CENTER:
        case BACKGROUND_SCALE_NONE:
        case BACKGROUND_SCALE_TILE:
            return (Vector2){1, 1};
    }
    
    return (Vector2){1, 1};
}

Vector2 BackgroundImage_CalculatePosition(const BackgroundImage* bg, Rectangle container_rect, Vector2 scale) {
    if (!bg || !bg->is_loaded) return (Vector2){container_rect.x, container_rect.y};
    
    float scaled_width = bg->original_width * scale.x;
    float scaled_height = bg->original_height * scale.y;
    
    float x = container_rect.x;
    float y = container_rect.y;
    
    switch (bg->position) {
        case BACKGROUND_POSITION_TOP_LEFT:
            break;
        case BACKGROUND_POSITION_TOP_CENTER:
            x += (container_rect.width - scaled_width) * 0.5f;
            break;
        case BACKGROUND_POSITION_TOP_RIGHT:
            x += container_rect.width - scaled_width;
            break;
        case BACKGROUND_POSITION_CENTER_LEFT:
            y += (container_rect.height - scaled_height) * 0.5f;
            break;
        case BACKGROUND_POSITION_CENTER:
            x += (container_rect.width - scaled_width) * 0.5f;
            y += (container_rect.height - scaled_height) * 0.5f;
            break;
        case BACKGROUND_POSITION_CENTER_RIGHT:
            x += container_rect.width - scaled_width;
            y += (container_rect.height - scaled_height) * 0.5f;
            break;
        case BACKGROUND_POSITION_BOTTOM_LEFT:
            y += container_rect.height - scaled_height;
            break;
        case BACKGROUND_POSITION_BOTTOM_CENTER:
            x += (container_rect.width - scaled_width) * 0.5f;
            y += container_rect.height - scaled_height;
            break;
        case BACKGROUND_POSITION_BOTTOM_RIGHT:
            x += container_rect.width - scaled_width;
            y += container_rect.height - scaled_height;
            break;
    }
    
    return (Vector2){x + bg->offset.x, y + bg->offset.y};
}

// Background Image Manager Implementation
BackgroundImageManager* BackgroundImageManager_Create(void) {
    BackgroundImageManager* manager = Safe_Malloc(sizeof(BackgroundImageManager));
    if (!manager) return NULL;
    
    BackgroundImageManager_Init(manager);
    return manager;
}

bool BackgroundImageManager_Init(BackgroundImageManager* manager) {
    RETURN_IF_NULL(manager);
    
    manager->images = NULL;
    manager->image_count = 0;
    manager->image_capacity = 0;
    strcpy(manager->cache_directory, "./cache");
    manager->enable_caching = true;
    manager->max_cache_size = 100 * 1024 * 1024; // 100MB
    
    return true;
}

void BackgroundImageManager_Destroy(BackgroundImageManager* manager) {
    if (!manager) return;
    
    BackgroundImageManager_ClearImages(manager);
    Safe_Free(manager->images);
    manager->images = NULL;
    manager->image_count = 0;
    manager->image_capacity = 0;
}

bool BackgroundImageManager_AddImage(BackgroundImageManager* manager, BackgroundImage* bg) {
    RETURN_IF_NULL(manager);
    RETURN_IF_NULL(bg);
    
    if (manager->image_count >= manager->image_capacity) {
        int new_capacity = manager->image_capacity == 0 ? 8 : manager->image_capacity * 2;
        BackgroundImage** new_images = Safe_Realloc(manager->images, new_capacity * sizeof(BackgroundImage*));
        if (!new_images) return false;
        
        manager->images = new_images;
        manager->image_capacity = new_capacity;
    }
    
    manager->images[manager->image_count++] = bg;
    return true;
}

bool BackgroundImageManager_RemoveImage(BackgroundImageManager* manager, BackgroundImage* bg) {
    RETURN_IF_NULL(manager);
    RETURN_IF_NULL(bg);
    
    for (int i = 0; i < manager->image_count; i++) {
        if (manager->images[i] == bg) {
            return BackgroundImageManager_RemoveImageByIndex(manager, i);
        }
    }
    
    return false;
}

bool BackgroundImageManager_RemoveImageByIndex(BackgroundImageManager* manager, int index) {
    RETURN_IF_NULL(manager);
    
    if (index < 0 || index >= manager->image_count) return false;
    
    // Shift remaining images
    for (int i = index; i < manager->image_count - 1; i++) {
        manager->images[i] = manager->images[i + 1];
    }
    
    manager->image_count--;
    return true;
}

void BackgroundImageManager_ClearImages(BackgroundImageManager* manager) {
    if (!manager) return;
    
    for (int i = 0; i < manager->image_count; i++) {
        BackgroundImage_Destroy(manager->images[i]);
        Safe_Free(manager->images[i]);
    }
    
    manager->image_count = 0;
}

int BackgroundImageManager_GetImageCount(const BackgroundImageManager* manager) {
    return manager ? manager->image_count : 0;
}

void BackgroundImageManager_SetCacheDirectory(BackgroundImageManager* manager, const char* directory) {
    if (!manager || !directory) return;
    strncpy(manager->cache_directory, directory, sizeof(manager->cache_directory) - 1);
    manager->cache_directory[sizeof(manager->cache_directory) - 1] = '\0';
}

void BackgroundImageManager_SetEnableCaching(BackgroundImageManager* manager, bool enable) {
    if (!manager) return;
    manager->enable_caching = enable;
}

void BackgroundImageManager_SetMaxCacheSize(BackgroundImageManager* manager, size_t max_size) {
    if (!manager) return;
    manager->max_cache_size = max_size;
}

const char* BackgroundImageManager_GetCacheDirectory(const BackgroundImageManager* manager) {
    return manager ? manager->cache_directory : "";
}

bool BackgroundImageManager_IsCachingEnabled(const BackgroundImageManager* manager) {
    return manager && manager->enable_caching;
}

size_t BackgroundImageManager_GetMaxCacheSize(const BackgroundImageManager* manager) {
    return manager ? manager->max_cache_size : 0;
}

void BackgroundImageManager_ClearCache(BackgroundImageManager* manager) {
    if (!manager) return;
    
    // Implementation would clear cached files from disk
    // This is a placeholder for the actual implementation
}

size_t BackgroundImageManager_GetCacheSize(const BackgroundImageManager* manager) {
    if (!manager) return 0;
    
    // Implementation would calculate actual cache size
    // This is a placeholder for the actual implementation
    return 0;
}

bool BackgroundImageManager_IsImageCached(BackgroundImageManager* manager, const char* file_path) {
    if (!manager || !file_path) return false;
    
    // Implementation would check if image is cached
    // This is a placeholder for the actual implementation
    return false;
}

// Utility Functions
const char* BackgroundImage_ScaleModeToString(BackgroundScaleMode mode) {
    switch (mode) {
        case BACKGROUND_SCALE_NONE: return "none";
        case BACKGROUND_SCALE_STRETCH: return "stretch";
        case BACKGROUND_SCALE_FIT: return "fit";
        case BACKGROUND_SCALE_COVER: return "cover";
        case BACKGROUND_SCALE_TILE: return "tile";
        case BACKGROUND_SCALE_CENTER: return "center";
        default: return "unknown";
    }
}

BackgroundScaleMode BackgroundImage_StringToScaleMode(const char* mode_string) {
    if (!mode_string) return BACKGROUND_SCALE_STRETCH;
    
    if (strcmp(mode_string, "none") == 0) return BACKGROUND_SCALE_NONE;
    if (strcmp(mode_string, "stretch") == 0) return BACKGROUND_SCALE_STRETCH;
    if (strcmp(mode_string, "fit") == 0) return BACKGROUND_SCALE_FIT;
    if (strcmp(mode_string, "cover") == 0) return BACKGROUND_SCALE_COVER;
    if (strcmp(mode_string, "tile") == 0) return BACKGROUND_SCALE_TILE;
    if (strcmp(mode_string, "center") == 0) return BACKGROUND_SCALE_CENTER;
    
    return BACKGROUND_SCALE_STRETCH;
}

const char* BackgroundImage_PositionToString(BackgroundPosition position) {
    switch (position) {
        case BACKGROUND_POSITION_TOP_LEFT: return "top-left";
        case BACKGROUND_POSITION_TOP_CENTER: return "top-center";
        case BACKGROUND_POSITION_TOP_RIGHT: return "top-right";
        case BACKGROUND_POSITION_CENTER_LEFT: return "center-left";
        case BACKGROUND_POSITION_CENTER: return "center";
        case BACKGROUND_POSITION_CENTER_RIGHT: return "center-right";
        case BACKGROUND_POSITION_BOTTOM_LEFT: return "bottom-left";
        case BACKGROUND_POSITION_BOTTOM_CENTER: return "bottom-center";
        case BACKGROUND_POSITION_BOTTOM_RIGHT: return "bottom-right";
        default: return "unknown";
    }
}

BackgroundPosition BackgroundImage_StringToPosition(const char* position_string) {
    if (!position_string) return BACKGROUND_POSITION_CENTER;
    
    if (strcmp(position_string, "top-left") == 0) return BACKGROUND_POSITION_TOP_LEFT;
    if (strcmp(position_string, "top-center") == 0) return BACKGROUND_POSITION_TOP_CENTER;
    if (strcmp(position_string, "top-right") == 0) return BACKGROUND_POSITION_TOP_RIGHT;
    if (strcmp(position_string, "center-left") == 0) return BACKGROUND_POSITION_CENTER_LEFT;
    if (strcmp(position_string, "center") == 0) return BACKGROUND_POSITION_CENTER;
    if (strcmp(position_string, "center-right") == 0) return BACKGROUND_POSITION_CENTER_RIGHT;
    if (strcmp(position_string, "bottom-left") == 0) return BACKGROUND_POSITION_BOTTOM_LEFT;
    if (strcmp(position_string, "bottom-center") == 0) return BACKGROUND_POSITION_BOTTOM_CENTER;
    if (strcmp(position_string, "bottom-right") == 0) return BACKGROUND_POSITION_BOTTOM_RIGHT;
    
    return BACKGROUND_POSITION_CENTER;
}

const char* BackgroundImage_RepeatModeToString(BackgroundRepeatMode mode) {
    switch (mode) {
        case BACKGROUND_REPEAT_NONE: return "none";
        case BACKGROUND_REPEAT_X: return "x";
        case BACKGROUND_REPEAT_Y: return "y";
        case BACKGROUND_REPEAT_BOTH: return "both";
        default: return "unknown";
    }
}

BackgroundRepeatMode BackgroundImage_StringToRepeatMode(const char* mode_string) {
    if (!mode_string) return BACKGROUND_REPEAT_NONE;
    
    if (strcmp(mode_string, "none") == 0) return BACKGROUND_REPEAT_NONE;
    if (strcmp(mode_string, "x") == 0) return BACKGROUND_REPEAT_X;
    if (strcmp(mode_string, "y") == 0) return BACKGROUND_REPEAT_Y;
    if (strcmp(mode_string, "both") == 0) return BACKGROUND_REPEAT_BOTH;
    
    return BACKGROUND_REPEAT_NONE;
}

const char* BackgroundImage_BlendModeToString(BackgroundBlendMode mode) {
    switch (mode) {
        case BACKGROUND_BLEND_NORMAL: return "normal";
        case BACKGROUND_BLEND_MULTIPLY: return "multiply";
        case BACKGROUND_BLEND_SCREEN: return "screen";
        case BACKGROUND_BLEND_OVERLAY: return "overlay";
        case BACKGROUND_BLEND_DARKEN: return "darken";
        case BACKGROUND_BLEND_LIGHTEN: return "lighten";
        case BACKGROUND_BLEND_COLOR_DODGE: return "color-dodge";
        case BACKGROUND_BLEND_COLOR_BURN: return "color-burn";
        case BACKGROUND_BLEND_HARD_LIGHT: return "hard-light";
        case BACKGROUND_BLEND_SOFT_LIGHT: return "soft-light";
        case BACKGROUND_BLEND_DIFFERENCE: return "difference";
        case BACKGROUND_BLEND_EXCLUSION: return "exclusion";
        default: return "unknown";
    }
}

BackgroundBlendMode BackgroundImage_StringToBlendMode(const char* mode_string) {
    if (!mode_string) return BACKGROUND_BLEND_NORMAL;
    
    if (strcmp(mode_string, "normal") == 0) return BACKGROUND_BLEND_NORMAL;
    if (strcmp(mode_string, "multiply") == 0) return BACKGROUND_BLEND_MULTIPLY;
    if (strcmp(mode_string, "screen") == 0) return BACKGROUND_BLEND_SCREEN;
    if (strcmp(mode_string, "overlay") == 0) return BACKGROUND_BLEND_OVERLAY;
    if (strcmp(mode_string, "darken") == 0) return BACKGROUND_BLEND_DARKEN;
    if (strcmp(mode_string, "lighten") == 0) return BACKGROUND_BLEND_LIGHTEN;
    if (strcmp(mode_string, "color-dodge") == 0) return BACKGROUND_BLEND_COLOR_DODGE;
    if (strcmp(mode_string, "color-burn") == 0) return BACKGROUND_BLEND_COLOR_BURN;
    if (strcmp(mode_string, "hard-light") == 0) return BACKGROUND_BLEND_HARD_LIGHT;
    if (strcmp(mode_string, "soft-light") == 0) return BACKGROUND_BLEND_SOFT_LIGHT;
    if (strcmp(mode_string, "difference") == 0) return BACKGROUND_BLEND_DIFFERENCE;
    if (strcmp(mode_string, "exclusion") == 0) return BACKGROUND_BLEND_EXCLUSION;
    
    return BACKGROUND_BLEND_NORMAL;
}

// Preset Configurations
void BackgroundImage_SetPresetGradient(BackgroundImage* bg, Color start_color, Color end_color, bool horizontal) {
    if (!bg) return;
    
    // Create a gradient image
    int width = horizontal ? 256 : 1;
    int height = horizontal ? 1 : 256;
    
    Image gradient = GenImageGradientV(width, height, start_color, end_color);
    if (horizontal) {
        Image rotated = ImageRotate(gradient, 90);
        UnloadImage(gradient);
        gradient = rotated;
    }
    
    BackgroundImage_LoadFromImage(bg, gradient);
    BackgroundImage_SetScaleMode(bg, BACKGROUND_SCALE_STRETCH);
    BackgroundImage_SetRepeatMode(bg, BACKGROUND_REPEAT_NONE);
}

void BackgroundImage_SetPresetPattern(BackgroundImage* bg, const char* pattern_type, Color color1, Color color2) {
    if (!bg || !pattern_type) return;
    
    // Create pattern image based on type
    Image pattern;
    
    if (strcmp(pattern_type, "checkerboard") == 0) {
        pattern = GenImageChecked(64, 64, 8, 8, color1, color2);
    } else if (strcmp(pattern_type, "stripes") == 0) {
        pattern = GenImageGradientH(64, 64, color1, color2);
    } else {
        // Default to solid color
        pattern = GenImageColor(64, 64, color1);
    }
    
    BackgroundImage_LoadFromImage(bg, pattern);
    BackgroundImage_SetScaleMode(bg, BACKGROUND_SCALE_TILE);
    BackgroundImage_SetRepeatMode(bg, BACKGROUND_REPEAT_BOTH);
}

void BackgroundImage_SetPresetNoise(BackgroundImage* bg, float scale, Color color) {
    if (!bg) return;
    
    // Create noise image
    Image noise = GenImagePerlinNoise(256, 256, 0, 0, scale);
    
    // Apply color tint
    ImageColorTint(&noise, color);
    
    BackgroundImage_LoadFromImage(bg, noise);
    BackgroundImage_SetScaleMode(bg, BACKGROUND_SCALE_STRETCH);
    BackgroundImage_SetRepeatMode(bg, BACKGROUND_REPEAT_NONE);
}
