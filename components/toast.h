#ifndef TOAST_H
#define TOAST_H

#include "raylib.h"
#include "component_base.h"
#include <stdbool.h>

// Toast types
typedef enum {
    TOAST_TYPE_INFO,
    TOAST_TYPE_SUCCESS,
    TOAST_TYPE_WARNING,
    TOAST_TYPE_ERROR
} ToastType;

// Toast positions
typedef enum {
    TOAST_POSITION_TOP_LEFT,
    TOAST_POSITION_TOP_CENTER,
    TOAST_POSITION_TOP_RIGHT,
    TOAST_POSITION_BOTTOM_LEFT,
    TOAST_POSITION_BOTTOM_CENTER,
    TOAST_POSITION_BOTTOM_RIGHT,
    TOAST_POSITION_CENTER
} ToastPosition;

typedef struct {
    // Base component
    ComponentBase base;
    
    // Toast properties
    char title[256];
    char message[512];
    ToastType type;
    ToastPosition position;
    
    // Visual properties
    Color background_color;
    Color border_color;
    Color text_color;
    Color title_color;
    Color icon_color;
    
    // Dimensions
    float width;
    float max_width;
    float min_width;
    float padding;
    float border_width;
    float corner_radius;
    float shadow_blur;
    
    // Animation
    float animation_progress;
    float animation_speed;
    bool is_entering;
    bool is_exiting;
    bool is_visible;
    
    // Timing
    float duration;
    float time_elapsed;
    bool auto_dismiss;
    bool pause_on_hover;
    
    // Interaction
    bool is_hovered;
    bool is_pressed;
    bool show_close_button;
    bool show_progress_bar;
    
    // Progress bar
    float progress;
    Color progress_color;
    
    // Callbacks
    void (*on_show)(struct Toast* toast);
    void (*on_hide)(struct Toast* toast);
    void (*on_click)(struct Toast* toast);
    void (*on_close)(struct Toast* toast);
} Toast;

// Toast manager for multiple toasts
typedef struct {
    Toast** toasts;
    int toast_count;
    int toast_capacity;
    ToastPosition default_position;
    float default_duration;
    float spacing;
    int max_toasts;
} ToastManager;

// Core Functions
Toast Toast_Create(const char* title, const char* message, ToastType type);
bool Toast_Init(Toast* toast, const char* title, const char* message, ToastType type);
void Toast_Update(Toast* toast);
void Toast_Draw(const Toast* toast);
void Toast_Destroy(Toast* toast);

// Toast Manager Functions
ToastManager* ToastManager_Create(void);
bool ToastManager_Init(ToastManager* manager);
void ToastManager_Update(ToastManager* manager);
void ToastManager_Draw(const ToastManager* manager);
void ToastManager_Destroy(ToastManager* manager);

// Toast Management
bool ToastManager_AddToast(ToastManager* manager, Toast* toast);
bool ToastManager_RemoveToast(ToastManager* manager, Toast* toast);
bool ToastManager_RemoveToastByIndex(ToastManager* manager, int index);
void ToastManager_ClearToasts(ToastManager* manager);
int ToastManager_GetToastCount(const ToastManager* manager);

// Quick Toast Functions
Toast* ToastManager_ShowInfo(ToastManager* manager, const char* title, const char* message);
Toast* ToastManager_ShowSuccess(ToastManager* manager, const char* title, const char* message);
Toast* ToastManager_ShowWarning(ToastManager* manager, const char* title, const char* message);
Toast* ToastManager_ShowError(ToastManager* manager, const char* title, const char* message);

// Toast Properties
void Toast_SetTitle(Toast* toast, const char* title);
void Toast_SetMessage(Toast* toast, const char* message);
void Toast_SetType(Toast* toast, ToastType type);
void Toast_SetPosition(Toast* toast, ToastPosition position);
const char* Toast_GetTitle(const Toast* toast);
const char* Toast_GetMessage(const Toast* toast);
ToastType Toast_GetType(const Toast* toast);
ToastPosition Toast_GetPosition(const Toast* toast);

// Visual Customization
void Toast_SetColors(Toast* toast, Color background, Color border, Color text);
void Toast_SetBackgroundColor(Toast* toast, Color color);
void Toast_SetBorderColor(Toast* toast, Color color);
void Toast_SetTextColor(Toast* toast, Color color);
void Toast_SetTitleColor(Toast* toast, Color color);
void Toast_SetIconColor(Toast* toast, Color color);

// Dimensions
void Toast_SetWidth(Toast* toast, float width);
void Toast_SetMaxWidth(Toast* toast, float width);
void Toast_SetMinWidth(Toast* toast, float width);
void Toast_SetPadding(Toast* toast, float padding);
void Toast_SetBorderWidth(Toast* toast, float width);
void Toast_SetCornerRadius(Toast* toast, float radius);
void Toast_SetShadowBlur(Toast* toast, float blur);

// Animation
void Toast_SetAnimationSpeed(Toast* toast, float speed);
void Toast_Show(Toast* toast);
void Toast_Hide(Toast* toast);
void Toast_Dismiss(Toast* toast);
bool Toast_IsVisible(const Toast* toast);
bool Toast_IsAnimating(const Toast* toast);

// Timing
void Toast_SetDuration(Toast* toast, float duration);
void Toast_SetAutoDismiss(Toast* toast, bool auto_dismiss);
void Toast_SetPauseOnHover(Toast* toast, bool pause);
float Toast_GetDuration(const Toast* toast);
bool Toast_IsAutoDismiss(const Toast* toast);

// Interaction
void Toast_SetShowCloseButton(Toast* toast, bool show);
void Toast_SetShowProgressBar(Toast* toast, bool show);
bool Toast_IsHovered(const Toast* toast);
bool Toast_IsPressed(const Toast* toast);
bool Toast_ShowCloseButton(const Toast* toast);
bool Toast_ShowProgressBar(const Toast* toast);

// Progress Bar
void Toast_SetProgress(Toast* toast, float progress);
void Toast_SetProgressColor(Toast* toast, Color color);
float Toast_GetProgress(const Toast* toast);

// Callbacks
void Toast_SetOnShowCallback(Toast* toast, void (*callback)(Toast* toast));
void Toast_SetOnHideCallback(Toast* toast, void (*callback)(Toast* toast));
void Toast_SetOnClickCallback(Toast* toast, void (*callback)(Toast* toast));
void Toast_SetOnCloseCallback(Toast* toast, void (*callback)(Toast* toast));

// Toast Manager Configuration
void ToastManager_SetDefaultPosition(ToastManager* manager, ToastPosition position);
void ToastManager_SetDefaultDuration(ToastManager* manager, float duration);
void ToastManager_SetSpacing(ToastManager* manager, float spacing);
void ToastManager_SetMaxToasts(ToastManager* manager, int max_toasts);
ToastPosition ToastManager_GetDefaultPosition(const ToastManager* manager);
float ToastManager_GetDefaultDuration(const ToastManager* manager);
float ToastManager_GetSpacing(const ToastManager* manager);
int ToastManager_GetMaxToasts(const ToastManager* manager);

// Input Handling
bool Toast_HandleMouseClick(Toast* toast, Vector2 position);
bool Toast_HandleMouseHover(Toast* toast, Vector2 position);
bool ToastManager_HandleMouseClick(ToastManager* manager, Vector2 position);
bool ToastManager_HandleMouseHover(ToastManager* manager, Vector2 position);

// Utility Functions
const char* Toast_TypeToString(ToastType type);
ToastType Toast_StringToType(const char* type_string);
const char* Toast_PositionToString(ToastPosition position);
ToastPosition Toast_StringToPosition(const char* position_string);
Color Toast_GetTypeColor(ToastType type);

#endif // TOAST_H
