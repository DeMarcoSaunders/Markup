#include "raylib.h"
#include "markup.h" // Includes all UI components
#include <stdio.h>
#include <string.h>

// Application state
typedef struct {
    int current_tab;
    bool show_about_modal;
    bool show_confirm_modal;
    bool settings_changed;
} AppState;

// Tab definitions
typedef enum {
    TAB_GENERAL = 0,
    TAB_GRAPHICS = 1,
    TAB_AUDIO = 2,
    TAB_PROFILE = 3,
    TAB_COUNT = 4
} TabType;

const char* tab_names[TAB_COUNT] = {"General", "Graphics", "Audio", "Profile"};

// Helper function to draw a section header
void DrawSectionHeader(const char* title, float x, float y) {
    DrawText(title, (int)x, (int)y, 18, (Color){55, 65, 81, 255});
    DrawRectangle((int)x, (int)y + 25, 300, 1, (Color){209, 213, 219, 255});
}

// Helper function to draw a labeled component
void DrawLabel(const char* label, float x, float y) {
    DrawText(label, (int)x, (int)y, 14, (Color){75, 85, 99, 255});
}

int main(void) {
    // Initialization
    const int screenWidth = 1200;
    const int screenHeight = 800;
    InitWindow(screenWidth, screenHeight, "MarkUp UI Library - Comprehensive Showcase");
    SetTargetFPS(60);

    Theme_Init();

    // Application state
    AppState app_state = {0};
    app_state.current_tab = TAB_GENERAL;

    // Layout constants
    const float sidebar_width = 200;
    const float header_height = 80;
    const float content_padding = 30;
    
    Rectangle sidebar_area = {0, header_height, sidebar_width, screenHeight - header_height};
    Rectangle content_area = {sidebar_width + content_padding, header_height + content_padding, 
                             screenWidth - sidebar_width - content_padding * 2, 
                             screenHeight - header_height - content_padding * 2};

    // --- Header Components ---
    ImageBox app_logo = ImageBox_Create((Rectangle){20, 20, 40, 40});
    ImageBox_SetBackground(&app_logo, true, (Color){59, 130, 246, 255});
    ImageBox_SetBorderRadius(&app_logo, 8.0f);

    ImageBox user_avatar = ImageBox_Create((Rectangle){screenWidth - 80, 20, 40, 40});
    ImageBox_SetBackground(&user_avatar, true, (Color){34, 197, 94, 150});
    ImageBox_SetBorderRadius(&user_avatar, 20.0f);
    ImageBox_SetBorder(&user_avatar, true, (Color){34, 197, 94, 255}, 2.0f);
    ImageBox_SetClickable(&user_avatar, true);

    ImageBox notification_badge = ImageBox_Create((Rectangle){screenWidth - 50, 15, 16, 16});
    ImageBox_SetBackground(&notification_badge, true, (Color){239, 68, 68, 255});
    ImageBox_SetBorderRadius(&notification_badge, 8.0f);
    ImageBox_SetBorder(&notification_badge, true, WHITE, 1.0f);

    // --- Collapsible Sidebar ---
    Sidebar nav_sidebar = Sidebar_Create(sidebar_area, SIDEBAR_LEFT);
    Sidebar_SetFlexDirection(&nav_sidebar, FLEX_DIRECTION_COLUMN);
    Sidebar_SetJustifyContent(&nav_sidebar, JUSTIFY_START);
    Sidebar_SetGap(&nav_sidebar, 10.0f);
    Sidebar_SetPadding(&nav_sidebar, 20, 10, 20, 10);

    // Sidebar toggle button
    Button sidebar_toggle = Button_Create((Rectangle){10, 10, 30, 30}, "☰", BUTTON_SECONDARY);
    Button_SetBorderRadius(&sidebar_toggle, 4.0f, 4.0f, 4.0f, 4.0f);

    // Tab navigation buttons
    Button tab_buttons[TAB_COUNT];
    for (int i = 0; i < TAB_COUNT; i++) {
        tab_buttons[i] = Button_Create((Rectangle){0, 0, sidebar_width - 40, 40}, 
                                      tab_names[i], BUTTON_SECONDARY);
        Button_SetBorderRadius(&tab_buttons[i], 6.0f, 6.0f, 6.0f, 6.0f);
        Sidebar_AddButton(&nav_sidebar, &tab_buttons[i]);
    }

    // --- Content Panels for Each Tab ---
    
    // General Tab Panel
    Rectangle general_content = {content_area.x, content_area.y + 80, content_area.width, content_area.height - 130};
    Panel general_panel = Panel_Create(general_content);
    Panel_SetFlexDirection(&general_panel, FLEX_DIRECTION_COLUMN);
    Panel_SetJustifyContent(&general_panel, JUSTIFY_START);
    Panel_SetAlignItems(&general_panel, ALIGN_START);
    Panel_SetGap(&general_panel, 20.0f);

    // General Tab Components
    Checkbox auto_save = Checkbox_Create((Rectangle){0, 0, 20, 20}, "Auto-save settings", true);
    Checkbox notifications = Checkbox_Create((Rectangle){0, 0, 20, 20}, "Show notifications", false);
    Checkbox dark_mode = Checkbox_Create((Rectangle){0, 0, 20, 20}, "Dark mode", false);
    
    Dropdown language_dropdown = Dropdown_Create((Rectangle){0, 0, 200, 40}, "Language", BUTTON_SECONDARY);
    Dropdown_AddOption(&language_dropdown, "English");
    Dropdown_AddOption(&language_dropdown, "Spanish");
    Dropdown_AddOption(&language_dropdown, "French");
    Dropdown_AddOption(&language_dropdown, "German");
    Dropdown_SetSelected(&language_dropdown, 0);
    
    // Font selection dropdown
    Dropdown font_dropdown = Dropdown_Create((Rectangle){0, 0, 250, 40}, "Font Family", BUTTON_SECONDARY);
    
    // Populate font dropdown with available fonts
    int font_count = Theme_GetFontCount();
    for (int i = 0; i < font_count; i++) {
        Dropdown_AddOption(&font_dropdown, Theme_GetFontName(i));
    }
    if (font_count > 0) {
        Dropdown_SetSelected(&font_dropdown, Theme_GetCurrentFontIndex());
    }

    // Add components to general panel
    Panel_AddCustom(&general_panel, &auto_save, 0, 30.0f);
    Panel_AddCustom(&general_panel, &notifications, 0, 30.0f);
    Panel_AddCustom(&general_panel, &dark_mode, 0, 30.0f);
    Panel_AddCustom(&general_panel, &language_dropdown, 0, 50.0f);
    Panel_AddCustom(&general_panel, &font_dropdown, 0, 50.0f);

    // Graphics Tab Panel
    Panel graphics_panel = Panel_Create(general_content); // Same area as general
    Panel_SetFlexDirection(&graphics_panel, FLEX_DIRECTION_COLUMN);
    Panel_SetJustifyContent(&graphics_panel, JUSTIFY_START);
    Panel_SetAlignItems(&graphics_panel, ALIGN_START);
    Panel_SetGap(&graphics_panel, 20.0f);

    // Graphics Tab Components
    Slider resolution_scale = Slider_Create((Rectangle){0, 0, 300, 25}, 50, 200, 100);
    Slider brightness = Slider_Create((Rectangle){0, 0, 300, 25}, 0, 100, 75);
    Slider contrast = Slider_Create((Rectangle){0, 0, 300, 25}, 0, 100, 50);
    Dropdown quality_preset = Dropdown_Create((Rectangle){0, 0, 200, 40}, "Quality Preset", BUTTON_SECONDARY);
    Checkbox vsync = Checkbox_Create((Rectangle){0, 0, 20, 20}, "Enable V-Sync", true);
    Checkbox fullscreen = Checkbox_Create((Rectangle){0, 0, 20, 20}, "Fullscreen mode", false);

    Dropdown_AddOption(&quality_preset, "Low");
    Dropdown_AddOption(&quality_preset, "Medium");
    Dropdown_AddOption(&quality_preset, "High");
    Dropdown_AddOption(&quality_preset, "Ultra");
    Dropdown_SetSelected(&quality_preset, 2);

    // Add components to graphics panel
    Panel_AddCustom(&graphics_panel, &resolution_scale, 0, 35.0f);
    Panel_AddCustom(&graphics_panel, &brightness, 0, 35.0f);
    Panel_AddCustom(&graphics_panel, &contrast, 0, 35.0f);
    Panel_AddCustom(&graphics_panel, &quality_preset, 0, 50.0f);
    Panel_AddCustom(&graphics_panel, &vsync, 0, 30.0f);
    Panel_AddCustom(&graphics_panel, &fullscreen, 0, 30.0f);

    // Audio Tab Panel
    Panel audio_panel = Panel_Create(general_content); // Same area as general
    Panel_SetFlexDirection(&audio_panel, FLEX_DIRECTION_COLUMN);
    Panel_SetJustifyContent(&audio_panel, JUSTIFY_START);
    Panel_SetAlignItems(&audio_panel, ALIGN_START);
    Panel_SetGap(&audio_panel, 20.0f);

    // Audio Tab Components
    Slider master_volume = Slider_Create((Rectangle){0, 0, 300, 25}, 0, 100, 85);
    Slider music_volume = Slider_Create((Rectangle){0, 0, 300, 25}, 0, 100, 70);
    Slider sfx_volume = Slider_Create((Rectangle){0, 0, 300, 25}, 0, 100, 90);
    Dropdown audio_device = Dropdown_Create((Rectangle){0, 0, 250, 40}, "Audio Device", BUTTON_SECONDARY);
    Checkbox mute_when_unfocused = Checkbox_Create((Rectangle){0, 0, 20, 20}, "Mute when unfocused", true);

    Dropdown_AddOption(&audio_device, "Default Device");
    Dropdown_AddOption(&audio_device, "Speakers");
    Dropdown_AddOption(&audio_device, "Headphones");
    Dropdown_SetSelected(&audio_device, 0);

    // Add components to audio panel
    Panel_AddCustom(&audio_panel, &master_volume, 0, 35.0f);
    Panel_AddCustom(&audio_panel, &music_volume, 0, 35.0f);
    Panel_AddCustom(&audio_panel, &sfx_volume, 0, 35.0f);
    Panel_AddCustom(&audio_panel, &audio_device, 0, 50.0f);
    Panel_AddCustom(&audio_panel, &mute_when_unfocused, 0, 30.0f);

    // Profile Tab Panel
    Panel profile_panel = Panel_Create(general_content); // Same area as general
    Panel_SetFlexDirection(&profile_panel, FLEX_DIRECTION_COLUMN);
    Panel_SetJustifyContent(&profile_panel, JUSTIFY_START);
    Panel_SetAlignItems(&profile_panel, ALIGN_START);
    Panel_SetGap(&profile_panel, 20.0f);

    // Profile Tab Components
    ImageBox profile_picture = ImageBox_Create((Rectangle){0, 0, 80, 80});
    ImageBox_SetBackground(&profile_picture, true, (Color){156, 163, 175, 255});
    ImageBox_SetBorderRadius(&profile_picture, 8.0f);
    ImageBox_SetBorder(&profile_picture, true, (Color){209, 213, 219, 255}, 2.0f);

    Button change_avatar = Button_Create((Rectangle){0, 0, 100, 30}, "Change", BUTTON_SECONDARY);
    Button_SetBorderRadius(&change_avatar, 4.0f, 4.0f, 4.0f, 4.0f);

    // Add components to profile panel
    Panel_AddCustom(&profile_panel, &profile_picture, 0, 90.0f);
    Panel_AddCustom(&profile_panel, &change_avatar, 0, 40.0f);

    // Text input components for profile
    TextInput username_input = TextInput_Create((Rectangle){content_area.x + 120, content_area.y + 80, 200, 35}, 
                                               "Enter username");
    TextInput_SetText(&username_input, "JohnDoe");

    TextInput email_input = TextInput_Create((Rectangle){content_area.x + 120, content_area.y + 125, 200, 35}, 
                                            "Enter email");
    TextInput_SetText(&email_input, "john@example.com");
    TextInput_SetType(&email_input, INPUT_TYPE_EMAIL);

    TextArea bio_area = TextArea_Create((Rectangle){content_area.x + 120, content_area.y + 170, 300, 100}, 
                                       "Tell us about yourself...");
    TextArea_SetText(&bio_area, "Software developer passionate about UI/UX design and creating intuitive user experiences.");

    // --- Action Buttons ---
    Button save_button = Button_Create((Rectangle){content_area.x + content_area.width - 240, 
                                                  content_area.y + content_area.height - 50, 100, 40}, 
                                      "Save", BUTTON_PRIMARY);
    Button_SetBorderRadius(&save_button, 6.0f, 6.0f, 6.0f, 6.0f);
    Button_SetShadow(&save_button, true);

    Button cancel_button = Button_Create((Rectangle){content_area.x + content_area.width - 130, 
                                                    content_area.y + content_area.height - 50, 100, 40}, 
                                        "Cancel", BUTTON_SECONDARY);
    Button_SetBorderRadius(&cancel_button, 6.0f, 6.0f, 6.0f, 6.0f);

    Button reset_button = Button_Create((Rectangle){content_area.x + 20, 
                                                   content_area.y + content_area.height - 50, 100, 40}, 
                                       "Reset", BUTTON_DESTRUCTIVE);
    Button_SetBorderRadius(&reset_button, 6.0f, 6.0f, 6.0f, 6.0f);

    // --- Modals ---
    Modal about_modal = Modal_Create("About MarkUp UI", MODAL_SIZE_MEDIUM);
    Modal_SetBlurEffect(&about_modal, true);

    Modal confirm_modal = Modal_Create("Confirm Changes", MODAL_SIZE_SMALL);
    Modal_SetBlurEffect(&confirm_modal, true);

    // --- Separator Components ---
    // Header separator (below app title)
    Separator header_separator = Separator_CreateHorizontal(content_area.x, content_area.y + 50, content_area.width);
    Separator_SetColor(&header_separator, (Color){209, 213, 219, 255});
    Separator_SetThickness(&header_separator, 1.0f);

    // Form section separators for each tab
    Separator general_section = Separator_CreateFormSection(content_area.x, content_area.y + 100, 
                                                           content_area.width, "General Settings");
    
    Separator graphics_section = Separator_CreateFormSection(content_area.x, content_area.y + 100, 
                                                            content_area.width, "Display & Graphics");
    
    Separator audio_section = Separator_CreateFormSection(content_area.x, content_area.y + 100, 
                                                         content_area.width, "Audio Settings");
    
    Separator profile_section = Separator_CreateFormSection(content_area.x, content_area.y + 100, 
                                                           content_area.width, "Profile Information");

    // Content break separators within tabs
    Separator preferences_break = Separator_CreateContentBreak(content_area.x, content_area.y + 250, 
                                                              content_area.width);
    
    Separator advanced_break = Separator_CreateContentBreak(content_area.x, content_area.y + 350, 
                                                           content_area.width);

    // Vertical separator between action buttons
    Separator button_separator = Separator_CreateToolbarDivider(content_area.x + content_area.width - 250, 
                                                               content_area.y + content_area.height - 50, 40);

    // Custom "OR" separator for demonstration
    Separator or_separator = Separator_CreateWithText((Rectangle){content_area.x + 50, content_area.y + 400, 200, 20}, 
                                                     "OR", SEPARATOR_TEXT_CENTER);
    Separator_SetStyle(&or_separator, SEPARATOR_DASHED);
    Separator_SetTextColor(&or_separator, (Color){107, 114, 128, 255});
    Separator_SetDashPattern(&or_separator, 6.0f, 4.0f);

    // Main game loop
    while (!WindowShouldClose()) {
        // --- Update ---
        
        // Header components
        ImageBox_Update(&app_logo);
        ImageBox_Update(&user_avatar);
        ImageBox_Update(&notification_badge);

        // Sidebar toggle
        Button_Update(&sidebar_toggle);
        if (sidebar_toggle.is_clicked) {
            Sidebar_Toggle(&nav_sidebar);
        }

        // Sidebar and navigation
        Sidebar_Update(&nav_sidebar);
        for (int i = 0; i < TAB_COUNT; i++) {
            Button_Update(&tab_buttons[i]);
            if (tab_buttons[i].is_clicked) {
                app_state.current_tab = i;
            }
        }

        // Update content panels based on current tab
        switch (app_state.current_tab) {
            case TAB_GENERAL:
                Panel_Update(&general_panel);
                Checkbox_Update(&auto_save);
                Checkbox_Update(&notifications);
                Checkbox_Update(&dark_mode);
                Dropdown_Update(&language_dropdown);
                Dropdown_Update(&font_dropdown);
                
                // Handle font selection changes
                static int last_font_selection = -1;
                int current_font_selection = Dropdown_GetSelected(&font_dropdown);
                if (current_font_selection != last_font_selection && current_font_selection >= 0) {
                    if (current_font_selection < Theme_GetFontCount()) {
                        Theme_SetCurrentFont(current_font_selection);
                        printf("Font changed to: %s\n", Theme_GetFontName(current_font_selection));
                    }
                    last_font_selection = current_font_selection;
                }
                break;
                
            case TAB_GRAPHICS:
                Panel_Update(&graphics_panel);
                Slider_Update(&resolution_scale);
                Slider_Update(&brightness);
                Slider_Update(&contrast);
                Dropdown_Update(&quality_preset);
                Checkbox_Update(&vsync);
                Checkbox_Update(&fullscreen);
                break;
                
            case TAB_AUDIO:
                Panel_Update(&audio_panel);
                Slider_Update(&master_volume);
                Slider_Update(&music_volume);
                Slider_Update(&sfx_volume);
                Dropdown_Update(&audio_device);
                Checkbox_Update(&mute_when_unfocused);
                break;
                
            case TAB_PROFILE:
                Panel_Update(&profile_panel);
                ImageBox_Update(&profile_picture);
                Button_Update(&change_avatar);
                TextArea_Update(&bio_area);
                break;
        }

        // Action buttons
        Button_Update(&save_button);
        Button_Update(&cancel_button);
        Button_Update(&reset_button);

        // Modal updates
        Modal_Update(&about_modal);
        Modal_Update(&confirm_modal);

        // Separator updates
        Separator_Update(&header_separator);
        Separator_Update(&general_section);
        Separator_Update(&graphics_section);
        Separator_Update(&audio_section);
        Separator_Update(&profile_section);
        Separator_Update(&preferences_break);
        Separator_Update(&advanced_break);
        Separator_Update(&button_separator);
        Separator_Update(&or_separator);

        // Handle button clicks
        if (save_button.is_clicked) {
            app_state.show_confirm_modal = true;
            Modal_Show(&confirm_modal);
        }
        
        if (cancel_button.is_clicked) {
            // Reset all settings to defaults (simplified)
            app_state.settings_changed = false;
        }

        if (reset_button.is_clicked) {
            app_state.show_confirm_modal = true;
            Modal_Show(&confirm_modal);
        }

        if (ImageBox_IsClicked(&user_avatar)) {
            app_state.show_about_modal = true;
            Modal_Show(&about_modal);
        }

        // --- Drawing ---
        BeginDrawing();
            
            ClearBackground((Color){248, 250, 252, 255}); // Very light background
            
            // Draw header
            DrawRectangle(0, 0, screenWidth, header_height, WHITE);
            DrawRectangle(0, header_height - 1, screenWidth, 1, (Color){229, 231, 235, 255});
            
            // App logo and title
            ImageBox_Draw(&app_logo);
            DrawText("MarkUp UI", 75, 30, 24, (Color){31, 41, 55, 255});
            DrawText("Comprehensive Showcase", 75, 50, 12, (Color){107, 114, 128, 255});
            
            // Header right side
            ImageBox_Draw(&user_avatar);
            ImageBox_Draw(&notification_badge);
            
            // Draw procedural icons
            Vector2 logo_center = {app_logo.bounds.x + 20, app_logo.bounds.y + 20};
            DrawText("M", (int)logo_center.x - 8, (int)logo_center.y - 10, 20, WHITE);
            
            Vector2 avatar_center = {user_avatar.bounds.x + 20, user_avatar.bounds.y + 20};
            DrawText("JD", (int)avatar_center.x - 8, (int)avatar_center.y - 6, 12, WHITE);
            
            Vector2 notif_center = {notification_badge.bounds.x + 8, notification_badge.bounds.y + 8};
            DrawText("3", (int)notif_center.x - 3, (int)notif_center.y - 4, 8, WHITE);
            
            // Draw sidebar toggle button
            Button_Draw(&sidebar_toggle);
            
            // Draw collapsible sidebar
            Sidebar_Draw(&nav_sidebar);
            
            // Update tab button styles and draw them
            for (int i = 0; i < TAB_COUNT; i++) {
                if (i == app_state.current_tab) {
                    Button_SetBackgroundColor(&tab_buttons[i], (Color){59, 130, 246, 255});
                    Button_SetTextColor(&tab_buttons[i], WHITE);
                } else {
                    Button_ResetAllCustomStyles(&tab_buttons[i]);
                }
            }
            
            // Draw content area
            DrawText(tab_names[app_state.current_tab], (int)content_area.x, (int)content_area.y, 28, 
                    (Color){31, 41, 55, 255});
            
            // Draw content based on current tab
            switch (app_state.current_tab) {
                case TAB_GENERAL:
                    DrawSectionHeader("Preferences", content_area.x, content_area.y + 50);
                    Panel_Draw(&general_panel);
                    break;
                    
                case TAB_GRAPHICS:
                    DrawSectionHeader("Display Settings", content_area.x, content_area.y + 50);
                    Panel_Draw(&graphics_panel);
                    break;
                    
                case TAB_AUDIO:
                    DrawSectionHeader("Audio Settings", content_area.x, content_area.y + 50);
                    Panel_Draw(&audio_panel);
                    break;
                    
                case TAB_PROFILE:
                    DrawSectionHeader("User Profile", content_area.x, content_area.y + 50);
                    Panel_Draw(&profile_panel);
                    
                    // Draw placeholder profile picture content
                    Vector2 profile_center = {profile_picture.bounds.x + 40, profile_picture.bounds.y + 40};
                    DrawText("USER", (int)profile_center.x - 20, (int)profile_center.y - 8, 16, 
                            (Color){107, 114, 128, 255});
                    
                    // Profile form fields
                    DrawLabel("Username:", content_area.x + 120, content_area.y + 60);
                    TextInput_Draw(&username_input);
                    
                    DrawLabel("Email:", content_area.x + 120, content_area.y + 105);
                    TextInput_Draw(&email_input);
                    
                    DrawLabel("Bio:", content_area.x + 120, content_area.y + 150);
                    TextArea_Draw(&bio_area);
                    
                    Button_Draw(&change_avatar);
                    
                    DrawText("Click avatar in header for more info", (int)content_area.x + 20, (int)content_area.y + 290, 
                            12, (Color){107, 114, 128, 255});
                    break;
            }
            
            // Draw separators
            Separator_Draw(&header_separator);
            
            // Draw tab-specific separators
            switch (app_state.current_tab) {
                case TAB_GENERAL:
                    Separator_Draw(&general_section);
                    Separator_Draw(&preferences_break);
                    break;
                case TAB_GRAPHICS:
                    Separator_Draw(&graphics_section);
                    Separator_Draw(&advanced_break);
                    break;
                case TAB_AUDIO:
                    Separator_Draw(&audio_section);
                    break;
                case TAB_PROFILE:
                    Separator_Draw(&profile_section);
                    Separator_Draw(&or_separator);
                    break;
            }
            
            // Draw action button separator
            Separator_Draw(&button_separator);
            
            // Draw action buttons
            Button_Draw(&save_button);
            Button_Draw(&cancel_button);
            Button_Draw(&reset_button);
            
            // Draw modals
            if (Modal_IsVisible(&about_modal)) {
                Modal_Draw(&about_modal);
                // Draw modal content
                Rectangle modal_content = Modal_GetContentArea(&about_modal);
                DrawText("MarkUp UI Library v1.0", (int)modal_content.x + 20, (int)modal_content.y + 20, 
                        18, (Color){31, 41, 55, 255});
                DrawText("A comprehensive UI library for Raylib", (int)modal_content.x + 20, (int)modal_content.y + 50, 
                        14, (Color){75, 85, 99, 255});
                DrawText("Features:", (int)modal_content.x + 20, (int)modal_content.y + 80, 14, 
                        (Color){75, 85, 99, 255});
                DrawText("• Modern component design", (int)modal_content.x + 30, (int)modal_content.y + 100, 
                        12, (Color){107, 114, 128, 255});
                DrawText("• Flexible theming system", (int)modal_content.x + 30, (int)modal_content.y + 120, 
                        12, (Color){107, 114, 128, 255});
                DrawText("• Responsive layouts", (int)modal_content.x + 30, (int)modal_content.y + 140, 
                        12, (Color){107, 114, 128, 255});
                DrawText("• Rich interaction support", (int)modal_content.x + 30, (int)modal_content.y + 160, 
                        12, (Color){107, 114, 128, 255});
            }
            
            if (Modal_IsVisible(&confirm_modal)) {
                Modal_Draw(&confirm_modal);
                Rectangle modal_content = Modal_GetContentArea(&confirm_modal);
                DrawText("Are you sure you want to save these changes?", 
                        (int)modal_content.x + 20, (int)modal_content.y + 20, 14, (Color){31, 41, 55, 255});
            }

        EndDrawing();
    }

    // --- De-Initialization ---
    ImageBox_Destroy(&app_logo);
    ImageBox_Destroy(&user_avatar);
    ImageBox_Destroy(&notification_badge);
    
    Button_Destroy(&sidebar_toggle);
    Sidebar_Destroy(&nav_sidebar);
    
    for (int i = 0; i < TAB_COUNT; i++) {
        Button_Destroy(&tab_buttons[i]);
    }
    
    // Destroy content panels
    Panel_Destroy(&general_panel);
    Panel_Destroy(&graphics_panel);
    Panel_Destroy(&audio_panel);
    Panel_Destroy(&profile_panel);
    
    Checkbox_Destroy(&auto_save);
    Checkbox_Destroy(&notifications);
    Checkbox_Destroy(&dark_mode);
    Dropdown_Destroy(&language_dropdown);
    Dropdown_Destroy(&font_dropdown);
    
    Slider_Destroy(&resolution_scale);
    Slider_Destroy(&brightness);
    Slider_Destroy(&contrast);
    Dropdown_Destroy(&quality_preset);
    Checkbox_Destroy(&vsync);
    Checkbox_Destroy(&fullscreen);
    
    Slider_Destroy(&master_volume);
    Slider_Destroy(&music_volume);
    Slider_Destroy(&sfx_volume);
    Dropdown_Destroy(&audio_device);
    Checkbox_Destroy(&mute_when_unfocused);
    
    ImageBox_Destroy(&profile_picture);
    Button_Destroy(&change_avatar);
    TextInput_Destroy(&username_input);
    TextInput_Destroy(&email_input);
    TextArea_Destroy(&bio_area);
    
    Button_Destroy(&save_button);
    Button_Destroy(&cancel_button);
    Button_Destroy(&reset_button);
    
    Modal_Destroy(&about_modal);
    Modal_Destroy(&confirm_modal);

    // Destroy separators
    Separator_Destroy(&header_separator);
    Separator_Destroy(&general_section);
    Separator_Destroy(&graphics_section);
    Separator_Destroy(&audio_section);
    Separator_Destroy(&profile_section);
    Separator_Destroy(&preferences_break);
    Separator_Destroy(&advanced_break);
    Separator_Destroy(&button_separator);
    Separator_Destroy(&or_separator);
    
    // Cleanup fonts
    Theme_CleanupFonts();
    
    CloseWindow();
    return 0;
}