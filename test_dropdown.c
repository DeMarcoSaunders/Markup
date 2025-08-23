#include "raylib.h"
#include "components/dropdown.h"
#include "theme.h"

int main(void) {
    // Initialize window
    const int screenWidth = 800;
    const int screenHeight = 600;
    InitWindow(screenWidth, screenHeight, "Dropdown Menu Test");
    SetTargetFPS(60);
    
    // Initialize theme
    Theme_Init();
    
    // Create dropdown menus
    Dropdown basic_dropdown = Dropdown_Create(
        (Rectangle){50, 50, 200, 40}, 
        "Select Option", 
        BUTTON_PRIMARY
    );
    
    Dropdown searchable_dropdown = Dropdown_Create(
        (Rectangle){300, 50, 200, 40}, 
        "Search & Select", 
        BUTTON_SECONDARY
    );
    
    Dropdown custom_dropdown = Dropdown_Create(
        (Rectangle){550, 50, 180, 40}, 
        "Custom Style", 
        BUTTON_DESTRUCTIVE
    );
    
    // Add options to basic dropdown
    Dropdown_AddOption(&basic_dropdown, "Option 1");
    Dropdown_AddOption(&basic_dropdown, "Option 2");
    Dropdown_AddOption(&basic_dropdown, "Option 3");
    Dropdown_AddSeparator(&basic_dropdown);
    Dropdown_AddOption(&basic_dropdown, "Option 4");
    Dropdown_AddOption(&basic_dropdown, "Disabled Option");
    Dropdown_SetOptionDisabled(&basic_dropdown, 4, true);
    
    // Add options to searchable dropdown
    Dropdown_SetSearchEnabled(&searchable_dropdown, true);
    Dropdown_AddOption(&searchable_dropdown, "Apple");
    Dropdown_AddOption(&searchable_dropdown, "Banana");
    Dropdown_AddOption(&searchable_dropdown, "Cherry");
    Dropdown_AddOption(&searchable_dropdown, "Date");
    Dropdown_AddOption(&searchable_dropdown, "Elderberry");
    Dropdown_AddOption(&searchable_dropdown, "Fig");
    Dropdown_AddOption(&searchable_dropdown, "Grape");
    Dropdown_AddOption(&searchable_dropdown, "Honeydew");
    
    // Add options to custom dropdown
    Dropdown_AddOption(&custom_dropdown, "Red");
    Dropdown_AddOption(&custom_dropdown, "Green");
    Dropdown_AddOption(&custom_dropdown, "Blue");
    Dropdown_AddOption(&custom_dropdown, "Yellow");
    Dropdown_AddOption(&custom_dropdown, "Purple");
    
    // Customize the third dropdown
    Dropdown_SetDropdownBackgroundColor(&custom_dropdown, (Color){40, 40, 60, 255});
    Dropdown_SetOptionHoverColor(&custom_dropdown, (Color){80, 80, 120, 255});
    Dropdown_SetOptionTextColor(&custom_dropdown, (Color){220, 220, 255, 255});
    Dropdown_SetOptionHeight(&custom_dropdown, 35.0f);
    Dropdown_SetAnimationSpeed(&custom_dropdown, 12.0f);
    Dropdown_SetAllowDeselect(&custom_dropdown, true);
    
    // Main game loop
    while (!WindowShouldClose()) {
        // Update
        Dropdown_Update(&basic_dropdown);
        Dropdown_Update(&searchable_dropdown);
        Dropdown_Update(&custom_dropdown);
        
        // Draw
        BeginDrawing();
        ClearBackground(AppTheme.components[COMPONENT_PANEL].colors[STATE_DEFAULT].background);
        
        // Draw title
        DrawText("Dropdown Menu Components", 20, 10, 24, AppTheme.text_primary_color);
        
        // Draw dropdowns
        Dropdown_Draw(&basic_dropdown);
        Dropdown_Draw(&searchable_dropdown);
        Dropdown_Draw(&custom_dropdown);
        
        // Draw labels
        DrawText("Basic Dropdown", 50, 100, 16, AppTheme.text_secondary_color);
        DrawText("Searchable Dropdown", 300, 100, 16, AppTheme.text_secondary_color);
        DrawText("Custom Styled", 550, 100, 16, AppTheme.text_secondary_color);
        
        // Draw selection info
        int y_offset = 150;
        
        // Basic dropdown selection
        const char* basic_selected = Dropdown_GetSelectedText(&basic_dropdown);
        if (basic_selected) {
            DrawText(TextFormat("Basic Selected: %s", basic_selected), 50, y_offset, 14, AppTheme.text_primary_color);
        } else {
            DrawText("Basic Selected: None", 50, y_offset, 14, AppTheme.text_secondary_color);
        }
        
        // Searchable dropdown selection
        const char* search_selected = Dropdown_GetSelectedText(&searchable_dropdown);
        if (search_selected) {
            DrawText(TextFormat("Search Selected: %s", search_selected), 50, y_offset + 25, 14, AppTheme.text_primary_color);
        } else {
            DrawText("Search Selected: None", 50, y_offset + 25, 14, AppTheme.text_secondary_color);
        }
        
        // Custom dropdown selection
        const char* custom_selected = Dropdown_GetSelectedText(&custom_dropdown);
        if (custom_selected) {
            DrawText(TextFormat("Custom Selected: %s", custom_selected), 50, y_offset + 50, 14, AppTheme.text_primary_color);
        } else {
            DrawText("Custom Selected: None", 50, y_offset + 50, 14, AppTheme.text_secondary_color);
        }
        
        // Draw instructions
        DrawText("Instructions:", 50, y_offset + 100, 16, AppTheme.text_primary_color);
        DrawText("• Click dropdown buttons to open/close", 50, y_offset + 125, 12, AppTheme.text_secondary_color);
        DrawText("• Use mouse wheel to scroll through options", 50, y_offset + 140, 12, AppTheme.text_secondary_color);
        DrawText("• Type to search in the searchable dropdown", 50, y_offset + 155, 12, AppTheme.text_secondary_color);
        DrawText("• Use arrow keys and Enter for keyboard navigation", 50, y_offset + 170, 12, AppTheme.text_secondary_color);
        DrawText("• Press Escape to close dropdown", 50, y_offset + 185, 12, AppTheme.text_secondary_color);
        DrawText("• Custom dropdown allows deselection", 50, y_offset + 200, 12, AppTheme.text_secondary_color);
        
        EndDrawing();
    }
    
    // Cleanup
    Dropdown_Destroy(&basic_dropdown);
    Dropdown_Destroy(&searchable_dropdown);
    Dropdown_Destroy(&custom_dropdown);
    
    CloseWindow();
    return 0;
}