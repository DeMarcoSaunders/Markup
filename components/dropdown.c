#include "dropdown.h"
#include "theme.h"
#include <string.h>
#include <stdlib.h>
#include <math.h>
#include <ctype.h>

// Helper function to calculate dropdown position
static Rectangle CalculateDropdownRect(const Dropdown* dropdown) {
    Rectangle rect = {
        dropdown->button.rect.x,
        dropdown->button.rect.y + dropdown->button.rect.height,
        dropdown->button.rect.width,
        fminf(dropdown->options.length * dropdown->option_height, dropdown->max_dropdown_height)
    };
    
    // Ensure dropdown doesn't go off screen
    float screen_height = GetScreenHeight();
    if (rect.y + rect.height > screen_height) {
        // Position above the button instead
        rect.y = dropdown->button.rect.y - rect.height;
    }
    
    return rect;
}

// Helper function to update search filtering
static void UpdateSearchFilter(Dropdown* dropdown) {
    vec_clear(&dropdown->filtered_options);
    
    if (!dropdown->search_enabled || strlen(dropdown->search_buffer) == 0) {
        // No search - show all options
        for (int i = 0; i < dropdown->options.length; i++) {
            vec_push(&dropdown->filtered_options, i);
        }
        return;
    }
    
    // Filter options based on search
    for (int i = 0; i < dropdown->options.length; i++) {
        DropdownOption* option = &dropdown->options.data[i];
        if (option->is_separator) continue;
        
        // Simple case-insensitive substring search
        char* option_text = option->text;
        char* search_text = dropdown->search_buffer;
        
        if (option_text && search_text) {
            // Convert to lowercase for comparison
            bool matches = false;
            int text_len = strlen(option_text);
            int search_len = strlen(search_text);
            
            for (int j = 0; j <= text_len - search_len; j++) {
                bool match = true;
                for (int k = 0; k < search_len; k++) {
                    if (tolower(option_text[j + k]) != tolower(search_text[k])) {
                        match = false;
                        break;
                    }
                }
                if (match) {
                    matches = true;
                    break;
                }
            }
            
            if (matches) {
                vec_push(&dropdown->filtered_options, i);
            }
        }
    }
}

Dropdown Dropdown_Create(Rectangle rect, const char* button_text, ButtonVariant variant) {
    Dropdown dropdown = {0};
    
    // Initialize base button
    dropdown.button = Button_Create(rect, button_text, variant);
    
    // Initialize dropdown-specific properties
    vec_init(&dropdown.options);
    vec_init(&dropdown.filtered_options);
    dropdown.selected_index = -1;
    dropdown.is_open = false;
    dropdown.show_arrow = true;
    
    // Visual properties
    dropdown.option_height = 30.0f;
    dropdown.max_dropdown_height = 200.0f;
    dropdown.scroll_offset = 0;
    dropdown.hovered_option = -1;
    
    // Animation properties
    dropdown.open_animation = 0.0f;
    dropdown.animation_speed = 8.0f;
    
    // Initialize custom style pointers
    dropdown.custom_dropdown_bg_color = NULL;
    dropdown.custom_option_hover_color = NULL;
    dropdown.custom_option_text_color = NULL;
    dropdown.custom_separator_color = NULL;
    dropdown.custom_dropdown_opacity = NULL;
    
    // Behavior flags
    dropdown.close_on_select = true;
    dropdown.allow_deselect = false;
    dropdown.search_enabled = false;
    
    // Search functionality
    memset(dropdown.search_buffer, 0, sizeof(dropdown.search_buffer));
    dropdown.search_timer = 0.0f;
    
    // Calculate initial dropdown rect
    dropdown.dropdown_rect = CalculateDropdownRect(&dropdown);
    
    return dropdown;
}

void Dropdown_Update(Dropdown* dropdown) {
    if (!dropdown) return;
    
    // Update base button
    Button_Update(&dropdown->button);
    
    // Handle button click to toggle dropdown
    if (dropdown->button.is_clicked) {
        Dropdown_Toggle(dropdown);
    }
    
    // Update animation
    float target = dropdown->is_open ? 1.0f : 0.0f;
    float delta = GetFrameTime() * dropdown->animation_speed;
    
    if (dropdown->open_animation < target) {
        dropdown->open_animation = fminf(dropdown->open_animation + delta, target);
    } else if (dropdown->open_animation > target) {
        dropdown->open_animation = fmaxf(dropdown->open_animation - delta, target);
    }
    
    // Update dropdown rect
    dropdown->dropdown_rect = CalculateDropdownRect(dropdown);
    
    if (dropdown->is_open && dropdown->open_animation > 0.1f) {
        Vector2 mouse_pos = GetMousePosition();
        
        // Handle search input
        if (dropdown->search_enabled) {
            // Update search timer
            dropdown->search_timer -= GetFrameTime();
            if (dropdown->search_timer <= 0.0f) {
                // Clear search buffer after timeout
                memset(dropdown->search_buffer, 0, sizeof(dropdown->search_buffer));
                UpdateSearchFilter(dropdown);
            }
            
            // Handle character input for search
            int key = GetCharPressed();
            if (key > 0 && key < 128) {
                int len = strlen(dropdown->search_buffer);
                if (len < sizeof(dropdown->search_buffer) - 1) {
                    dropdown->search_buffer[len] = (char)key;
                    dropdown->search_buffer[len + 1] = '\0';
                    dropdown->search_timer = 2.0f; // Reset timer
                    UpdateSearchFilter(dropdown);
                }
            }
            
            // Handle backspace
            if (IsKeyPressed(KEY_BACKSPACE)) {
                int len = strlen(dropdown->search_buffer);
                if (len > 0) {
                    dropdown->search_buffer[len - 1] = '\0';
                    dropdown->search_timer = 2.0f; // Reset timer
                    UpdateSearchFilter(dropdown);
                }
            }
        } else {
            // No search - show all options
            UpdateSearchFilter(dropdown);
        }
        
        // Check mouse hover over options
        dropdown->hovered_option = -1;
        if (CheckCollisionPointRec(mouse_pos, dropdown->dropdown_rect)) {
            float relative_y = mouse_pos.y - dropdown->dropdown_rect.y;
            int option_index = (int)(relative_y / dropdown->option_height) + dropdown->scroll_offset;
            
            if (option_index >= 0 && option_index < dropdown->filtered_options.length) {
                int actual_index = dropdown->filtered_options.data[option_index];
                DropdownOption* actual_option = &dropdown->options.data[actual_index];
                if (!actual_option->is_separator && !actual_option->is_disabled) {
                    dropdown->hovered_option = actual_index;
                }
            }
        }
        
        // Handle option selection
        if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
            if (dropdown->hovered_option >= 0) {
                if (dropdown->allow_deselect && dropdown->selected_index == dropdown->hovered_option) {
                    dropdown->selected_index = -1;
                } else {
                    dropdown->selected_index = dropdown->hovered_option;
                }
                
                if (dropdown->close_on_select) {
                    Dropdown_Close(dropdown);
                }
            } else if (!CheckCollisionPointRec(mouse_pos, dropdown->dropdown_rect) &&
                      !CheckCollisionPointRec(mouse_pos, dropdown->button.rect)) {
                // Click outside - close dropdown
                Dropdown_Close(dropdown);
            }
        }
        
        // Handle scrolling
        float wheel = GetMouseWheelMove();
        if (wheel != 0.0f && CheckCollisionPointRec(mouse_pos, dropdown->dropdown_rect)) {
            dropdown->scroll_offset -= (int)wheel;
            int max_scroll = dropdown->filtered_options.length - (int)(dropdown->dropdown_rect.height / dropdown->option_height);
            dropdown->scroll_offset = dropdown->scroll_offset < 0 ? 0 : 
                                    (dropdown->scroll_offset > max_scroll ? max_scroll : dropdown->scroll_offset);
        }
        
        // Handle keyboard navigation
        if (IsKeyPressed(KEY_UP)) {
            if (dropdown->hovered_option > 0) {
                dropdown->hovered_option--;
            }
        }
        if (IsKeyPressed(KEY_DOWN)) {
            if (dropdown->hovered_option < dropdown->options.length - 1) {
                dropdown->hovered_option++;
            }
        }
        if (IsKeyPressed(KEY_ENTER) && dropdown->hovered_option >= 0) {
            dropdown->selected_index = dropdown->hovered_option;
            if (dropdown->close_on_select) {
                Dropdown_Close(dropdown);
            }
        }
        if (IsKeyPressed(KEY_ESCAPE)) {
            Dropdown_Close(dropdown);
        }
    } else if (dropdown->is_open && IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
        // Close if clicked outside while opening
        Vector2 mouse_pos = GetMousePosition();
        if (!CheckCollisionPointRec(mouse_pos, dropdown->button.rect)) {
            Dropdown_Close(dropdown);
        }
    }
}

void Dropdown_Draw(const Dropdown* dropdown) {
    if (!dropdown) return;
    
    // Draw base button with arrow indicator
    Button_Draw(&dropdown->button);
    
    // Draw arrow indicator
    if (dropdown->show_arrow) {
        Rectangle arrow_area = {
            dropdown->button.rect.x + dropdown->button.rect.width - 30,
            dropdown->button.rect.y,
            30,
            dropdown->button.rect.height
        };
        
        Vector2 arrow_center = {
            arrow_area.x + arrow_area.width / 2,
            arrow_area.y + arrow_area.height / 2
        };
        
        float arrow_size = 6.0f;
        Color arrow_color = dropdown->button.custom_text_color ? 
                           *dropdown->button.custom_text_color : 
                           AppTheme.components[COMPONENT_BUTTON].colors[STATE_DEFAULT].text;
        
        if (dropdown->is_open) {
            // Up arrow
            DrawTriangle(
                (Vector2){arrow_center.x, arrow_center.y - arrow_size/2},
                (Vector2){arrow_center.x - arrow_size, arrow_center.y + arrow_size/2},
                (Vector2){arrow_center.x + arrow_size, arrow_center.y + arrow_size/2},
                arrow_color
            );
        } else {
            // Down arrow
            DrawTriangle(
                (Vector2){arrow_center.x, arrow_center.y + arrow_size/2},
                (Vector2){arrow_center.x - arrow_size, arrow_center.y - arrow_size/2},
                (Vector2){arrow_center.x + arrow_size, arrow_center.y - arrow_size/2},
                arrow_color
            );
        }
    }
    
    // Draw dropdown list if open
    if (dropdown->is_open && dropdown->open_animation > 0.0f) {
        Rectangle animated_rect = dropdown->dropdown_rect;
        animated_rect.height *= dropdown->open_animation;
        
        // Get colors from the theme
        Color bg_color = dropdown->custom_dropdown_bg_color ? 
                        *dropdown->custom_dropdown_bg_color : 
                        AppTheme.components[COMPONENT_PANEL].colors[STATE_DEFAULT].background;
        Color text_color = dropdown->custom_option_text_color ? 
                          *dropdown->custom_option_text_color : 
                          AppTheme.components[COMPONENT_PANEL].colors[STATE_DEFAULT].text;
        Color hover_color = dropdown->custom_option_hover_color ? 
                           *dropdown->custom_option_hover_color : 
                           AppTheme.components[COMPONENT_BUTTON].colors[STATE_HOVER].background;
        Color separator_color = dropdown->custom_separator_color ? 
                               *dropdown->custom_separator_color : 
                               AppTheme.components[COMPONENT_PANEL].colors[STATE_DEFAULT].border;
        
        float opacity = dropdown->custom_dropdown_opacity ? 
                       *dropdown->custom_dropdown_opacity : 1.0f;
        opacity *= dropdown->open_animation;
        
        bg_color.a = (unsigned char)(bg_color.a * opacity);
        text_color.a = (unsigned char)(text_color.a * opacity);
        hover_color.a = (unsigned char)(hover_color.a * opacity);
        separator_color.a = (unsigned char)(separator_color.a * opacity);
        
        // Draw dropdown background
        DrawRectangleRec(animated_rect, bg_color);
        DrawRectangleLinesEx(animated_rect, 1.0f, separator_color);
        
        // Draw options
        BeginScissorMode((int)animated_rect.x, (int)animated_rect.y, 
                        (int)animated_rect.width, (int)animated_rect.height);
        
        int visible_options = (int)(animated_rect.height / dropdown->option_height);
        for (int i = 0; i < visible_options && (i + dropdown->scroll_offset) < dropdown->filtered_options.length; i++) {
            int option_index = dropdown->filtered_options.data[i + dropdown->scroll_offset];
            if (option_index >= dropdown->options.length) continue;
            
            DropdownOption* option = &dropdown->options.data[option_index];
            Rectangle option_rect = {
                animated_rect.x,
                animated_rect.y + i * dropdown->option_height,
                animated_rect.width,
                dropdown->option_height
            };
            
            if (option->is_separator) {
                // Draw separator line
                float sep_y = option_rect.y + option_rect.height / 2;
                DrawLineEx(
                    (Vector2){option_rect.x + 10, sep_y},
                    (Vector2){option_rect.x + option_rect.width - 10, sep_y},
                    1.0f, separator_color
                );
            } else {
                // Draw option background
                Color option_bg = bg_color;
                if (option_index == dropdown->hovered_option) {
                    option_bg = hover_color;
                } else if (option_index == dropdown->selected_index) {
                    option_bg.r = (unsigned char)(option_bg.r * 0.9f);
                    option_bg.g = (unsigned char)(option_bg.g * 0.9f);
                    option_bg.b = (unsigned char)(option_bg.b * 0.9f);
                }
                
                if (option_index == dropdown->hovered_option || option_index == dropdown->selected_index) {
                    DrawRectangleRec(option_rect, option_bg);
                }
                
                // Draw option text
                if (option->text) {
                    Color final_text_color = text_color;
                    if (option->is_disabled) {
                        final_text_color.a = (unsigned char)(final_text_color.a * 0.5f);
                    }
                    
                    Vector2 text_pos = {
                        option_rect.x + 10,
                        option_rect.y + (option_rect.height - 20) / 2
                    };
                    
                    DrawTextEx(GetFontDefault(), option->text, text_pos, 20, 1.0f, final_text_color);
                }
                
                // Draw selection indicator
                if (option_index == dropdown->selected_index) {
                    DrawCircle((int)(option_rect.x + option_rect.width - 15), 
                              (int)(option_rect.y + option_rect.height / 2), 
                              3, text_color);
                }
            }
        }
        
        EndScissorMode();
        
        // Draw search indicator if enabled
        if (dropdown->search_enabled && strlen(dropdown->search_buffer) > 0) {
            Rectangle search_rect = {
                animated_rect.x, 
                animated_rect.y - 25, 
                animated_rect.width, 
                20
            };
            DrawRectangleRec(search_rect, bg_color);
            DrawRectangleLinesEx(search_rect, 1.0f, separator_color);
            
            char search_text[80];
            snprintf(search_text, sizeof(search_text), "Search: %s", dropdown->search_buffer);
            DrawTextEx(GetFontDefault(), search_text, 
                      (Vector2){search_rect.x + 5, search_rect.y + 2}, 
                      16, 1.0f, text_color);
        }
    }
}
void Dropdown_Destroy(Dropdown* dropdown) {
    if (!dropdown) return;
    
    // Destroy base button
    Button_Destroy(&dropdown->button);
    
    // Free option texts
    for (int i = 0; i < dropdown->options.length; i++) {
        if (dropdown->options.data[i].text) {
            free(dropdown->options.data[i].text);
            dropdown->options.data[i].text = NULL;
        }
    }
    vec_deinit(&dropdown->options);
    vec_deinit(&dropdown->filtered_options);
    
    // Free custom style allocations
    if (dropdown->custom_dropdown_bg_color) { 
        free(dropdown->custom_dropdown_bg_color); 
        dropdown->custom_dropdown_bg_color = NULL; 
    }
    if (dropdown->custom_option_hover_color) { 
        free(dropdown->custom_option_hover_color); 
        dropdown->custom_option_hover_color = NULL; 
    }
    if (dropdown->custom_option_text_color) { 
        free(dropdown->custom_option_text_color); 
        dropdown->custom_option_text_color = NULL; 
    }
    if (dropdown->custom_separator_color) { 
        free(dropdown->custom_separator_color); 
        dropdown->custom_separator_color = NULL; 
    }
    if (dropdown->custom_dropdown_opacity) { 
        free(dropdown->custom_dropdown_opacity); 
        dropdown->custom_dropdown_opacity = NULL; 
    }
}

// Option Management
int Dropdown_AddOption(Dropdown* dropdown, const char* text) {
    if (!dropdown) return -1;

    DropdownOption option = {0};
    if (text) {
        size_t text_len = strlen(text);
        option.text = (char*)malloc(text_len + 1);
        if (option.text) {
            strcpy(option.text, text);
        }
    }
    vec_push(&dropdown->options, option);
    UpdateSearchFilter(dropdown);
    return dropdown->options.length - 1;
}

int Dropdown_AddSeparator(Dropdown* dropdown) {
    if (!dropdown) return -1;

    DropdownOption option = {.is_separator = true};
    vec_push(&dropdown->options, option);
    return dropdown->options.length - 1;
}

void Dropdown_RemoveOption(Dropdown* dropdown, int index) {
    if (!dropdown || index < 0 || index >= dropdown->options.length) {
        return;
    }
    
    // Free text if allocated
    if (dropdown->options.data[index].text) {
        free(dropdown->options.data[index].text);
    }
    
    vec_splice(&dropdown->options, index, 1);
    
    // Adjust selected index if necessary
    if (dropdown->selected_index == index) {
        dropdown->selected_index = -1;
    } else if (dropdown->selected_index > index) {
        dropdown->selected_index--;
    }
    
    UpdateSearchFilter(dropdown);
}

void Dropdown_ClearOptions(Dropdown* dropdown) {
    if (!dropdown) return;
    
    // Free all option texts
    for (int i = 0; i < dropdown->options.length; i++) {
        if (dropdown->options.data[i].text) {
            free(dropdown->options.data[i].text);
        }
    }
    vec_clear(&dropdown->options);
    
    dropdown->selected_index = -1;
    vec_clear(&dropdown->filtered_options);
}

void Dropdown_SetOptionDisabled(Dropdown* dropdown, int index, bool disabled) {
    if (!dropdown || index < 0 || index >= dropdown->options.length) {
        return;
    }
    
    dropdown->options.data[index].is_disabled = disabled;
}

void Dropdown_SetOptionUserData(Dropdown* dropdown, int index, void* user_data) {
    if (!dropdown || index < 0 || index >= dropdown->options.length) {
        return;
    }
    
    dropdown->options.data[index].user_data = user_data;
}

// Selection Management
void Dropdown_SetSelected(Dropdown* dropdown, int index) {
    if (!dropdown) return;
    
    if (index < -1 || index >= dropdown->options.length) {
        return;
    }
    
    if (index >= 0) {
        DropdownOption* option = &dropdown->options.data[index];
        if (option->is_disabled || option->is_separator) {
            return;
        }
    }
    
    dropdown->selected_index = index;
}

int Dropdown_GetSelected(const Dropdown* dropdown) {
    return dropdown ? dropdown->selected_index : -1;
}

const char* Dropdown_GetSelectedText(const Dropdown* dropdown) {
    if (!dropdown || dropdown->selected_index < 0 || dropdown->selected_index >= dropdown->options.length) {
        return NULL;
    }
    
    return dropdown->options.data[dropdown->selected_index].text;
}

void* Dropdown_GetSelectedUserData(const Dropdown* dropdown) {
    if (!dropdown || dropdown->selected_index < 0 || dropdown->selected_index >= dropdown->options.length) {
        return NULL;
    }
    
    return dropdown->options.data[dropdown->selected_index].user_data;
}

bool Dropdown_HasSelection(const Dropdown* dropdown) {
    return dropdown && dropdown->selected_index >= 0;
}

// State Management
void Dropdown_Open(Dropdown* dropdown) {
    if (dropdown) {
        dropdown->is_open = true;
        dropdown->hovered_option = -1;
        dropdown->scroll_offset = 0;
        UpdateSearchFilter(dropdown);
    }
}

void Dropdown_Close(Dropdown* dropdown) {
    if (dropdown) {
        dropdown->is_open = false;
        dropdown->hovered_option = -1;
        memset(dropdown->search_buffer, 0, sizeof(dropdown->search_buffer));
        dropdown->search_timer = 0.0f;
    }
}

void Dropdown_Toggle(Dropdown* dropdown) {
    if (!dropdown) return;
    
    if (dropdown->is_open) {
        Dropdown_Close(dropdown);
    } else {
        Dropdown_Open(dropdown);
    }
}

bool Dropdown_IsOpen(const Dropdown* dropdown) {
    return dropdown && dropdown->is_open;
}

// Visual Customization
void Dropdown_SetOptionHeight(Dropdown* dropdown, float height) {
    if (dropdown && height > 0) {
        dropdown->option_height = height;
    }
}

void Dropdown_SetMaxHeight(Dropdown* dropdown, float max_height) {
    if (dropdown && max_height > 0) {
        dropdown->max_dropdown_height = max_height;
    }
}

void Dropdown_SetAnimationSpeed(Dropdown* dropdown, float speed) {
    if (dropdown && speed > 0) {
        dropdown->animation_speed = speed;
    }
}

void Dropdown_SetShowArrow(Dropdown* dropdown, bool show_arrow) {
    if (dropdown) {
        dropdown->show_arrow = show_arrow;
    }
}

void Dropdown_SetDropdownBackgroundColor(Dropdown* dropdown, Color color) {
    if (!dropdown) return;
    
    if (!dropdown->custom_dropdown_bg_color) {
        dropdown->custom_dropdown_bg_color = (Color*)malloc(sizeof(Color));
    }
    
    if (dropdown->custom_dropdown_bg_color) {
        *dropdown->custom_dropdown_bg_color = color;
    }
}

void Dropdown_SetOptionHoverColor(Dropdown* dropdown, Color color) {
    if (!dropdown) return;
    
    if (!dropdown->custom_option_hover_color) {
        dropdown->custom_option_hover_color = (Color*)malloc(sizeof(Color));
    }
    
    if (dropdown->custom_option_hover_color) {
        *dropdown->custom_option_hover_color = color;
    }
}

void Dropdown_SetOptionTextColor(Dropdown* dropdown, Color color) {
    if (!dropdown) return;
    
    if (!dropdown->custom_option_text_color) {
        dropdown->custom_option_text_color = (Color*)malloc(sizeof(Color));
    }
    
    if (dropdown->custom_option_text_color) {
        *dropdown->custom_option_text_color = color;
    }
}

void Dropdown_SetSeparatorColor(Dropdown* dropdown, Color color) {
    if (!dropdown) return;
    
    if (!dropdown->custom_separator_color) {
        dropdown->custom_separator_color = (Color*)malloc(sizeof(Color));
    }
    
    if (dropdown->custom_separator_color) {
        *dropdown->custom_separator_color = color;
    }
}

void Dropdown_SetDropdownOpacity(Dropdown* dropdown, float opacity) {
    if (!dropdown) return;
    
    if (!dropdown->custom_dropdown_opacity) {
        dropdown->custom_dropdown_opacity = (float*)malloc(sizeof(float));
    }
    
    if (dropdown->custom_dropdown_opacity) {
        *dropdown->custom_dropdown_opacity = opacity < 0.0f ? 0.0f : (opacity > 1.0f ? 1.0f : opacity);
    }
}

// Behavior Configuration
void Dropdown_SetCloseOnSelect(Dropdown* dropdown, bool close_on_select) {
    if (dropdown) {
        dropdown->close_on_select = close_on_select;
    }
}

void Dropdown_SetAllowDeselect(Dropdown* dropdown, bool allow_deselect) {
    if (dropdown) {
        dropdown->allow_deselect = allow_deselect;
    }
}

void Dropdown_SetSearchEnabled(Dropdown* dropdown, bool search_enabled) {
    if (dropdown) {
        dropdown->search_enabled = search_enabled;
        if (!search_enabled) {
            memset(dropdown->search_buffer, 0, sizeof(dropdown->search_buffer));
            dropdown->search_timer = 0.0f;
            UpdateSearchFilter(dropdown);
        }
    }
}

// Utility Functions
int Dropdown_FindOptionByText(const Dropdown* dropdown, const char* text) {
    if (!dropdown || !text) return -1;
    
    for (int i = 0; i < dropdown->options.length; i++) {
        if (dropdown->options.data[i].text && strcmp(dropdown->options.data[i].text, text) == 0) {
            return i;
        }
    }
    
    return -1;
}

void Dropdown_ScrollToOption(Dropdown* dropdown, int index) {
    if (!dropdown || index < 0 || index >= dropdown->options.length) {
        return;
    }
    
    int visible_options = (int)(dropdown->dropdown_rect.height / dropdown->option_height);
    
    if (index < dropdown->scroll_offset) {
        dropdown->scroll_offset = index;
    } else if (index >= dropdown->scroll_offset + visible_options) {
        dropdown->scroll_offset = index - visible_options + 1;
    }
    
    // Clamp scroll offset
    int max_scroll = dropdown->options.length - visible_options;
    dropdown->scroll_offset = dropdown->scroll_offset < 0 ? 0 : 
                             (dropdown->scroll_offset > max_scroll ? max_scroll : dropdown->scroll_offset);
}

Rectangle Dropdown_GetOptionRect(const Dropdown* dropdown, int option_index) {
    Rectangle rect = {0};
    
    if (!dropdown || option_index < 0 || option_index >= dropdown->options.length) {
        return rect;
    }
    
    rect.x = dropdown->dropdown_rect.x;
    rect.y = dropdown->dropdown_rect.y + (option_index - dropdown->scroll_offset) * dropdown->option_height;
    rect.width = dropdown->dropdown_rect.width;
    rect.height = dropdown->option_height;
    
    return rect;
}

// Reset Functions
void Dropdown_ResetCustomStyles(Dropdown* dropdown) {
    if (!dropdown) return;
    
    // Reset button styles
    Button_ResetAllCustomStyles(&dropdown->button);
    
    // Free dropdown-specific custom styles
    if (dropdown->custom_dropdown_bg_color) { 
        free(dropdown->custom_dropdown_bg_color); 
        dropdown->custom_dropdown_bg_color = NULL; 
    }
    if (dropdown->custom_option_hover_color) { 
        free(dropdown->custom_option_hover_color); 
        dropdown->custom_option_hover_color = NULL; 
    }
    if (dropdown->custom_option_text_color) { 
        free(dropdown->custom_option_text_color); 
        dropdown->custom_option_text_color = NULL; 
    }
    if (dropdown->custom_separator_color) { 
        free(dropdown->custom_separator_color); 
        dropdown->custom_separator_color = NULL; 
    }
    if (dropdown->custom_dropdown_opacity) { 
        free(dropdown->custom_dropdown_opacity); 
        dropdown->custom_dropdown_opacity = NULL; 
    }
    
    // Reset to default values
    dropdown->option_height = 30.0f;
    dropdown->max_dropdown_height = 200.0f;
    dropdown->animation_speed = 8.0f;
    dropdown->show_arrow = true;
    dropdown->close_on_select = true;
    dropdown->allow_deselect = false;
    dropdown->search_enabled = false;
}