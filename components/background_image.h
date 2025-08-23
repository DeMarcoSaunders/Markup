#ifndef BACKGROUND_IMAGE_H
#define BACKGROUND_IMAGE_H

#include "raylib.h"
#include <stdbool.h>

// Background image scaling modes
typedef enum {
    BACKGROUND_SCALE_NONE,        // No scaling, use original size
    BACKGROUND_SCALE_STRETCH,     // Stretch to fill container
    BACKGROUND_SCALE_FIT,         // Scale to fit within container (maintain aspect ratio)
    BACKGROUND_SCALE_COVER,       // Scale to cover container (maintain aspect ratio, crop if needed)
    BACKGROUND_SCALE_TILE,        // Tile the image to fill container
    BACKGROUND_SCALE_CENTER       // Center the image without scaling
} BackgroundScaleMode;

// Background image positioning
typedef enum {
    BACKGROUND_POSITION_TOP_LEFT,
    BACKGROUND_POSITION_TOP_CENTER,
    BACKGROUND_POSITION_TOP_RIGHT,
    BACKGROUND_POSITION_CENTER_LEFT,
    BACKGROUND_POSITION_CENTER,
    BACKGROUND_POSITION_CENTER_RIGHT,
    BACKGROUND_POSITION_BOTTOM_LEFT,
    BACKGROUND_POSITION_BOTTOM_CENTER,
    BACKGROUND_POSITION_BOTTOM_RIGHT
} BackgroundPosition;

// Background image repeat modes
typedef enum {
    BACKGROUND_REPEAT_NONE,
    BACKGROUND_REPEAT_X,
    BACKGROUND_REPEAT_Y,
    BACKGROUND_REPEAT_BOTH
} BackgroundRepeatMode;

// Background image blend modes
typedef enum {
    BACKGROUND_BLEND_NORMAL,
    BACKGROUND_BLEND_MULTIPLY,
    BACKGROUND_BLEND_SCREEN,
    BACKGROUND_BLEND_OVERLAY,
    BACKGROUND_BLEND_DARKEN,
    BACKGROUND_BLEND_LIGHTEN,
    BACKGROUND_BLEND_COLOR_DODGE,
    BACKGROUND_BLEND_COLOR_BURN,
    BACKGROUND_BLEND_HARD_LIGHT,
    BACKGROUND_BLEND_SOFT_LIGHT,
    BACKGROUND_BLEND_DIFFERENCE,
    BACKGROUND_BLEND_EXCLUSION
} BackgroundBlendMode;

typedef struct {
    // Image data
    Texture2D texture;
    Image image;
    char file_path[512];
    
    // Loading state
    bool is_loaded;
    bool is_valid;
    char error_message[256];
    
    // Original dimensions
    int original_width;
    int original_height;
    
    // Current display dimensions
    int display_width;
    int display_height;
    
    // Scaling and positioning
    BackgroundScaleMode scale_mode;
    BackgroundPosition position;
    BackgroundRepeatMode repeat_mode;
    BackgroundBlendMode blend_mode;
    
    // Offset and tiling
    Vector2 offset;
    Vector2 tile_size;
    Vector2 tile_offset;
    
    // Color and opacity
    Color tint_color;
    float opacity;
    
    // Animation properties
    bool is_animated;
    float animation_speed;
    float current_frame;
    int frame_count;
    int current_frame_index;
    
    // Caching
    RenderTexture2D cached_texture;
    bool use_cache;
    bool cache_dirty;
    
    // Memory management
    bool should_free_texture;
} BackgroundImage;

// Background image manager for multiple images
typedef struct {
    BackgroundImage** images;
    int image_count;
    int image_capacity;
    char cache_directory[512];
    bool enable_caching;
    size_t max_cache_size;
} BackgroundImageManager;

// Core Functions
BackgroundImage BackgroundImage_Create(void);
bool BackgroundImage_Init(BackgroundImage* bg);
void BackgroundImage_Destroy(BackgroundImage* bg);

// Loading and Management
bool BackgroundImage_LoadFromFile(BackgroundImage* bg, const char* file_path);
bool BackgroundImage_LoadFromMemory(BackgroundImage* bg, const unsigned char* data, int data_size);
bool BackgroundImage_LoadFromImage(BackgroundImage* bg, Image image);
bool BackgroundImage_LoadFromTexture(BackgroundImage* bg, Texture2D texture);
void BackgroundImage_Unload(BackgroundImage* bg);

// Image Properties
bool BackgroundImage_IsLoaded(const BackgroundImage* bg);
bool BackgroundImage_IsValid(const BackgroundImage* bg);
const char* BackgroundImage_GetError(const BackgroundImage* bg);
const char* BackgroundImage_GetFilePath(const BackgroundImage* bg);
int BackgroundImage_GetWidth(const BackgroundImage* bg);
int BackgroundImage_GetHeight(const BackgroundImage* bg);
Vector2 BackgroundImage_GetSize(const BackgroundImage* bg);

// Scaling and Positioning
void BackgroundImage_SetScaleMode(BackgroundImage* bg, BackgroundScaleMode mode);
void BackgroundImage_SetPosition(BackgroundImage* bg, BackgroundPosition position);
void BackgroundImage_SetRepeatMode(BackgroundImage* bg, BackgroundRepeatMode mode);
void BackgroundImage_SetBlendMode(BackgroundImage* bg, BackgroundBlendMode mode);
BackgroundScaleMode BackgroundImage_GetScaleMode(const BackgroundImage* bg);
BackgroundPosition BackgroundImage_GetPosition(const BackgroundImage* bg);
BackgroundRepeatMode BackgroundImage_GetRepeatMode(const BackgroundImage* bg);
BackgroundBlendMode BackgroundImage_GetBlendMode(const BackgroundImage* bg);

// Offset and Tiling
void BackgroundImage_SetOffset(BackgroundImage* bg, Vector2 offset);
void BackgroundImage_SetTileSize(BackgroundImage* bg, Vector2 tile_size);
void BackgroundImage_SetTileOffset(BackgroundImage* bg, Vector2 tile_offset);
Vector2 BackgroundImage_GetOffset(const BackgroundImage* bg);
Vector2 BackgroundImage_GetTileSize(const BackgroundImage* bg);
Vector2 BackgroundImage_GetTileOffset(const BackgroundImage* bg);

// Color and Opacity
void BackgroundImage_SetTintColor(BackgroundImage* bg, Color color);
void BackgroundImage_SetOpacity(BackgroundImage* bg, float opacity);
Color BackgroundImage_GetTintColor(const BackgroundImage* bg);
float BackgroundImage_GetOpacity(const BackgroundImage* bg);

// Animation
void BackgroundImage_SetAnimated(BackgroundImage* bg, bool animated);
void BackgroundImage_SetAnimationSpeed(BackgroundImage* bg, float speed);
void BackgroundImage_SetFrameCount(BackgroundImage* bg, int frame_count);
bool BackgroundImage_IsAnimated(const BackgroundImage* bg);
float BackgroundImage_GetAnimationSpeed(const BackgroundImage* bg);
int BackgroundImage_GetFrameCount(const BackgroundImage* bg);
int BackgroundImage_GetCurrentFrame(const BackgroundImage* bg);

// Caching
void BackgroundImage_SetUseCache(BackgroundImage* bg, bool use_cache);
void BackgroundImage_SetCacheDirty(BackgroundImage* bg, bool dirty);
bool BackgroundImage_UseCache(const BackgroundImage* bg);
bool BackgroundImage_IsCacheDirty(const BackgroundImage* bg);

// Rendering
void BackgroundImage_Update(BackgroundImage* bg);
void BackgroundImage_Draw(const BackgroundImage* bg, Rectangle dest_rect);
void BackgroundImage_DrawScaled(const BackgroundImage* bg, Rectangle dest_rect, Vector2 scale);
void BackgroundImage_DrawTiled(const BackgroundImage* bg, Rectangle dest_rect);

// Utility Functions
Rectangle BackgroundImage_CalculateDestRect(const BackgroundImage* bg, Rectangle container_rect);
Vector2 BackgroundImage_CalculateScale(const BackgroundImage* bg, Rectangle container_rect);
Vector2 BackgroundImage_CalculatePosition(const BackgroundImage* bg, Rectangle container_rect, Vector2 scale);

// Background Image Manager
BackgroundImageManager* BackgroundImageManager_Create(void);
bool BackgroundImageManager_Init(BackgroundImageManager* manager);
void BackgroundImageManager_Destroy(BackgroundImageManager* manager);

// Manager Functions
bool BackgroundImageManager_AddImage(BackgroundImageManager* manager, BackgroundImage* bg);
bool BackgroundImageManager_RemoveImage(BackgroundImageManager* manager, BackgroundImage* bg);
bool BackgroundImageManager_RemoveImageByIndex(BackgroundImageManager* manager, int index);
void BackgroundImageManager_ClearImages(BackgroundImageManager* manager);
int BackgroundImageManager_GetImageCount(const BackgroundImageManager* manager);

// Manager Configuration
void BackgroundImageManager_SetCacheDirectory(BackgroundImageManager* manager, const char* directory);
void BackgroundImageManager_SetEnableCaching(BackgroundImageManager* manager, bool enable);
void BackgroundImageManager_SetMaxCacheSize(BackgroundImageManager* manager, size_t max_size);
const char* BackgroundImageManager_GetCacheDirectory(const BackgroundImageManager* manager);
bool BackgroundImageManager_IsCachingEnabled(const BackgroundImageManager* manager);
size_t BackgroundImageManager_GetMaxCacheSize(const BackgroundImageManager* manager);

// Cache Management
void BackgroundImageManager_ClearCache(BackgroundImageManager* manager);
size_t BackgroundImageManager_GetCacheSize(const BackgroundImageManager* manager);
bool BackgroundImageManager_IsImageCached(BackgroundImageManager* manager, const char* file_path);

// Utility Functions
const char* BackgroundImage_ScaleModeToString(BackgroundScaleMode mode);
BackgroundScaleMode BackgroundImage_StringToScaleMode(const char* mode_string);
const char* BackgroundImage_PositionToString(BackgroundPosition position);
BackgroundPosition BackgroundImage_StringToPosition(const char* position_string);
const char* BackgroundImage_RepeatModeToString(BackgroundRepeatMode mode);
BackgroundRepeatMode BackgroundImage_StringToRepeatMode(const char* mode_string);
const char* BackgroundImage_BlendModeToString(BackgroundBlendMode mode);
BackgroundBlendMode BackgroundImage_StringToBlendMode(const char* mode_string);

// Preset Configurations
void BackgroundImage_SetPresetGradient(BackgroundImage* bg, Color start_color, Color end_color, bool horizontal);
void BackgroundImage_SetPresetPattern(BackgroundImage* bg, const char* pattern_type, Color color1, Color color2);
void BackgroundImage_SetPresetNoise(BackgroundImage* bg, float scale, Color color);

#endif // BACKGROUND_IMAGE_H
