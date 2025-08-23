#include "raylib.h"
#include "components/button.h"
#include "components/checkbox.h"
#include "components/dropdown.h"
#include "components/modal.h"
#include "components/panel.h"
#include "components/radio_button.h"
#include "components/sidebar.h"
#include "components/slider.h"
#include "components/text_area.h"
#include "components/text_input.h"
#include "components/blur_effect.h"
#include "theme.h"
#include <stdio.h>

// Demo state
typedef struct {
    bool show_modal;
    bool checkbox_checked;
    int radio_selection;
    float slider_value;
    char text_input_buffer[256];
    char text_area_buffer[1024];
    int selected_dropdown_option;
} DemoState;

int main(void) {
    // Initialize window
    const int screenWidth = 1400;
    const int screenHeight = 900;
    InitWindow(screenWidth, screenHeight, "Markup UI - Component Showcase");
    SetTargetFPS(60);

    // Initialize UI system
    Theme_Init_Default();
    BlurEffect_Init();

    // Initialize demo state
    DemoState demo = {0};
    demo.radio_selection = 1;
    demo.slider_value = 50.0f;
    strcpy(demo.text_input_buffer, "Type here...");
    strcpy(demo.text_area_buffer, "This is a multi-line text area.\nYou can type multiple lines of text here.\n\nFeatures:\n• Line numbers\n• Syntax highlighting\n• Auto-indent\n• Word wrap");
    demo.selected_dropdown_option = -1;

    // Create main layout panels
    Panel main_panel = Panel_Create((Rectangle){10, 10, screenWidth - 20, screenHeight - 20});
    Panel_SetFlexDirection(&main_panel, FLEX_DIRECTION_ROW);
    Panel_SetGap(&main_panel, 20);
    Panel_SetAlignItems(&main_panel, ALIGN_START);

    // Left panel for basic components
    Panel left_panel = Panel_Create((Rectangle){0, 0, 400, 0});
    Panel_SetFlexDirection(&left_panel, FLEX_DIRECTION_COLUMN);
    Panel_SetGap(&left_panel, 15);
    Panel_SetAlignItems(&left_panel, ALIGN_START);

    // Right panel for advanced components
    Panel right_panel = Panel_Create((Rectangle){0, 0, 400, 0});
    Panel_SetFlexDirection(&right_panel, FLEX_DIRECTION_COLUMN);
    Panel_SetGap(&right_panel, 15);
    Panel_SetAlignItems(&right_panel, ALIGN_START);

    // Center panel for interactive demo
    Panel center_panel = Panel_Create((Rectangle){0, 0, 500, 0});
    Panel_SetFlexDirection(&center_panel, FLEX_DIRECTION_COLUMN);
    Panel_SetGap(&center_panel, 15);
    Panel_SetAlignItems(&center_panel, ALIGN_START);

    // Create components for left panel (Basic Components)
    Button primary_btn = Button_Create((Rectangle){0, 0, 200, 40}, "Primary Button", BUTTON_PRIMARY);
    Button secondary_btn = Button_Create((Rectangle){0, 0, 200, 40}, "Secondary Button", BUTTON_SECONDARY);
    Button destructive_btn = Button_Create((Rectangle){0, 0, 200, 40}, "Destructive Button", BUTTON_DESTRUCTIVE);
    
    Checkbox checkbox = Checkbox_Create((Rectangle){0, 0, 20, 20}, "Enable Feature", false);
    
    Slider slider = Slider_Create((Rectangle){0, 0, 200, 20}, 0, 100, 50);
    Slider_SetShowValue(&slider, true);

    // Create components for right panel (Advanced Components)
    TextInput text_input = TextInput_Create((Rectangle){0, 0, 300, 40}, "Enter your name...");
    TextArea text_area = TextArea_Create((Rectangle){0, 0, 300, 200}, "Multi-line text area...");
    TextArea_SetShowLineNumbers(&text_area, true);
    TextArea_SetWordWrap(&text_area, true);

    Dropdown dropdown = Dropdown_Create((Rectangle){0, 0, 200, 40}, "Choose Option", BUTTON_PRIMARY);
    Dropdown_AddOption(&dropdown, "Option 1");
    Dropdown_AddOption(&dropdown, "Option 2");
    Dropdown_AddSeparator(&dropdown);
    Dropdown_AddOption(&dropdown, "Option 3");
    Dropdown_AddOption(&dropdown, "Option 4");
    Dropdown_SetSearchEnabled(&dropdown, true);

    // Create components for center panel (Interactive Demo)
    Button open_modal_btn = Button_Create((Rectangle){0, 0, 200, 40}, "Open Modal", BUTTON_PRIMARY);
    
    RadioGroup* radio_group = RadioGroup_Create();
    RadioButton radio1 = RadioButton_Create((Rectangle){0, 0, 20, 20}, "Choice A", 1);
    RadioButton radio2 = RadioButton_Create((Rectangle){0, 0, 20, 20}, "Choice B", 2);
    RadioButton radio3 = RadioButton_Create((Rectangle){0, 0, 20, 20}, "Choice C", 3);
    RadioGroup_AddButton(radio_group, &radio1);
    RadioGroup_AddButton(radio_group, &radio2);
    RadioGroup_AddButton(radio_group, &radio3);
    RadioGroup_SetSelected(radio_group, 2);

    // Create modal
    Modal modal = Modal_Create("Settings", MODAL_SIZE_MEDIUM);
    Modal_SetBlurEffect(&modal, true);
    
    // Create sidebar for navigation
    Sidebar sidebar = Sidebar_Create((Rectangle){0, 0, 250, screenHeight}, SIDEBAR_LEFT);
    Sidebar_SetFlexDirection(&sidebar, FLEX_DIRECTION_COLUMN);
    Sidebar_SetGap(&sidebar, 10);
    
    // Add navigation buttons to sidebar
    Button nav_home = Button_Create((Rectangle){0, 0, 200, 40}, "Home", BUTTON_PRIMARY);
    Button nav_settings = Button_Create((Rectangle){0, 0, 200, 40}, "Settings", BUTTON_SECONDARY);
    Button nav_help = Button_Create((Rectangle){0, 0, 200, 40}, "Help", BUTTON_SECONDARY);
    
    Sidebar_AddButton(&sidebar, &nav_home);
    Sidebar_AddButton(&sidebar, &nav_settings);
    Sidebar_AddButton(&sidebar, &nav_help);
    
    // Add components to panels
    Panel_AddButton(&left_panel, &primary_btn, 0, 40);
    Panel_AddButton(&left_panel, &secondary_btn, 0, 40);
    Panel_AddButton(&left_panel, &destructive_btn, 0, 40);
    Panel_AddSlider(&left_panel, &slider, 0, 20);
    
    Panel_AddTextInput(&right_panel, &text_input, 0, 40);
    Panel_AddTextArea(&right_panel, &text_area, 1, 200);
    Panel_AddDropdown(&right_panel, &dropdown, 0, 40);
    
    Panel_AddButton(&center_panel, &open_modal_btn, 0, 40);

    // Add panels to main panel
    Panel_AddPanel(&main_panel, &left_panel, 0, 400);
    Panel_AddPanel(&main_panel, &center_panel, 1, 500);
    Panel_AddPanel(&main_panel, &right_panel, 0, 400);

    // Main game loop
    while (!WindowShouldClose()) {
        // Update
        Panel_Update(&main_panel);
        Checkbox_Update(&checkbox);
        RadioGroup_UpdateAll(radio_group);
        Modal_Update(&modal);
        Sidebar_Update(&sidebar);

        // Handle button interactions
        if (Button_IsClicked(&open_modal_btn)) {
            demo.show_modal = true;
            Modal_Show(&modal);
        }

        if (Button_IsClicked(&nav_settings)) {
            demo.show_modal = true;
            Modal_Show(&modal);
        }

        if (Button_IsClicked(&primary_btn)) {
            printf("Primary button clicked!\n");
        }

        if (Button_IsClicked(&secondary_btn)) {
            printf("Secondary button clicked!\n");
        }

        if (Button_IsClicked(&destructive_btn)) {
            printf("Destructive button clicked!\n");
        }

        // Update demo state
        demo.checkbox_checked = checkbox.is_checked;
        demo.radio_selection = RadioGroup_GetSelected(radio_group);
        demo.slider_value = Slider_GetValue(&slider);
        demo.selected_dropdown_option = Dropdown_GetSelected(&dropdown);

        // Handle modal close
        if (demo.show_modal && !Modal_IsVisible(&modal)) {
            demo.show_modal = false;
        }

        // Drawing
        BeginDrawing();
        ClearBackground((Color){245, 245, 245, 255});

        // Draw background pattern
        DrawRectangle(0, 0, screenWidth, screenHeight, (Color){245, 245, 245, 255});
        
        // Draw title
        DrawText("Markup UI - Component Showcase", 20, 20, 24, DARKGRAY);
        DrawText("A comprehensive UI component library built with raylib", 20, 50, 16, GRAY);

        // Draw panels
        Panel_Draw(&main_panel);
        
        // Draw sidebar
        Sidebar_Draw(&sidebar);
        
        // Manually position and draw components that aren't in panels
        checkbox.bounds = (Rectangle){left_panel.rect.x + 50, left_panel.rect.y + 200, 20, 20};
        Checkbox_Draw(&checkbox);

        radio1.bounds = (Rectangle){center_panel.rect.x + 50, center_panel.rect.y + 100, 20, 20};
        radio2.bounds = (Rectangle){center_panel.rect.x + 50, center_panel.rect.y + 130, 20, 20};
        radio3.bounds = (Rectangle){center_panel.rect.x + 50, center_panel.rect.y + 160, 20, 20};
        RadioGroup_DrawAll(radio_group);

        // Draw section headers
        DrawText("Basic Components", left_panel.rect.x, left_panel.rect.y - 25, 18, DARKGRAY);
        DrawText("Interactive Demo", center_panel.rect.x, center_panel.rect.y - 25, 18, DARKGRAY);
        DrawText("Advanced Components", right_panel.rect.x, right_panel.rect.y - 25, 18, DARKGRAY);

        // Draw demo state information
        int info_y = center_panel.rect.y + 250;
        DrawText("Current State:", center_panel.rect.x, info_y, 16, DARKGRAY);
        DrawText(TextFormat("Checkbox: %s", demo.checkbox_checked ? "Checked" : "Unchecked"), 
                center_panel.rect.x, info_y + 25, 14, GRAY);
        DrawText(TextFormat("Radio Selection: %d", demo.radio_selection), 
                center_panel.rect.x, info_y + 45, 14, GRAY);
        DrawText(TextFormat("Slider Value: %.1f", demo.slider_value), 
                center_panel.rect.x, info_y + 65, 14, GRAY);
        
        const char* dropdown_text = demo.selected_dropdown_option >= 0 ? 
            Dropdown_GetSelectedText(&dropdown) : "None";
        DrawText(TextFormat("Dropdown: %s", dropdown_text), 
                center_panel.rect.x, info_y + 85, 14, GRAY);

        // Draw instructions
        int instructions_y = screenHeight - 120;
        DrawText("Instructions:", 20, instructions_y, 16, DARKGRAY);
        DrawText("• Click buttons to see interactions", 20, instructions_y + 20, 12, GRAY);
        DrawText("• Use the slider to adjust values", 20, instructions_y + 35, 12, GRAY);
        DrawText("• Type in text inputs and areas", 20, instructions_y + 50, 12, GRAY);
        DrawText("• Select options from dropdowns", 20, instructions_y + 65, 12, GRAY);
        DrawText("• Toggle checkboxes and radio buttons", 20, instructions_y + 80, 12, GRAY);

        // Draw modal
        Modal_Draw(&modal);

        EndDrawing();
    }

    // Cleanup
    Button_Destroy(&primary_btn);
    Button_Destroy(&secondary_btn);
    Button_Destroy(&destructive_btn);
    Button_Destroy(&open_modal_btn);
    Button_Destroy(&nav_home);
    Button_Destroy(&nav_settings);
    Button_Destroy(&nav_help);
    Checkbox_Destroy(&checkbox);
    Dropdown_Destroy(&dropdown);
    RadioGroup_Destroy(radio_group);
    TextInput_Destroy(&text_input);
    TextArea_Destroy(&text_area);
    Modal_Destroy(&modal);
    Sidebar_Destroy(&sidebar);
    BlurEffect_Destroy();
    CloseWindow();

    return 0;
} 