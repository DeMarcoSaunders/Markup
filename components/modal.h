#ifndef MODAL_H
#define MODAL_H

#include "raylib.h"
#include <stdbool.h>

typedef enum {
    MODAL_SIZE_AUTO,
    MODAL_SIZE_SMALL,
    MODAL_SIZE_MEDIUM,
    MODAL_SIZE_LARGE,
    MODAL_SIZE_CUSTOM
} ModalSize;

typedef struct {
    Rectangle bounds;
    Rectangle content_bounds;
    char* title;
    bool is_visible;
    bool is_closing;
    bool close_on_overlay_click;
    bool show_close_button;
    ModalSize size_preset;
    
    // Animation state
    float animation_progress;
    float target_progress;
    
    // Interaction state
    bool is_dragging;
    Vector2 drag_offset;
    bool close_button_hovered;
    bool close_button_pressed;
    
    // Content callback function pointer (optional)
    void (*draw_content)(Rectangle content_area, void* user_data);
    void* user_data;
    
    // Blur effect settings
    bool use_blur_effect;
} Modal;

// Function declarations
Modal Modal_Create(const char* title, ModalSize size);
Modal Modal_CreateCustom(const char* title, float width, float height);
void Modal_Update(Modal* modal);
void Modal_Draw(Modal* modal);
void Modal_DrawWithBackground(Modal* modal, void (*draw_background)(void));
void Modal_Show(Modal* modal);
void Modal_Hide(Modal* modal);
void Modal_Close(Modal* modal);
bool Modal_IsVisible(Modal* modal);
void Modal_SetTitle(Modal* modal, const char* title);
void Modal_SetSize(Modal* modal, ModalSize size);
void Modal_SetCustomSize(Modal* modal, float width, float height);
void Modal_SetCloseOnOverlayClick(Modal* modal, bool enabled);
void Modal_SetShowCloseButton(Modal* modal, bool show);
void Modal_SetContentCallback(Modal* modal, void (*callback)(Rectangle, void*), void* user_data);
Rectangle Modal_GetContentArea(Modal* modal);
void Modal_SetBlurEffect(Modal* modal, bool enabled);
void Modal_Destroy(Modal* modal);

#endif // MODAL_H