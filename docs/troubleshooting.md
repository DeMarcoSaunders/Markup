# Troubleshooting Guide

This guide helps you resolve common issues when using the Markup UI Library.

## Build Issues

### CMake Configuration Errors

**Error: `CMake Error: Could not find raylib`**

**Solution:**
```cmake
# Option 1: Use the included raylib
add_subdirectory(external/raylib)

# Option 2: Install raylib system-wide
# On Windows: vcpkg install raylib
# On macOS: brew install raylib
# On Linux: sudo apt install libraylib-dev
```

**Error: `C99 standard not supported`**

**Solution:**
```cmake
# Add to your CMakeLists.txt
set(CMAKE_C_STANDARD 99)
set(CMAKE_C_STANDARD_REQUIRED ON)
```

### Compilation Errors

**Error: `'vec_t' undeclared identifier`**

**Solution:**
- Ensure `vendor/vec.h` is included
- Check that `vendor/vec.c` is added to your build

**Error: `'Button' undeclared identifier`**

**Solution:**
```c
// Include the specific component header
#include "components/button.h"

// Or include all components
#include "components/button.h"
#include "components/panel.h"
// ... other components
```

**Error: `'Theme_Init_Default' undeclared`**

**Solution:**
```c
// Include the theme header
#include "theme.h"

// Call initialization before creating components
Theme_Init_Default();
```

### Linker Errors

**Error: `undefined reference to 'vec_expand_'`**

**Solution:**
- Add `vendor/vec.c` to your build sources
- Ensure the vector implementation is compiled

**Error: `undefined reference to raylib functions`**

**Solution:**
```cmake
# Link with raylib
target_link_libraries(your_app raylib)

# Platform-specific libraries
if(WIN32)
    target_link_libraries(your_app winmm gdi32)
endif()
```

## Runtime Issues

### Components Not Responding

**Problem: Components don't react to mouse input**

**Solutions:**
1. **Check Update Order:**
```c
// Correct order
Button_Update(&button);  // Handle input
Button_Draw(&button);    // Render
```

2. **Verify Bounds:**
```c
// Ensure component is within window bounds
Rectangle bounds = {100, 100, 200, 40};  // x, y, width, height
Button button = Button_Create(bounds, "Text", BUTTON_PRIMARY);
```

3. **Check Mouse Coordinates:**
```c
// Debug mouse position
Vector2 mouse = GetMousePosition();
printf("Mouse: %.0f, %.0f\n", mouse.x, mouse.y);
```

### Visual Issues

**Problem: Components are invisible**

**Solutions:**
1. **Check Theme Initialization:**
```c
// Must be called before creating components
Theme_Init_Default();
```

2. **Verify Drawing Order:**
```c
BeginDrawing();
ClearBackground(RAYWHITE);  // Clear first
Button_Draw(&button);       // Draw components
EndDrawing();
```

3. **Check Component Visibility:**
```c
// Ensure component is visible
Button_SetVisible(&button, true);
```

**Problem: Components appear in wrong colors**

**Solutions:**
1. **Check Theme Colors:**
```c
// Verify theme is loaded
Color bg = AppTheme.components[COMPONENT_BUTTON].colors[STATE_DEFAULT].background;
printf("Button background: %d, %d, %d, %d\n", bg.r, bg.g, bg.b, bg.a);
```

2. **Custom Colors:**
```c
// Override theme colors
Button_SetBackgroundColor(&button, (Color){255, 0, 0, 255});
Button_SetTextColor(&button, (Color){255, 255, 255, 255});
```

### Layout Issues

**Problem: Components overlap or are positioned incorrectly**

**Solutions:**
1. **Use Panels for Layout:**
```c
Panel panel = Panel_Create((Rectangle){10, 10, 780, 580});
Panel_SetFlexDirection(&panel, FLEX_DIRECTION_COLUMN);
Panel_SetGap(&panel, 10);
Panel_AddButton(&panel, &button, 0, 40);
```

2. **Check Rectangle Bounds:**
```c
// Ensure positive dimensions
Rectangle rect = {x, y, width, height};  // width > 0, height > 0
```

3. **Account for Padding/Margin:**
```c
// Components have internal padding
float total_width = button_width + padding_left + padding_right;
```

### Memory Issues

**Problem: Memory leaks or crashes**

**Solutions:**
1. **Proper Cleanup:**
```c
// Always destroy components
Button_Destroy(&button);
TextInput_Destroy(&input);
Panel_Destroy(&panel);
```

2. **Check for Null Pointers:**
```c
// Validate before use
if (button != NULL) {
    Button_Update(button);
    Button_Draw(button);
}
```

3. **Avoid Double Destruction:**
```c
// Set to NULL after destruction
Button_Destroy(&button);
button = NULL;  // Prevent double-free
```

## Component-Specific Issues

### Button Issues

**Problem: Button doesn't show click state**

**Solution:**
```c
// Check click state properly
if (Button_IsClicked(&button)) {
    // Handle click
}

// Or check the field directly
if (button.is_clicked) {
    // Handle click
}
```

### Dropdown Issues

**Problem: Dropdown options don't appear**

**Solutions:**
1. **Add Options:**
```c
Dropdown_AddOption(&dropdown, "Option 1");
Dropdown_AddOption(&dropdown, "Option 2");
```

2. **Check Dropdown State:**
```c
// Ensure dropdown is open
if (dropdown.is_open) {
    // Options should be visible
}
```

### Text Input Issues

**Problem: Can't type in text input**

**Solutions:**
1. **Set Focus:**
```c
TextInput_SetFocus(&input, true);
```

2. **Check Read-only State:**
```c
// Ensure not read-only
TextInput_SetReadonly(&input, false);
```

### Panel Issues

**Problem: Components in panel don't layout correctly**

**Solutions:**
1. **Set Flex Properties:**
```c
Panel_SetFlexDirection(&panel, FLEX_DIRECTION_COLUMN);
Panel_SetGap(&panel, 10);
Panel_SetAlignItems(&panel, ALIGN_START);
```

2. **Check Child Properties:**
```c
// Set flex properties for children
Panel_SetChildFlexGrow(&panel, child_index, 1);
Panel_SetChildFlexBasis(&panel, child_index, 100);
```

## Performance Issues

### Slow Rendering

**Problem: Application runs slowly**

**Solutions:**
1. **Limit Update Frequency:**
```c
// Only update when needed
if (component_needs_update) {
    Component_Update(&component);
}
```

2. **Reduce Visual Effects:**
```c
// Disable expensive effects
Button_SetShadow(&button, false);
Button_SetOutline(&button, false);
```

3. **Optimize Drawing:**
```c
// Only draw visible components
if (component.is_visible && IsInViewport(component.bounds)) {
    Component_Draw(&component);
}
```

### High Memory Usage

**Problem: Application uses too much memory**

**Solutions:**
1. **Destroy Unused Components:**
```c
// Clean up when not needed
if (!component_needed) {
    Component_Destroy(&component);
}
```

2. **Limit Dynamic Allocations:**
```c
// Use static buffers when possible
char static_buffer[256];
// Instead of dynamic allocation
```

## Debugging Tips

### Enable Debug Output

```c
// Add debug prints
printf("Button bounds: %.0f, %.0f, %.0f, %.0f\n", 
       button.rect.x, button.rect.y, button.rect.width, button.rect.height);
printf("Mouse position: %.0f, %.0f\n", mouse.x, mouse.y);
printf("Button state: hover=%d, pressed=%d, clicked=%d\n", 
       button.is_hovered, button.is_pressed, button.is_clicked);
```

### Visual Debugging

```c
// Draw component bounds
DrawRectangleLines(button.rect.x, button.rect.y, 
                  button.rect.width, button.rect.height, RED);

// Draw mouse position
DrawCircle(mouse.x, mouse.y, 3, BLUE);
```

### Check Component State

```c
// Verify component properties
printf("Button text: %s\n", button.text);
printf("Button variant: %d\n", button.variant);
printf("Button visible: %d\n", button.is_visible);
printf("Button disabled: %d\n", button.is_disabled);
```

## Getting Help

If you're still having issues:

1. **Check the Examples:**
   - Run `demo.exe` to see working implementations
   - Look at `demo.c` for complete examples

2. **Review the Source:**
   - Check component source files for implementation details
   - Look at `theme.c` for theme system details

3. **Common Patterns:**
   - Always initialize theme before components
   - Call Update before Draw
   - Destroy components before exit
   - Check bounds and visibility

4. **Report Issues:**
   - Include error messages
   - Provide minimal reproduction code
   - Specify platform and compiler version 