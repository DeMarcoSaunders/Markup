# Components Reference

Complete API reference for all UI components in the library.

## Button Component

Interactive button with multiple variants and states.

### Creation and Destruction

```c
Button Button_Create(Rectangle rect, const char* text, ButtonVariant variant);
void Button_Destroy(Button* button);
```

**Parameters:**
- `rect`: Position and size of the button
- `text`: Button label text (copied internally)
- `variant`: `BUTTON_PRIMARY`, `BUTTON_SECONDARY`, or `BUTTON_DESTRUCTIVE`

### Update and Drawing

```c
void Button_Update(Button* button);
void Button_Draw(const Button* button);
```

Call `Button_Update()` in your main loop to handle input, then `Button_Draw()` to render.

### State Properties

```c
// Read-only state (check after Button_Update)
bool is_hovered;     // Mouse is over button
bool is_pressed;     // Mouse button is down on button
bool is_clicked;     // Button was clicked this frame
bool is_focused;     // Button has keyboard focus
bool is_disabled;    // Button is disabled
bool is_visible;     // Button is visible
```

### Visual State Control

```c
void Button_SetBorder(Button* button, bool show_border);
void Button_SetOutline(Button* button, bool show_outline);
void Button_SetShadow(Button* button, bool show_shadow);
void Button_SetVisible(Button* button, bool is_visible);
void Button_SetDisabled(Button* button, bool is_disabled);
```

### Styling Functions

```c
// Layout
void Button_SetPadding(Button* button, float top, float right, float bottom, float left);
void Button_SetMargin(Button* button, float top, float right, float bottom, float left);
void Button_SetBorderRadius(Button* button, float top_left, float top_right, float bottom_right, float bottom_left);

// Colors
void Button_SetBackgroundColor(Button* button, Color color);
void Button_SetTextColor(Button* button, Color color);
void Button_SetBorderColor(Button* button, Color color);

// Typography
void Button_SetFontSize(Button* button, int font_size);
void Button_SetTextAlign(Button* button, TextAlign horizontal, TextVerticalAlign vertical);

// Effects
void Button_SetOpacity(Button* button, float opacity);
void Button_SetShadowStyle(Button* button, Vector2 offset, float blur, float spread, Color color);
void Button_SetTransform(Button* button, Vector2 offset, float rotation, Vector2 scale);
void Button_SetZIndex(Button* button, int z_index);
```

### Reset Functions

```c
void Button_ResetPadding(Button* button);
void Button_ResetMargin(Button* button);
void Button_ResetAllCustomStyles(Button* button);
```

### Example Usage

```c
// Create button
Button save_btn = Button_Create((Rectangle){100, 100, 120, 40}, "Save", BUTTON_PRIMARY);

// Customize appearance
Button_SetShadow(&save_btn, true);
Button_SetBorderRadius(&save_btn, 8, 8, 8, 8);
Button_SetPadding(&save_btn, 12, 20, 12, 20);

// Main loop
while (!WindowShouldClose()) {
    Button_Update(&save_btn);
    
    if (save_btn.is_clicked) {
        SaveFile();
    }
    
    BeginDrawing();
    Button_Draw(&save_btn);
    EndDrawing();
}

Button_Destroy(&save_btn);
```

## Panel Component

Container with flexbox layout capabilities.

### Creation and Destruction

```c
Panel Panel_Create(Rectangle bounds);
void Panel_Destroy(Panel* panel);
```

### Child Management

```c
void Panel_AddButton(Panel* panel, Button* button);
void Panel_AddPanel(Panel* panel, Panel* child_panel);
void Panel_AddSlider(Panel* panel, Slider* slider);
void Panel_AddSidebar(Panel* panel, Sidebar* sidebar);

void Panel_RemoveChild(Panel* panel, int index);
void Panel_ClearChildren(Panel* panel);
int Panel_GetChildCount(Panel* panel);
```

### Layout Properties

```c
void Panel_SetFlexDirection(Panel* panel, FlexDirection direction);
void Panel_SetJustifyContent(Panel* panel, JustifyContent justify);
void Panel_SetAlignItems(Panel* panel, AlignItems align);
void Panel_SetGap(Panel* panel, float gap);

// Per-child properties
void Panel_SetChildFlexGrow(Panel* panel, int child_index, float grow);
void Panel_SetChildFlexShrink(Panel* panel, int child_index, float shrink);
void Panel_SetChildAlignSelf(Panel* panel, int child_index, AlignSelf align);
```

**Flex Direction:**
- `FLEX_ROW`: Horizontal layout (default)
- `FLEX_COLUMN`: Vertical layout
- `FLEX_ROW_REVERSE`: Horizontal, reversed
- `FLEX_COLUMN_REVERSE`: Vertical, reversed

**Justify Content:**
- `JUSTIFY_FLEX_START`: Pack to start
- `JUSTIFY_FLEX_END`: Pack to end
- `JUSTIFY_CENTER`: Center items
- `JUSTIFY_SPACE_BETWEEN`: Space between items
- `JUSTIFY_SPACE_AROUND`: Space around items
- `JUSTIFY_SPACE_EVENLY`: Even spacing

**Align Items:**
- `ALIGN_FLEX_START`: Align to start
- `ALIGN_FLEX_END`: Align to end
- `ALIGN_CENTER`: Center align
- `ALIGN_STRETCH`: Stretch to fill

### Update and Drawing

```c
void Panel_Update(Panel* panel);
void Panel_Draw(Panel* panel);
void Panel_RecalculateLayout(Panel* panel);
```

### Example Usage

```c
Panel main_panel = Panel_Create((Rectangle){0, 0, 800, 600});

// Set layout
Panel_SetFlexDirection(&main_panel, FLEX_COLUMN);
Panel_SetJustifyContent(&main_panel, JUSTIFY_CENTER);
Panel_SetAlignItems(&main_panel, ALIGN_CENTER);
Panel_SetGap(&main_panel, 20);

// Add children
Button btn1 = Button_Create((Rectangle){0, 0, 200, 50}, "Button 1", BUTTON_PRIMARY);
Button btn2 = Button_Create((Rectangle){0, 0, 200, 50}, "Button 2", BUTTON_SECONDARY);

Panel_AddButton(&main_panel, &btn1);
Panel_AddButton(&main_panel, &btn2);

// Main loop
while (!WindowShouldClose()) {
    Panel_Update(&main_panel);
    
    BeginDrawing();
    Panel_Draw(&main_panel);
    EndDrawing();
}
```

## Modal Component

Overlay dialogs with blur backdrop and animations.

### Creation and Destruction

```c
Modal Modal_Create(const char* title, ModalSize size);
Modal Modal_CreateCustom(const char* title, float width, float height);
void Modal_Destroy(Modal* modal);
```

**Modal Sizes:**
- `MODAL_SIZE_AUTO`: Automatic sizing
- `MODAL_SIZE_SMALL`: 400x300 (or 60% of screen)
- `MODAL_SIZE_MEDIUM`: 600x450 (or 70% of screen)
- `MODAL_SIZE_LARGE`: 800x600 (or 80% of screen)
- `MODAL_SIZE_CUSTOM`: Custom dimensions

### Visibility Control

```c
void Modal_Show(Modal* modal);
void Modal_Hide(Modal* modal);
void Modal_Close(Modal* modal);  // Same as Hide
bool Modal_IsVisible(Modal* modal);
```

### Content Management

```c
void Modal_SetContentCallback(Modal* modal, void (*callback)(Rectangle, void*), void* user_data);
Rectangle Modal_GetContentArea(Modal* modal);
```

### Configuration

```c
void Modal_SetTitle(Modal* modal, const char* title);
void Modal_SetSize(Modal* modal, ModalSize size);
void Modal_SetCustomSize(Modal* modal, float width, float height);
void Modal_SetCloseOnOverlayClick(Modal* modal, bool enabled);
void Modal_SetShowCloseButton(Modal* modal, bool show);
void Modal_SetBlurEffect(Modal* modal, bool enabled);
```

### Update and Drawing

```c
void Modal_Update(Modal* modal);
void Modal_Draw(Modal* modal);
void Modal_DrawWithBackground(Modal* modal, void (*draw_background)(void));
```

### Example Usage

```c
Modal settings_modal = Modal_Create("Settings", MODAL_SIZE_MEDIUM);

// Set up content
void draw_settings_content(Rectangle area, void* data) {
    DrawText("Settings go here", area.x + 20, area.y + 20, 20, WHITE);
    
    // Draw settings controls...
}
Modal_SetContentCallback(&settings_modal, draw_settings_content, NULL);

// Show modal
if (IsKeyPressed(KEY_F1)) {
    Modal_Show(&settings_modal);
}

// Main loop
void draw_main_app() {
    // Draw your main application
    DrawText("Main App", 100, 100, 20, WHITE);
}

while (!WindowShouldClose()) {
    Modal_Update(&settings_modal);
    
    BeginDrawing();
    ClearBackground(DARKGRAY);
    
    if (Modal_IsVisible(&settings_modal)) {
        Modal_DrawWithBackground(&settings_modal, draw_main_app);
    } else {
        draw_main_app();
    }
    
    EndDrawing();
}

Modal_Destroy(&settings_modal);
```

## Slider Component

Horizontal and vertical value input controls.

### Creation

```c
Slider Slider_Create(Rectangle bounds, float min_val, float max_val, float initial_val);
Slider Slider_CreateVertical(Rectangle bounds, float min_val, float max_val, float initial_val);
```

### Value Management

```c
void Slider_SetValue(Slider* slider, float value);
float Slider_GetValue(Slider* slider);
void Slider_SetRange(Slider* slider, float min_val, float max_val);
void Slider_SetStepSize(Slider* slider, float step);
```

### Configuration

```c
void Slider_SetShowValue(Slider* slider, bool show);
void Slider_SetDisabled(Slider* slider, bool disabled);
```

### Update and Drawing

```c
void Slider_Update(Slider* slider);
void Slider_Draw(Slider* slider);
```

### State Properties

```c
// Read-only state
bool is_hovered;     // Mouse over slider
bool is_dragging;    // Currently being dragged
bool is_disabled;    // Slider is disabled
float value;         // Current value
char value_text[32]; // Formatted value string
```

### Example Usage

```c
// Volume slider
Slider volume = Slider_Create((Rectangle){100, 100, 200, 30}, 0.0f, 100.0f, 50.0f);
Slider_SetStepSize(&volume, 1.0f);
Slider_SetShowValue(&volume, true);

// Vertical brightness slider
Slider brightness = Slider_CreateVertical((Rectangle){50, 100, 30, 200}, 0.0f, 1.0f, 0.8f);
Slider_SetStepSize(&brightness, 0.01f);

// Main loop
while (!WindowShouldClose()) {
    Slider_Update(&volume);
    Slider_Update(&brightness);
    
    // Use values
    float vol = Slider_GetValue(&volume);
    float bright = Slider_GetValue(&brightness);
    SetMasterVolume(vol / 100.0f);
    SetScreenBrightness(bright);
    
    BeginDrawing();
    Slider_Draw(&volume);
    Slider_Draw(&brightness);
    EndDrawing();
}
```

## Sidebar Component

Navigation panels with collapsible functionality.

### Creation and Destruction

```c
Sidebar Sidebar_Create(Rectangle bounds, SidebarPosition position);
void Sidebar_Destroy(Sidebar* sidebar);
```

**Sidebar Positions:**
- `SIDEBAR_LEFT`: Left side of screen
- `SIDEBAR_RIGHT`: Right side of screen
- `SIDEBAR_TOP`: Top of screen
- `SIDEBAR_BOTTOM`: Bottom of screen

### Child Management

```c
void Sidebar_AddButton(Sidebar* sidebar, Button* button);
void Sidebar_AddPanel(Sidebar* sidebar, Panel* panel);
void Sidebar_AddSlider(Sidebar* sidebar, Slider* slider);
void Sidebar_AddCustom(Sidebar* sidebar, void* component, ComponentType type);

void Sidebar_RemoveChild(Sidebar* sidebar, int index);
void Sidebar_ClearChildren(Sidebar* sidebar);
```

### State Management

```c
void Sidebar_SetState(Sidebar* sidebar, SidebarState state);
void Sidebar_Toggle(Sidebar* sidebar);
SidebarState Sidebar_GetState(Sidebar* sidebar);
```

**Sidebar States:**
- `SIDEBAR_EXPANDED`: Fully visible
- `SIDEBAR_COLLAPSED`: Minimized
- `SIDEBAR_HIDDEN`: Not visible
- `SIDEBAR_TRANSITIONING`: Animating between states

### Layout Properties

```c
void Sidebar_SetFlexDirection(Sidebar* sidebar, FlexDirection direction);
void Sidebar_SetJustifyContent(Sidebar* sidebar, JustifyContent justify);
void Sidebar_SetAlignItems(Sidebar* sidebar, AlignItems align);
void Sidebar_SetGap(Sidebar* sidebar, float gap);
```

### Update and Drawing

```c
void Sidebar_Update(Sidebar* sidebar);
void Sidebar_Draw(Sidebar* sidebar);
void Sidebar_RecalculateLayout(Sidebar* sidebar);
```

### Example Usage

```c
Sidebar nav_sidebar = Sidebar_Create((Rectangle){0, 0, 240, 600}, SIDEBAR_LEFT);

// Add navigation items
Button home_btn = Button_Create((Rectangle){0, 0, 200, 40}, "Home", BUTTON_SECONDARY);
Button settings_btn = Button_Create((Rectangle){0, 0, 200, 40}, "Settings", BUTTON_SECONDARY);
Button about_btn = Button_Create((Rectangle){0, 0, 200, 40}, "About", BUTTON_SECONDARY);

Sidebar_AddButton(&nav_sidebar, &home_btn);
Sidebar_AddButton(&nav_sidebar, &settings_btn);
Sidebar_AddButton(&nav_sidebar, &about_btn);

// Set layout
Sidebar_SetFlexDirection(&nav_sidebar, FLEX_COLUMN);
Sidebar_SetJustifyContent(&nav_sidebar, JUSTIFY_FLEX_START);
Sidebar_SetGap(&nav_sidebar, 10);

// Toggle with key
if (IsKeyPressed(KEY_TAB)) {
    Sidebar_Toggle(&nav_sidebar);
}

// Main loop
while (!WindowShouldClose()) {
    Sidebar_Update(&nav_sidebar);
    
    BeginDrawing();
    Sidebar_Draw(&nav_sidebar);
    EndDrawing();
}

Sidebar_Destroy(&nav_sidebar);
```

## Common Patterns

### Form Layout

```c
Panel form = Panel_Create((Rectangle){100, 100, 400, 300});
Panel_SetFlexDirection(&form, FLEX_COLUMN);
Panel_SetGap(&form, 15);

// Name field
Panel name_row = Panel_Create((Rectangle){0, 0, 400, 40});
Panel_SetFlexDirection(&name_row, FLEX_ROW);
Panel_SetAlignItems(&name_row, ALIGN_CENTER);
Panel_SetGap(&name_row, 10);

// Add label and input...
Panel_AddPanel(&form, &name_row);

// Submit button
Button submit = Button_Create((Rectangle){0, 0, 100, 40}, "Submit", BUTTON_PRIMARY);
Panel_AddButton(&form, &submit);
```

### Settings Panel

```c
Panel settings = Panel_Create((Rectangle){50, 50, 300, 400});
Panel_SetFlexDirection(&settings, FLEX_COLUMN);
Panel_SetGap(&settings, 20);

// Volume setting
Slider volume = Slider_Create((Rectangle){0, 0, 250, 30}, 0, 100, 75);
Panel_AddSlider(&settings, &volume);

// Fullscreen toggle
Button fullscreen = Button_Create((Rectangle){0, 0, 250, 40}, "Toggle Fullscreen", BUTTON_SECONDARY);
Panel_AddButton(&settings, &fullscreen);
```

### Confirmation Dialog

```c
Modal confirm = Modal_Create("Confirm Action", MODAL_SIZE_SMALL);

void draw_confirm_content(Rectangle area, void* data) {
    DrawText("Are you sure you want to delete this item?", 
             area.x + 20, area.y + 20, 16, WHITE);
    
    // Yes/No buttons
    if (GuiButton((Rectangle){area.x + 20, area.y + 80, 80, 30}, "Yes")) {
        // Perform action
        Modal_Hide((Modal*)data);
    }
    if (GuiButton((Rectangle){area.x + 120, area.y + 80, 80, 30}, "No")) {
        Modal_Hide((Modal*)data);
    }
}

Modal_SetContentCallback(&confirm, draw_confirm_content, &confirm);
```

## ImageBox Component

Display images and icons with various scaling modes, borders, and effects.

### Creation and Destruction

```c
ImageBox ImageBox_Create(Rectangle bounds);
ImageBox ImageBox_CreateFromFile(Rectangle bounds, const char* image_path);
ImageBox ImageBox_CreateFromTexture(Rectangle bounds, Texture2D texture, bool take_ownership);
void ImageBox_Destroy(ImageBox* imagebox);
```

**Parameters:**
- `bounds`: Position and size of the ImageBox
- `image_path`: Path to image file to load
- `texture`: Raylib Texture2D to display
- `take_ownership`: Whether ImageBox should unload the texture when destroyed

### Image Management

```c
void ImageBox_SetTexture(ImageBox* imagebox, Texture2D texture, bool take_ownership);
void ImageBox_LoadFromFile(ImageBox* imagebox, const char* image_path);
void ImageBox_ClearTexture(ImageBox* imagebox);
```

### Scaling and Alignment

```c
void ImageBox_SetScaleMode(ImageBox* imagebox, ImageScaleMode mode);
void ImageBox_SetAlignment(ImageBox* imagebox, ImageAlignment alignment);
```

**Scale Modes:**
- `IMAGE_SCALE_STRETCH`: Stretch to fill entire bounds
- `IMAGE_SCALE_FIT`: Scale to fit within bounds (maintain aspect ratio)
- `IMAGE_SCALE_FILL`: Scale to fill bounds (maintain aspect ratio, may crop)
- `IMAGE_SCALE_NONE`: Display at original size (may be clipped)

**Alignment Options:**
- `IMAGE_ALIGN_TOP_LEFT`, `IMAGE_ALIGN_TOP_CENTER`, `IMAGE_ALIGN_TOP_RIGHT`
- `IMAGE_ALIGN_CENTER_LEFT`, `IMAGE_ALIGN_CENTER`, `IMAGE_ALIGN_CENTER_RIGHT`
- `IMAGE_ALIGN_BOTTOM_LEFT`, `IMAGE_ALIGN_BOTTOM_CENTER`, `IMAGE_ALIGN_BOTTOM_RIGHT`

### Visual Styling

```c
void ImageBox_SetTint(ImageBox* imagebox, Color tint);
void ImageBox_SetOpacity(ImageBox* imagebox, float opacity);
void ImageBox_SetBorder(ImageBox* imagebox, bool show, Color color, float width);
void ImageBox_SetBorderRadius(ImageBox* imagebox, float radius);
void ImageBox_SetBackground(ImageBox* imagebox, bool show, Color color);
```

### Transformations

```c
void ImageBox_SetRotation(ImageBox* imagebox, float rotation, Vector2 origin);
void ImageBox_SetFlip(ImageBox* imagebox, bool horizontal, bool vertical);
```

### Interaction

```c
void ImageBox_SetClickable(ImageBox* imagebox, bool clickable);
void ImageBox_SetVisible(ImageBox* imagebox, bool visible);
bool ImageBox_IsClicked(const ImageBox* imagebox);
bool ImageBox_IsHovered(const ImageBox* imagebox);
```

### Update and Drawing

```c
void ImageBox_Update(ImageBox* imagebox);
void ImageBox_Draw(const ImageBox* imagebox);
```

### Example Usage

```c
// Load image from file
ImageBox logo = ImageBox_CreateFromFile((Rectangle){100, 50, 200, 100}, "assets/logo.png");
ImageBox_SetScaleMode(&logo, IMAGE_SCALE_FIT);
ImageBox_SetAlignment(&logo, IMAGE_ALIGN_CENTER);

// Create avatar with background
ImageBox avatar = ImageBox_Create((Rectangle){300, 50, 60, 60});
ImageBox_SetBackground(&avatar, true, (Color){34, 197, 94, 150});
ImageBox_SetBorderRadius(&avatar, 30.0f); // Circular
ImageBox_SetBorder(&avatar, true, WHITE, 2.0f);

// Icon with click interaction
ImageBox settings_icon = ImageBox_Create((Rectangle){400, 50, 32, 32});
ImageBox_SetClickable(&settings_icon, true);
ImageBox_SetBackground(&settings_icon, true, (Color){59, 130, 246, 100});
ImageBox_SetBorderRadius(&settings_icon, 4.0f);

// Main loop
while (!WindowShouldClose()) {
    ImageBox_Update(&logo);
    ImageBox_Update(&avatar);
    ImageBox_Update(&settings_icon);
    
    if (ImageBox_IsClicked(&settings_icon)) {
        OpenSettingsMenu();
    }
    
    BeginDrawing();
    ImageBox_Draw(&logo);
    ImageBox_Draw(&avatar);
    ImageBox_Draw(&settings_icon);
    
    // Draw procedural content on top if needed
    if (avatar.texture.id == 0) {
        Vector2 center = {avatar.bounds.x + avatar.bounds.width/2, avatar.bounds.y + avatar.bounds.height/2};
        DrawText("JD", center.x - 8, center.y - 6, 12, WHITE);
    }
    
    EndDrawing();
}

ImageBox_Destroy(&logo);
ImageBox_Destroy(&avatar);
ImageBox_Destroy(&settings_icon);
```

### Common Use Cases

**Profile Pictures:**
```c
ImageBox profile = ImageBox_Create((Rectangle){x, y, 50, 50});
ImageBox_SetBackground(&profile, true, LIGHTGRAY);
ImageBox_SetBorderRadius(&profile, 25.0f); // Circular
ImageBox_LoadFromFile(&profile, user_avatar_path);
ImageBox_SetScaleMode(&profile, IMAGE_SCALE_FILL);
```

**Icon Buttons:**
```c
ImageBox icon_btn = ImageBox_Create((Rectangle){x, y, 24, 24});
ImageBox_SetClickable(&icon_btn, true);
ImageBox_LoadFromFile(&icon_btn, "icons/settings.png");
ImageBox_SetTint(&icon_btn, DARKGRAY);

if (ImageBox_IsHovered(&icon_btn)) {
    ImageBox_SetTint(&icon_btn, BLACK);
}
```

**Image Gallery:**
```c
ImageBox gallery_item = ImageBox_CreateFromFile((Rectangle){x, y, 150, 100}, image_path);
ImageBox_SetScaleMode(&gallery_item, IMAGE_SCALE_FILL);
ImageBox_SetBorder(&gallery_item, true, GRAY, 1.0f);
ImageBox_SetClickable(&gallery_item, true);
```

## Separator Component

Visual dividers for organizing content and creating layout structure.

### Creation and Destruction

```c
Separator Separator_Create(Rectangle bounds, SeparatorType type);
Separator Separator_CreateHorizontal(float x, float y, float width);
Separator Separator_CreateVertical(float x, float y, float height);
Separator Separator_CreateSpacer(float x, float y, float width, float height);
Separator Separator_CreateWithText(Rectangle bounds, const char* text, SeparatorTextAlign align);
void Separator_Destroy(Separator* separator);
```

**Separator Types:**
- `SEPARATOR_HORIZONTAL`: Horizontal line divider
- `SEPARATOR_VERTICAL`: Vertical line divider  
- `SEPARATOR_SPACER`: Invisible spacing element

**Text Alignment:**
- `SEPARATOR_TEXT_LEFT`: Text aligned to left
- `SEPARATOR_TEXT_CENTER`: Text centered
- `SEPARATOR_TEXT_RIGHT`: Text aligned to right

### Visual Styling

```c
void Separator_SetStyle(Separator* separator, SeparatorStyle style);
void Separator_SetColor(Separator* separator, Color color);
void Separator_SetThickness(Separator* separator, float thickness);
void Separator_SetOpacity(Separator* separator, float opacity);
void Separator_SetVisible(Separator* separator, bool visible);
```

**Line Styles:**
- `SEPARATOR_SOLID`: Solid line (default)
- `SEPARATOR_DASHED`: Dashed line pattern
- `SEPARATOR_DOTTED`: Dotted line pattern

### Layout and Spacing

```c
void Separator_SetMargin(Separator* separator, float top, float right, float bottom, float left);
void Separator_SetMarginHorizontal(Separator* separator, float horizontal);
void Separator_SetMarginVertical(Separator* separator, float vertical);
void Separator_SetMarginAll(Separator* separator, float margin);
```

### Text Labels

```c
void Separator_SetText(Separator* separator, const char* text);
void Separator_SetTextAlign(Separator* separator, SeparatorTextAlign align);
void Separator_SetTextColor(Separator* separator, Color color);
void Separator_SetFontSize(Separator* separator, int font_size);
void Separator_SetTextPadding(Separator* separator, float padding);
void Separator_ClearText(Separator* separator);
```

### Advanced Styling

```c
void Separator_SetDashPattern(Separator* separator, float dash_length, float gap_length);
void Separator_SetFadeIn(Separator* separator, float duration);
void Separator_StartFadeIn(Separator* separator);
void Separator_SetZIndex(Separator* separator, int z_index);
```

### Update and Drawing

```c
void Separator_Update(Separator* separator);
void Separator_Draw(const Separator* separator);
```

### Preset Creators

```c
Separator Separator_CreateMenuDivider(float x, float y, float width);
Separator Separator_CreateFormSection(float x, float y, float width, const char* section_title);
Separator Separator_CreateToolbarDivider(float x, float y, float height);
Separator Separator_CreateContentBreak(float x, float y, float width);
```

### Example Usage

```c
// Basic horizontal divider
Separator divider = Separator_CreateHorizontal(50, 200, 300);
Separator_SetColor(&divider, GRAY);
Separator_SetThickness(&divider, 2.0f);

// Form section with title
Separator section = Separator_CreateFormSection(50, 100, 400, "Personal Information");

// Vertical toolbar divider
Separator toolbar_div = Separator_CreateToolbarDivider(200, 50, 40);

// Dashed content break
Separator content_break = Separator_CreateContentBreak(50, 300, 500);

// Custom separator with text
Separator custom = Separator_CreateWithText((Rectangle){50, 150, 400, 20}, "OR", SEPARATOR_TEXT_CENTER);
Separator_SetStyle(&custom, SEPARATOR_DASHED);
Separator_SetTextColor(&custom, DARKBLUE);

// Invisible spacer for layout
Separator spacer = Separator_CreateSpacer(0, 250, 400, 30);

// Main loop
while (!WindowShouldClose()) {
    Separator_Update(&divider);
    Separator_Update(&section);
    Separator_Update(&toolbar_div);
    Separator_Update(&content_break);
    Separator_Update(&custom);
    
    BeginDrawing();
    ClearBackground(WHITE);
    
    Separator_Draw(&divider);
    Separator_Draw(&section);
    Separator_Draw(&toolbar_div);
    Separator_Draw(&content_break);
    Separator_Draw(&custom);
    
    EndDrawing();
}

// Cleanup
Separator_Destroy(&section);
Separator_Destroy(&custom);
```

### Common Use Cases

**Menu Separators:**
```c
Separator menu_sep = Separator_CreateMenuDivider(10, y_pos, menu_width);
```

**Form Sections:**
```c
Separator personal_info = Separator_CreateFormSection(x, y, width, "Personal Details");
Separator contact_info = Separator_CreateFormSection(x, y + 150, width, "Contact Information");
```

**Toolbar Dividers:**
```c
Separator tool_div1 = Separator_CreateToolbarDivider(100, 10, 30);
Separator tool_div2 = Separator_CreateToolbarDivider(200, 10, 30);
```

**Content Breaks:**
```c
Separator article_break = Separator_CreateContentBreak(margin, y_pos, content_width);
```

This components reference provides the complete API for building sophisticated user interfaces with the Raylib UI Library.