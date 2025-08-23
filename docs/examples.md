# Complete Examples

This document provides complete, working examples that demonstrate various aspects of the UI library.

## Example 1: Simple Application

A basic application with buttons and a modal dialog.

```c
#include "raylib.h"
#include "ui.h"
#include <stdio.h>

int main(void) {
    // Initialize window
    InitWindow(800, 600, "Simple UI Example");
    SetTargetFPS(60);
    
    // Initialize UI systems
    Theme_Init_Default();
    BlurEffect_Init();
    
    // Create components
    Button primary_btn = Button_Create((Rectangle){100, 100, 150, 50}, "Primary", BUTTON_PRIMARY);
    Button secondary_btn = Button_Create((Rectangle){270, 100, 150, 50}, "Secondary", BUTTON_SECONDARY);
    Button danger_btn = Button_Create((Rectangle){440, 100, 150, 50}, "Delete", BUTTON_DESTRUCTIVE);
    
    Modal info_modal = Modal_Create("Information", MODAL_SIZE_MEDIUM);
    
    // Enable visual effects
    Button_SetShadow(&primary_btn, true);
    Button_SetShadow(&secondary_btn, true);
    Button_SetShadow(&danger_btn, true);
    
    // Set up modal content
    void draw_modal_content(Rectangle area, void* data) {
        DrawText("This is a modal dialog with blur backdrop!", 
                 area.x + 20, area.y + 20, 20, WHITE);
        DrawText("Click outside or press ESC to close.", 
                 area.x + 20, area.y + 60, 16, LIGHTGRAY);
        
        // Add some interactive content
        static Slider demo_slider = {0};
        if (demo_slider.bounds.width == 0) {
            demo_slider = Slider_Create((Rectangle){area.x + 20, area.y + 100, 200, 30}, 
                                       0, 100, 50);
        }
        
        Slider_Update(&demo_slider);
        Slider_Draw(&demo_slider);
        
        char value_text[32];
        sprintf(value_text, "Value: %.1f", Slider_GetValue(&demo_slider));
        DrawText(value_text, area.x + 20, area.y + 140, 16, WHITE);
    }
    
    Modal_SetContentCallback(&info_modal, draw_modal_content, NULL);
    
    // Main loop
    while (!WindowShouldClose()) {
        // Update components
        Button_Update(&primary_btn);
        Button_Update(&secondary_btn);
        Button_Update(&danger_btn);
        Modal_Update(&info_modal);
        
        // Handle button clicks
        if (primary_btn.is_clicked) {
            printf("Primary button clicked!\n");
            Modal_Show(&info_modal);
        }
        
        if (secondary_btn.is_clicked) {
            printf("Secondary button clicked!\n");
        }
        
        if (danger_btn.is_clicked) {
            printf("Danger button clicked!\n");
        }
        
        // Drawing
        BeginDrawing();
        ClearBackground((Color){20, 25, 35, 255});
        
        // Draw title
        DrawText("Raylib UI Library Demo", 50, 30, 28, WHITE);
        DrawText("Click buttons to interact", 50, 65, 16, LIGHTGRAY);
        
        // Draw main UI
        void draw_main_ui() {
            ClearBackground((Color){20, 25, 35, 255});
            DrawText("Raylib UI Library Demo", 50, 30, 28, WHITE);
            DrawText("Click buttons to interact", 50, 65, 16, LIGHTGRAY);
            Button_Draw(&primary_btn);
            Button_Draw(&secondary_btn);
            Button_Draw(&danger_btn);
        }
        
        // Draw components
        if (Modal_IsVisible(&info_modal)) {
            Modal_DrawWithBackground(&info_modal, draw_main_ui);
        } else {
            draw_main_ui();
        }
        
        EndDrawing();
    }
    
    // Cleanup
    Button_Destroy(&primary_btn);
    Button_Destroy(&secondary_btn);
    Button_Destroy(&danger_btn);
    Modal_Destroy(&info_modal);
    BlurEffect_Destroy();
    CloseWindow();
    
    return 0;
}
```

## Example 2: Settings Panel with Layout

A settings interface using panels and flexbox layout.

```c
#include "raylib.h"
#include "ui.h"
#include <stdio.h>

typedef struct {
    float master_volume;
    float music_volume;
    float sfx_volume;
    bool fullscreen;
    bool vsync;
} Settings;

int main(void) {
    InitWindow(900, 700, "Settings Panel Example");
    SetTargetFPS(60);
    
    Theme_Init_Default();
    BlurEffect_Init();
    
    Settings app_settings = {75.0f, 60.0f, 80.0f, false, true};
    
    // Create main layout
    Panel main_panel = Panel_Create((Rectangle){50, 50, 800, 600});
    Panel_SetFlexDirection(&main_panel, FLEX_COLUMN);
    Panel_SetJustifyContent(&main_panel, JUSTIFY_FLEX_START);
    Panel_SetAlignItems(&main_panel, ALIGN_STRETCH);
    Panel_SetGap(&main_panel, 30);
    
    // Audio settings section
    Panel audio_section = Panel_Create((Rectangle){0, 0, 800, 200});
    Panel_SetFlexDirection(&audio_section, FLEX_COLUMN);
    Panel_SetGap(&audio_section, 15);
    
    Slider master_slider = Slider_Create((Rectangle){0, 0, 300, 30}, 0, 100, app_settings.master_volume);
    Slider music_slider = Slider_Create((Rectangle){0, 0, 300, 30}, 0, 100, app_settings.music_volume);
    Slider sfx_slider = Slider_Create((Rectangle){0, 0, 300, 30}, 0, 100, app_settings.sfx_volume);
    
    Slider_SetShowValue(&master_slider, true);
    Slider_SetShowValue(&music_slider, true);
    Slider_SetShowValue(&sfx_slider, true);
    
    Panel_AddSlider(&audio_section, &master_slider);
    Panel_AddSlider(&audio_section, &music_slider);
    Panel_AddSlider(&audio_section, &sfx_slider);
    
    // Graphics settings section
    Panel graphics_section = Panel_Create((Rectangle){0, 0, 800, 150});
    Panel_SetFlexDirection(&graphics_section, FLEX_COLUMN);
    Panel_SetGap(&graphics_section, 15);
    
    Button fullscreen_btn = Button_Create((Rectangle){0, 0, 200, 40}, 
                                         app_settings.fullscreen ? "Windowed" : "Fullscreen", 
                                         BUTTON_SECONDARY);
    Button vsync_btn = Button_Create((Rectangle){0, 0, 200, 40}, 
                                    app_settings.vsync ? "VSync: ON" : "VSync: OFF", 
                                    BUTTON_SECONDARY);
    
    Panel_AddButton(&graphics_section, &fullscreen_btn);
    Panel_AddButton(&graphics_section, &vsync_btn);
    
    // Action buttons
    Panel action_section = Panel_Create((Rectangle){0, 0, 800, 60});
    Panel_SetFlexDirection(&action_section, FLEX_ROW);
    Panel_SetJustifyContent(&action_section, JUSTIFY_SPACE_BETWEEN);
    
    Button save_btn = Button_Create((Rectangle){0, 0, 120, 50}, "Save", BUTTON_PRIMARY);
    Button reset_btn = Button_Create((Rectangle){0, 0, 120, 50}, "Reset", BUTTON_DESTRUCTIVE);
    Button cancel_btn = Button_Create((Rectangle){0, 0, 120, 50}, "Cancel", BUTTON_SECONDARY);
    
    Panel_AddButton(&action_section, &save_btn);
    Panel_AddButton(&action_section, &reset_btn);
    Panel_AddButton(&action_section, &cancel_btn);
    
    // Add sections to main panel
    Panel_AddPanel(&main_panel, &audio_section);
    Panel_AddPanel(&main_panel, &graphics_section);
    Panel_AddPanel(&main_panel, &action_section);
    
    // Confirmation modal
    Modal confirm_modal = Modal_Create("Confirm Reset", MODAL_SIZE_SMALL);
    
    void draw_confirm_content(Rectangle area, void* data) {
        DrawText("Reset all settings to defaults?", area.x + 20, area.y + 20, 18, WHITE);
        DrawText("This action cannot be undone.", area.x + 20, area.y + 50, 14, LIGHTGRAY);
    }
    Modal_SetContentCallback(&confirm_modal, draw_confirm_content, NULL);
    
    while (!WindowShouldClose()) {
        // Update all components
        Panel_Update(&main_panel);
        Modal_Update(&confirm_modal);
        
        // Handle slider changes
        app_settings.master_volume = Slider_GetValue(&master_slider);
        app_settings.music_volume = Slider_GetValue(&music_slider);
        app_settings.sfx_volume = Slider_GetValue(&sfx_slider);
        
        // Handle button clicks
        if (fullscreen_btn.is_clicked) {
            app_settings.fullscreen = !app_settings.fullscreen;
            Button_Destroy(&fullscreen_btn);
            fullscreen_btn = Button_Create((Rectangle){0, 0, 200, 40}, 
                                          app_settings.fullscreen ? "Windowed" : "Fullscreen", 
                                          BUTTON_SECONDARY);
            // Re-add to panel (simplified for example)
        }
        
        if (vsync_btn.is_clicked) {
            app_settings.vsync = !app_settings.vsync;
            Button_Destroy(&vsync_btn);
            vsync_btn = Button_Create((Rectangle){0, 0, 200, 40}, 
                                     app_settings.vsync ? "VSync: ON" : "VSync: OFF", 
                                     BUTTON_SECONDARY);
        }
        
        if (save_btn.is_clicked) {
            printf("Settings saved!\n");
            // Save settings to file...
        }
        
        if (reset_btn.is_clicked) {
            Modal_Show(&confirm_modal);
        }
        
        if (cancel_btn.is_clicked) {
            printf("Settings cancelled\n");
            // Restore original settings...
        }
        
        // Drawing
        BeginDrawing();
        
        void draw_main_app() {
            ClearBackground((Color){25, 30, 40, 255});
            
            // Draw section headers
            DrawText("Audio Settings", 70, 80, 20, WHITE);
            DrawText("Graphics Settings", 70, 280, 20, WHITE);
            
            // Draw labels for sliders
            DrawText("Master Volume:", 70, 120, 16, LIGHTGRAY);
            DrawText("Music Volume:", 70, 165, 16, LIGHTGRAY);
            DrawText("SFX Volume:", 70, 210, 16, LIGHTGRAY);
            
            Panel_Draw(&main_panel);
        }
        
        if (Modal_IsVisible(&confirm_modal)) {
            Modal_DrawWithBackground(&confirm_modal, draw_main_app);
        } else {
            draw_main_app();
        }
        
        EndDrawing();
    }
    
    // Cleanup
    Panel_Destroy(&main_panel);
    Modal_Destroy(&confirm_modal);
    BlurEffect_Destroy();
    CloseWindow();
    
    return 0;
}
```

## Example 3: Dashboard with Sidebar

A dashboard application with navigation sidebar and multiple content areas.

```c
#include "raylib.h"
#include "ui.h"
#include <stdio.h>
#include <string.h>

typedef enum {
    PAGE_DASHBOARD,
    PAGE_ANALYTICS,
    PAGE_SETTINGS,
    PAGE_HELP
} CurrentPage;

int main(void) {
    InitWindow(1200, 800, "Dashboard Example");
    SetTargetFPS(60);
    
    Theme_Init_Default();
    BlurEffect_Init();
    
    CurrentPage current_page = PAGE_DASHBOARD;
    
    // Create sidebar
    Sidebar nav_sidebar = Sidebar_Create((Rectangle){0, 0, 250, 800}, SIDEBAR_LEFT);
    Sidebar_SetFlexDirection(&nav_sidebar, FLEX_COLUMN);
    Sidebar_SetJustifyContent(&nav_sidebar, JUSTIFY_FLEX_START);
    Sidebar_SetGap(&nav_sidebar, 10);
    
    // Navigation buttons
    Button dashboard_btn = Button_Create((Rectangle){0, 0, 220, 45}, "📊 Dashboard", BUTTON_SECONDARY);
    Button analytics_btn = Button_Create((Rectangle){0, 0, 220, 45}, "📈 Analytics", BUTTON_SECONDARY);
    Button settings_btn = Button_Create((Rectangle){0, 0, 220, 45}, "⚙️ Settings", BUTTON_SECONDARY);
    Button help_btn = Button_Create((Rectangle){0, 0, 220, 45}, "❓ Help", BUTTON_SECONDARY);
    
    Sidebar_AddButton(&nav_sidebar, &dashboard_btn);
    Sidebar_AddButton(&nav_sidebar, &analytics_btn);
    Sidebar_AddButton(&nav_sidebar, &settings_btn);
    Sidebar_AddButton(&nav_sidebar, &help_btn);
    
    // Main content area
    Panel content_panel = Panel_Create((Rectangle){270, 20, 910, 760});
    Panel_SetFlexDirection(&content_panel, FLEX_COLUMN);
    Panel_SetGap(&content_panel, 20);
    
    // Dashboard widgets
    Panel stats_row = Panel_Create((Rectangle){0, 0, 910, 120});
    Panel_SetFlexDirection(&stats_row, FLEX_ROW);
    Panel_SetJustifyContent(&stats_row, JUSTIFY_SPACE_BETWEEN);
    
    // Create stat cards (simplified as buttons for this example)
    Button users_card = Button_Create((Rectangle){0, 0, 200, 100}, "Users\n1,234", BUTTON_PRIMARY);
    Button revenue_card = Button_Create((Rectangle){0, 0, 200, 100}, "Revenue\n$12,345", BUTTON_PRIMARY);
    Button orders_card = Button_Create((Rectangle){0, 0, 200, 100}, "Orders\n567", BUTTON_PRIMARY);
    Button growth_card = Button_Create((Rectangle){0, 0, 200, 100}, "Growth\n+12%", BUTTON_PRIMARY);
    
    Panel_AddButton(&stats_row, &users_card);
    Panel_AddButton(&stats_row, &revenue_card);
    Panel_AddButton(&stats_row, &orders_card);
    Panel_AddButton(&stats_row, &growth_card);
    
    // Chart area (simplified)
    Panel chart_panel = Panel_Create((Rectangle){0, 0, 910, 400});
    
    // Control panel
    Panel controls_panel = Panel_Create((Rectangle){0, 0, 910, 80});
    Panel_SetFlexDirection(&controls_panel, FLEX_ROW);
    Panel_SetJustifyContent(&controls_panel, JUSTIFY_FLEX_START);
    Panel_SetGap(&controls_panel, 15);
    
    Button refresh_btn = Button_Create((Rectangle){0, 0, 100, 40}, "Refresh", BUTTON_SECONDARY);
    Button export_btn = Button_Create((Rectangle){0, 0, 100, 40}, "Export", BUTTON_SECONDARY);
    Slider time_range = Slider_Create((Rectangle){0, 0, 200, 30}, 1, 30, 7);
    
    Panel_AddButton(&controls_panel, &refresh_btn);
    Panel_AddButton(&controls_panel, &export_btn);
    Panel_AddSlider(&controls_panel, &time_range);
    
    Panel_AddPanel(&content_panel, &stats_row);
    Panel_AddPanel(&content_panel, &chart_panel);
    Panel_AddPanel(&content_panel, &controls_panel);
    
    // Notification modal
    Modal notification_modal = Modal_Create("Notification", MODAL_SIZE_SMALL);
    char notification_text[256] = "";
    
    void draw_notification_content(Rectangle area, void* data) {
        DrawText((char*)data, area.x + 20, area.y + 20, 16, WHITE);
    }
    Modal_SetContentCallback(&notification_modal, draw_notification_content, notification_text);
    
    while (!WindowShouldClose()) {
        // Update components
        Sidebar_Update(&nav_sidebar);
        Panel_Update(&content_panel);
        Modal_Update(&notification_modal);
        
        // Handle navigation
        if (dashboard_btn.is_clicked) {
            current_page = PAGE_DASHBOARD;
            strcpy(notification_text, "Switched to Dashboard");
            Modal_Show(&notification_modal);
        }
        if (analytics_btn.is_clicked) {
            current_page = PAGE_ANALYTICS;
            strcpy(notification_text, "Switched to Analytics");
            Modal_Show(&notification_modal);
        }
        if (settings_btn.is_clicked) {
            current_page = PAGE_SETTINGS;
            strcpy(notification_text, "Switched to Settings");
            Modal_Show(&notification_modal);
        }
        if (help_btn.is_clicked) {
            current_page = PAGE_HELP;
            strcpy(notification_text, "Switched to Help");
            Modal_Show(&notification_modal);
        }
        
        // Handle other interactions
        if (refresh_btn.is_clicked) {
            strcpy(notification_text, "Data refreshed!");
            Modal_Show(&notification_modal);
        }
        
        if (export_btn.is_clicked) {
            strcpy(notification_text, "Data exported successfully!");
            Modal_Show(&notification_modal);
        }
        
        // Toggle sidebar with key
        if (IsKeyPressed(KEY_TAB)) {
            Sidebar_Toggle(&nav_sidebar);
        }
        
        // Drawing
        BeginDrawing();
        
        void draw_main_dashboard() {
            ClearBackground((Color){15, 20, 30, 255});
            
            // Draw page title
            const char* page_titles[] = {"Dashboard", "Analytics", "Settings", "Help"};
            DrawText(page_titles[current_page], 290, 30, 32, WHITE);
            
            // Draw sidebar and content
            Sidebar_Draw(&nav_sidebar);
            
            // Only draw content panel for dashboard page
            if (current_page == PAGE_DASHBOARD) {
                Panel_Draw(&content_panel);
            } else {
                // Draw placeholder content for other pages
                DrawText("Content for this page is not implemented in this example.", 
                         300, 200, 20, LIGHTGRAY);
            }
            
            // Draw time range label
            char time_text[64];
            sprintf(time_text, "Time Range: %.0f days", Slider_GetValue(&time_range));
            DrawText(time_text, 600, 720, 14, LIGHTGRAY);
            
            // Draw instructions
            DrawText("Press TAB to toggle sidebar", 20, 750, 12, DARKGRAY);
        }
        
        if (Modal_IsVisible(&notification_modal)) {
            Modal_DrawWithBackground(&notification_modal, draw_main_dashboard);
        } else {
            draw_main_dashboard();
        }
        
        EndDrawing();
    }
    
    // Cleanup
    Sidebar_Destroy(&nav_sidebar);
    Panel_Destroy(&content_panel);
    Modal_Destroy(&notification_modal);
    BlurEffect_Destroy();
    CloseWindow();
    
    return 0;
}
```

## Example 4: Custom Theme Application

Demonstrates theme customization and runtime theme switching.

```c
#include "raylib.h"
#include "ui.h"
#include <stdio.h>

typedef enum {
    THEME_DARK,
    THEME_LIGHT,
    THEME_NEON
} AppTheme;

void SetAppTheme(AppTheme theme) {
    switch (theme) {
        case THEME_DARK:
            Theme_Init_Default();
            break;
            
        case THEME_LIGHT: {
            Theme_Init_Default();
            
            // Override with light colors
            ComponentStyle* button = &AppTheme.components[COMPONENT_BUTTON];
            button->colors[STATE_DEFAULT] = (ColorPalette){
                .background = WHITE,
                .border = LIGHTGRAY,
                .text = BLACK,
                .accent = BLUE,
                .shadow = (Color){0, 0, 0, 50}
            };
            
            ComponentStyle* panel = &AppTheme.components[COMPONENT_PANEL];
            panel->colors[STATE_DEFAULT] = (ColorPalette){
                .background = (Color){248, 249, 250, 255},
                .border = (Color){229, 231, 235, 255},
                .text = (Color){17, 24, 39, 255},
                .accent = BLUE,
                .shadow = (Color){0, 0, 0, 30}
            };
            break;
        }
        
        case THEME_NEON: {
            Theme_Init_Default();
            
            // Neon theme
            ComponentStyle* button = &AppTheme.components[COMPONENT_BUTTON];
            button->colors[STATE_DEFAULT] = (ColorPalette){
                .background = (Color){20, 20, 40, 255},
                .border = (Color){0, 255, 255, 255},
                .text = (Color){0, 255, 255, 255},
                .accent = (Color){255, 0, 255, 255},
                .shadow = (Color){0, 255, 255, 100}
            };
            
            button->colors[STATE_HOVER] = (ColorPalette){
                .background = (Color){40, 40, 80, 255},
                .border = (Color){0, 255, 255, 255},
                .text = (Color){255, 255, 255, 255},
                .accent = (Color){255, 0, 255, 255},
                .shadow = (Color){0, 255, 255, 150}
            };
            
            // Increase glow effects
            button->shadow_blur = 15.0f;
            button->shadow_spread = 3.0f;
            break;
        }
    }
}

int main(void) {
    InitWindow(800, 600, "Theme Customization Example");
    SetTargetFPS(60);
    
    BlurEffect_Init();
    
    AppTheme current_theme = THEME_DARK;
    SetAppTheme(current_theme);
    
    // Create UI components
    Panel main_panel = Panel_Create((Rectangle){50, 50, 700, 500});
    Panel_SetFlexDirection(&main_panel, FLEX_COLUMN);
    Panel_SetGap(&main_panel, 20);
    
    // Theme selector
    Panel theme_panel = Panel_Create((Rectangle){0, 0, 700, 60});
    Panel_SetFlexDirection(&theme_panel, FLEX_ROW);
    Panel_SetJustifyContent(&theme_panel, JUSTIFY_CENTER);
    Panel_SetGap(&theme_panel, 15);
    
    Button dark_theme_btn = Button_Create((Rectangle){0, 0, 120, 40}, "Dark", BUTTON_SECONDARY);
    Button light_theme_btn = Button_Create((Rectangle){0, 0, 120, 40}, "Light", BUTTON_SECONDARY);
    Button neon_theme_btn = Button_Create((Rectangle){0, 0, 120, 40}, "Neon", BUTTON_SECONDARY);
    
    Panel_AddButton(&theme_panel, &dark_theme_btn);
    Panel_AddButton(&theme_panel, &light_theme_btn);
    Panel_AddButton(&theme_panel, &neon_theme_btn);
    
    // Demo components
    Panel demo_panel = Panel_Create((Rectangle){0, 0, 700, 300});
    Panel_SetFlexDirection(&demo_panel, FLEX_COLUMN);
    Panel_SetGap(&demo_panel, 15);
    
    Button primary_demo = Button_Create((Rectangle){0, 0, 200, 50}, "Primary Button", BUTTON_PRIMARY);
    Button secondary_demo = Button_Create((Rectangle){0, 0, 200, 50}, "Secondary Button", BUTTON_SECONDARY);
    Button danger_demo = Button_Create((Rectangle){0, 0, 200, 50}, "Danger Button", BUTTON_DESTRUCTIVE);
    
    Slider demo_slider = Slider_Create((Rectangle){0, 0, 300, 30}, 0, 100, 50);
    Slider_SetShowValue(&demo_slider, true);
    
    // Enable shadows for better theme demonstration
    Button_SetShadow(&primary_demo, true);
    Button_SetShadow(&secondary_demo, true);
    Button_SetShadow(&danger_demo, true);
    
    Panel_AddButton(&demo_panel, &primary_demo);
    Panel_AddButton(&demo_panel, &secondary_demo);
    Panel_AddButton(&demo_panel, &danger_demo);
    Panel_AddSlider(&demo_panel, &demo_slider);
    
    Panel_AddPanel(&main_panel, &theme_panel);
    Panel_AddPanel(&main_panel, &demo_panel);
    
    // Theme info modal
    Modal theme_info = Modal_Create("Theme Information", MODAL_SIZE_MEDIUM);
    char theme_info_text[512];
    
    void draw_theme_info_content(Rectangle area, void* data) {
        DrawText((char*)data, area.x + 20, area.y + 20, 16, WHITE);
    }
    Modal_SetContentCallback(&theme_info, draw_theme_info_content, theme_info_text);
    
    while (!WindowShouldClose()) {
        // Update components
        Panel_Update(&main_panel);
        Modal_Update(&theme_info);
        
        // Handle theme switching
        if (dark_theme_btn.is_clicked && current_theme != THEME_DARK) {
            current_theme = THEME_DARK;
            SetAppTheme(current_theme);
            sprintf(theme_info_text, "Switched to Dark Theme\n\nFeatures:\n- Dark backgrounds\n- Blue accents\n- Subtle shadows\n- Easy on the eyes");
            Modal_Show(&theme_info);
        }
        
        if (light_theme_btn.is_clicked && current_theme != THEME_LIGHT) {
            current_theme = THEME_LIGHT;
            SetAppTheme(current_theme);
            sprintf(theme_info_text, "Switched to Light Theme\n\nFeatures:\n- Light backgrounds\n- Dark text\n- Clean appearance\n- High contrast");
            Modal_Show(&theme_info);
        }
        
        if (neon_theme_btn.is_clicked && current_theme != THEME_NEON) {
            current_theme = THEME_NEON;
            SetAppTheme(current_theme);
            sprintf(theme_info_text, "Switched to Neon Theme\n\nFeatures:\n- Cyberpunk aesthetic\n- Glowing borders\n- Neon colors\n- Enhanced shadows");
            Modal_Show(&theme_info);
        }
        
        // Drawing
        BeginDrawing();
        
        void draw_main_app() {
            Color bg_color;
            switch (current_theme) {
                case THEME_DARK: bg_color = (Color){15, 20, 30, 255}; break;
                case THEME_LIGHT: bg_color = (Color){240, 242, 247, 255}; break;
                case THEME_NEON: bg_color = (Color){10, 10, 20, 255}; break;
            }
            
            ClearBackground(bg_color);
            
            // Draw title
            Color title_color = (current_theme == THEME_LIGHT) ? BLACK : WHITE;
            DrawText("Theme Customization Demo", 50, 20, 24, title_color);
            
            Panel_Draw(&main_panel);
            
            // Draw current theme indicator
            const char* theme_names[] = {"Dark", "Light", "Neon"};
            char status_text[64];
            sprintf(status_text, "Current Theme: %s", theme_names[current_theme]);
            DrawText(status_text, 50, 570, 14, title_color);
        }
        
        if (Modal_IsVisible(&theme_info)) {
            Modal_DrawWithBackground(&theme_info, draw_main_app);
        } else {
            draw_main_app();
        }
        
        EndDrawing();
    }
    
    // Cleanup
    Panel_Destroy(&main_panel);
    Modal_Destroy(&theme_info);
    BlurEffect_Destroy();
    CloseWindow();
    
    return 0;
}
```

## Building and Running Examples

### Compilation

Make sure you have Raylib installed, then compile with:

```bash
gcc -o example1 example1.c ui_library_files*.c -lraylib -lGL -lm -lpthread -ldl -lrt -lX11
```

### Project Structure

```
your_project/
├── examples/
│   ├── example1.c
│   ├── example2.c
│   ├── example3.c
│   └── example4.c
├── ui_library/
│   ├── ui.h
│   ├── theme.h/c
│   ├── button.h/c
│   ├── panel.h/c
│   ├── modal.h/c
│   ├── slider.h/c
│   ├── sidebar.h/c
│   ├── shadow.h/c
│   ├── blur_effect.h/c
│   ├── backdrop.h/c
│   └── vendor/vec.h
└── shaders/
    ├── blur.fs
    └── blur_improved.fs
```

### Makefile Example

```makefile
CC = gcc
CFLAGS = -Wall -Wextra -std=c99
LIBS = -lraylib -lGL -lm -lpthread -ldl -lrt -lX11

UI_SOURCES = ui_library/theme.c ui_library/button.c ui_library/panel.c \
             ui_library/modal.c ui_library/slider.c ui_library/sidebar.c \
             ui_library/shadow.c ui_library/blur_effect.c ui_library/backdrop.c

all: example1 example2 example3 example4

example1: examples/example1.c $(UI_SOURCES)
	$(CC) $(CFLAGS) -o $@ $^ $(LIBS)

example2: examples/example2.c $(UI_SOURCES)
	$(CC) $(CFLAGS) -o $@ $^ $(LIBS)

example3: examples/example3.c $(UI_SOURCES)
	$(CC) $(CFLAGS) -o $@ $^ $(LIBS)

example4: examples/example4.c $(UI_SOURCES)
	$(CC) $(CFLAGS) -o $@ $^ $(LIBS)

clean:
	rm -f example1 example2 example3 example4
```

These examples demonstrate the full capabilities of the UI library and provide a solid foundation for building your own applications.