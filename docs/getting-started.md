# Getting Started Guide

This guide will help you set up and use the Markup UI Library in your project.

## Prerequisites

- **CMake 3.15+** - Build system
- **C99 compatible compiler** - GCC, Clang, or MSVC
- **raylib** - Graphics library (included in this project)

## Installation

### Option 1: Use as Submodule (Recommended)

```bash
# Add as git submodule to your project
git submodule add https://github.com/your-repo/markup-ui.git external/markup-ui

# Or clone directly
git clone https://github.com/your-repo/markup-ui.git external/markup-ui
```

### Option 2: Copy Files

Copy the following files to your project:

```
your_project/
├── components/           # UI component implementations
│   ├── button.h/c       # Button component
│   ├── checkbox.h/c     # Checkbox component
│   ├── dropdown.h/c     # Dropdown component
│   ├── modal.h/c        # Modal component
│   ├── panel.h/c        # Layout panel component
│   ├── radio_button.h/c # Radio button component
│   ├── slider.h/c       # Slider component
│   ├── text_area.h/c    # Text area component
│   ├── text_input.h/c   # Text input component
│   └── blur_effect.h/c  # Blur effect utilities
├── vendor/              # Third-party libraries
│   ├── vec.h/c         # Vector implementation
│   └── shadow.h/c      # Shadow rendering utilities
├── external/            # External dependencies
│   └── raylib/         # raylib library
├── theme.h/c           # Theme system
└── CMakeLists.txt      # Build configuration
```

## Building with CMake

### Step 1: Create CMakeLists.txt

```cmake
cmake_minimum_required(VERSION 3.15)
project(MyUIApp)

# Set C standard
set(CMAKE_C_STANDARD 99)

# Add raylib (if not using the included version)
# find_package(raylib REQUIRED)

# Add UI library
add_subdirectory(external/markup-ui)

# Create your executable
add_executable(my_app main.c)

# Link with the UI library
target_link_libraries(my_app markup_ui)

# Platform-specific libraries
if(WIN32)
    target_link_libraries(my_app winmm gdi32)
endif()
```

### Step 2: Build Your Project

```bash
mkdir build
cd build
cmake ..
cmake --build . --config Debug
```

## Your First Application

### Basic Setup

```c
#include "raylib.h"
#include "components/button.h"
#include "theme.h"

int main(void) {
    // Initialize window
    InitWindow(800, 600, "My UI App");
    SetTargetFPS(60);
    
    // Initialize UI system
    Theme_Init_Default();
    
    // Create a button
    Button my_button = Button_Create(
        (Rectangle){300, 250, 200, 40},  // Position and size
        "Click Me!",                      // Text
        BUTTON_PRIMARY                    // Style variant
    );
    
    // Main game loop
    while (!WindowShouldClose()) {
        // Update button (handles mouse input)
        Button_Update(&my_button);
        
        // Check if button was clicked
        if (Button_IsClicked(&my_button)) {
            printf("Button was clicked!\n");
        }
        
        // Drawing
        BeginDrawing();
        ClearBackground(RAYWHITE);
        
        // Draw the button
        Button_Draw(&my_button);
        
        EndDrawing();
    }
    
    // Cleanup
    Button_Destroy(&my_button);
    CloseWindow();
    return 0;
}
```

### Advanced Example with Layout

```c
#include "raylib.h"
#include "components/panel.h"
#include "components/button.h"
#include "components/text_input.h"
#include "theme.h"

int main(void) {
    InitWindow(800, 600, "Layout Example");
    SetTargetFPS(60);
    
    Theme_Init_Default();
    
    // Create main panel
    Panel main_panel = Panel_Create((Rectangle){10, 10, 780, 580});
    Panel_SetFlexDirection(&main_panel, FLEX_DIRECTION_COLUMN);
    Panel_SetGap(&main_panel, 20);
    
    // Create components
    TextInput name_input = TextInput_Create((Rectangle){0, 0, 300, 40}, "Enter your name...");
    Button submit_btn = Button_Create((Rectangle){0, 0, 200, 40}, "Submit", BUTTON_PRIMARY);
    
    // Add components to panel
    Panel_AddTextInput(&main_panel, &name_input, 0, 40);
    Panel_AddButton(&main_panel, &submit_btn, 0, 40);
    
    while (!WindowShouldClose()) {
        // Update components
        Panel_Update(&main_panel);
        
        // Handle interactions
        if (Button_IsClicked(&submit_btn)) {
            printf("Name: %s\n", TextInput_GetText(&name_input));
        }
        
        // Drawing
        BeginDrawing();
        ClearBackground(RAYWHITE);
        Panel_Draw(&main_panel);
        EndDrawing();
    }
    
    // Cleanup
    TextInput_Destroy(&name_input);
    Button_Destroy(&submit_btn);
    Panel_Destroy(&main_panel);
    CloseWindow();
    return 0;
}
```

## Component Lifecycle

Every component follows this lifecycle:

1. **Create** - Initialize the component
2. **Configure** - Set properties and styling
3. **Update** - Handle input and state changes
4. **Draw** - Render the component
5. **Destroy** - Clean up resources

```c
// 1. Create
Button btn = Button_Create(rect, "Text", BUTTON_PRIMARY);

// 2. Configure (optional)
Button_SetBackgroundColor(&btn, RED);
Button_SetBorderRadius(&btn, 8, 8, 8, 8);

// 3. Update (in main loop)
Button_Update(&btn);

// 4. Draw (in main loop)
Button_Draw(&btn);

// 5. Destroy (cleanup)
Button_Destroy(&btn);
```

## Available Components

### Basic Components
- **Button** - Interactive buttons with variants
- **Checkbox** - Toggle controls
- **Radio Button** - Single selection controls
- **Slider** - Value input controls

### Advanced Components
- **Dropdown** - Selection menus with search
- **Text Input** - Single-line text entry
- **Text Area** - Multi-line text editing
- **Modal** - Overlay dialogs

### Layout Components
- **Panel** - Flexbox layout container

## Next Steps

1. **Explore Components** - Check the [Components Reference](components.md)
2. **Learn Theming** - Read the [Theme System Guide](theme-system.md)
3. **See Examples** - Look at the [Examples](examples.md)
4. **Try the Demos** - Run `demo.exe` to see all components in action

## Troubleshooting

### Common Issues

**Build Errors:**
- Ensure CMake 3.15+ is installed
- Check that raylib is properly linked
- Verify C99 standard is set

**Runtime Errors:**
- Always call `Theme_Init_Default()` before creating components
- Call `Update()` before `Draw()` for each component
- Don't forget to call `Destroy()` for cleanup

**Visual Issues:**
- Check that components are within window bounds
- Verify theme is initialized
- Ensure proper drawing order

### Getting Help

- Check the [Components Reference](components.md) for detailed API docs
- Look at the [Examples](examples.md) for usage patterns
- Run the demos to see working implementations
- Check the source code for advanced usage