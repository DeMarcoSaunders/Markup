# MarkUp UI - Lightweight C GUI Library

A robust, lightweight GUI library for C applications built on top of Raylib. MarkUp UI provides a comprehensive set of pre-made components with CSS-like styling capabilities, making it easy to create beautiful and responsive user interfaces.

## 🚀 Features

### **Core Systems**
- **Error Handling** - Comprehensive error management with detailed reporting
- **Component Base** - Unified component lifecycle and management
- **Style System** - CSS-like styling with specificity and inheritance
- **Layout Engine** - Advanced layout system supporting Flexbox and Grid
- **Event System** - Robust event handling with bubbling and capturing

### **UI Components**
- **Basic Controls**: Button, Checkbox, Radio Button, Slider
- **Input Components**: Text Input, Text Area, Dropdown
- **Layout Components**: Panel, Sidebar, Card, Separator
- **Feedback Components**: Loading Bar, Spinner, Toast, Modal
- **Navigation**: Tabs, Breadcrumb (planned)
- **Data Display**: Table, List, Tree (planned)
- **Visual Effects**: Shadow, Backdrop, Blur Effect, Background Images

### **Key Benefits**
- **Lightweight** - Minimal memory footprint and dependencies
- **Robust** - Comprehensive error handling and validation
- **Flexible** - CSS-like styling system with inheritance
- **Performant** - Optimized rendering and layout calculations
- **Extensible** - Easy to add custom components
- **Cross-platform** - Works on Windows, macOS, Linux, and more

## 📦 Installation

### Prerequisites
- [Raylib](https://www.raylib.com/) (4.0 or later)
- C compiler (GCC, Clang, MSVC)

### Quick Start
1. Clone the repository:
```bash
git clone https://github.com/yourusername/markup-ui.git
cd markup-ui
```

2. Include the library in your project:
```c
#include "markup.h"
```

3. Link against Raylib in your build system.

## 🎯 Quick Example

```c
#include "markup.h"
#include <stdio.h>

void OnButtonClick(Event* event, void* user_data) {
    printf("Button clicked!\n");
}

int main(void) {
    // Initialize Raylib
    InitWindow(800, 600, "MarkUp UI Example");
    SetTargetFPS(60);

    // Initialize MarkUp system
    MarkupSystem markup;
    Markup_Init(&markup);
    Markup_SetDebugMode(&markup, true);

    // Create a root panel
    Panel root_panel = Panel_Create((Rectangle){0, 0, 800, 600});
    ComponentBase_Init((ComponentBase*)&root_panel, COMPONENT_TYPE_PANEL, root_panel.rect);
    Markup_SetRootComponent(&markup, (ComponentBase*)&root_panel);

    // Create a button
    Button button = Button_Create((Rectangle){50, 50, 200, 50}, "Click Me!", BUTTON_PRIMARY);
    ComponentBase_Init((ComponentBase*)&button, COMPONENT_TYPE_BUTTON, button.rect);

    // Add event listener
    EventSystem_AddEventListener(&markup.event_system, (ComponentBase*)&button, 
                                EVENT_MOUSE_CLICK, OnButtonClick, NULL);

    // Add button to panel
    Panel_AddButton(&root_panel, &button, 0, 200.0f);

    // Main loop
    while (!WindowShouldClose()) {
        Markup_Update(&markup);

        BeginDrawing();
        ClearBackground(RAYWHITE);
        Markup_Draw(&markup);
        EndDrawing();
    }

    // Cleanup
    ComponentBase_Destroy((ComponentBase*)&button);
    ComponentBase_Destroy((ComponentBase*)&root_panel);
    Markup_Destroy(&markup);
    CloseWindow();

    return 0;
}
```

## 📚 Component Documentation

### **Basic Components**

#### Button
```c
Button button = Button_Create(rect, "Text", BUTTON_PRIMARY);
Button_SetColors(&button, background, text, border);
Button_SetOnClick(&button, callback_function);
```

#### Loading Bar
```c
LoadingBar bar = LoadingBar_Create(rect, LOADING_BAR_STYLE_LINEAR);
LoadingBar_SetProgress(&bar, 0.75f); // 75% complete
LoadingBar_SetShowPercentage(&bar, true);
```

#### Spinner
```c
Spinner spinner = Spinner_Create(rect, SPINNER_STYLE_CIRCULAR);
Spinner_SetShowText(&spinner, true);
Spinner_SetText(&spinner, "Loading...");
```

#### Toast Notifications
```c
ToastManager* manager = ToastManager_Create();
Toast* toast = ToastManager_ShowSuccess(manager, "Success!", "Operation completed");
```

### **Layout Components**

#### Panel (Flexbox Container)
```c
Panel panel = Panel_Create(rect);
Panel_SetFlexDirection(&panel, FLEX_DIRECTION_COLUMN);
Panel_SetJustifyContent(&panel, JUSTIFY_CENTER);
Panel_SetAlignItems(&panel, ALIGN_CENTER);
```

#### Tabs
```c
Tabs tabs = Tabs_Create(rect, TABS_STYLE_LINEAR);
Tabs_AddTab(&tabs, "Tab 1", "tab1");
Tabs_AddTab(&tabs, "Tab 2", "tab2");
Tabs_SetActiveTab(&tabs, 0);
```

#### Card
```c
Card card = Card_Create(rect, CARD_STYLE_ELEVATED);
Card_SetTitle(&card, "Card Title");
Card_SetSubtitle(&card, "Card Subtitle");
Card_SetContent(&card, some_component);
```

## 🎨 Styling System

MarkUp UI includes a CSS-like styling system:

```c
// Set colors
Button_SetColors(&button, 
    (Color){240, 240, 240, 255},  // Background
    (Color){59, 130, 246, 255},   // Text
    (Color){200, 200, 200, 255}   // Border
);

// Set dimensions
Button_SetPadding(&button, 10.0f);
Button_SetCornerRadius(&button, 8.0f);
Button_SetBorderWidth(&button, 2.0f);
```

## 🖼️ Background Images

MarkUp UI supports rich background images with multiple scaling and positioning options:

```c
// Create a background image
BackgroundImage* bg = Safe_Malloc(sizeof(BackgroundImage));
BackgroundImage_Init(bg);

// Load from file
BackgroundImage_LoadFromFile(bg, "assets/gradient.png");

// Or create programmatically
BackgroundImage_SetPresetGradient(bg, 
    (Color){59, 130, 246, 255},   // Start color
    (Color){147, 51, 234, 255},   // End color
    true                          // Horizontal gradient
);

// Configure scaling and positioning
BackgroundImage_SetScaleMode(bg, BACKGROUND_SCALE_STRETCH);
BackgroundImage_SetPosition(bg, BACKGROUND_POSITION_CENTER);
BackgroundImage_SetOpacity(bg, 0.8f);

// Draw the background
BackgroundImage_Draw(bg, component_rect);

// Cleanup
BackgroundImage_Destroy(bg);
Safe_Free(bg);
```

### Background Image Features:
- **Multiple Scale Modes**: Stretch, Fit, Cover, Tile, Center, None
- **9 Positioning Options**: Top-left, Top-center, Top-right, Center-left, Center, Center-right, Bottom-left, Bottom-center, Bottom-right
- **Tiling Support**: Repeat patterns horizontally, vertically, or both
- **Blend Modes**: Normal, Multiply, Screen, Overlay, and more
- **Animation Support**: Animated backgrounds with frame-based animation
- **Preset Patterns**: Gradients, checkerboards, stripes, noise
- **Caching System**: Automatic texture caching for performance

## 🔧 Error Handling

The library includes comprehensive error handling:

```c
if (!Markup_Init(&markup)) {
    printf("Failed to initialize: %s\n", Markup_GetLastErrorMessage(&markup));
    return -1;
}

// Check for errors during operation
if (Markup_HasError(&markup)) {
    printf("Error: %s\n", Markup_GetLastErrorMessage(&markup));
    Markup_ClearError(&markup);
}
```

## 📁 Project Structure

```
markup-ui/
├── components/           # UI Components
│   ├── button.h/c       # Button component
│   ├── panel.h/c        # Panel container
│   ├── loading_bar.h/c  # Loading indicators
│   ├── spinner.h/c      # Spinner animations
│   ├── tabs.h/c         # Tab interface
│   ├── toast.h/c        # Toast notifications
│   ├── card.h/c         # Card containers
│   └── ...              # Other components
├── error_handling.h/c   # Error management
├── component_base.h/c   # Base component system
├── style_system.h/c     # CSS-like styling
├── layout_engine.h/c    # Layout calculations
├── event_system.h/c     # Event handling
├── markup.h            # Main library header
├── theme.h             # Theme definitions
└── examples/           # Example applications
```

## 🚧 Planned Components

- **Table/Grid** - Data display
- **List/ListView** - List components
- **Tree/TreeView** - Hierarchical data
- **Color Picker** - Color selection
- **Date/Time Picker** - Date and time input
- **File Upload** - File selection
- **Menu/MenuBar** - Navigation menus
- **Tooltip** - Hover information
- **Accordion** - Collapsible sections

## 🤝 Contributing

Contributions are welcome! Please feel free to submit a Pull Request. For major changes, please open an issue first to discuss what you would like to change.

### Development Setup
1. Fork the repository
2. Create a feature branch
3. Make your changes
4. Add tests if applicable
5. Submit a pull request

## 📄 License

This project is licensed under the MIT License - see the [LICENSE](LICENSE) file for details.

## 🙏 Acknowledgments

- Built on top of [Raylib](https://www.raylib.com/) - A simple and easy-to-use library for games programming
- Inspired by modern web UI frameworks and CSS styling systems
- Designed for performance and ease of use

## 📞 Support

- **Issues**: [GitHub Issues](https://github.com/yourusername/markup-ui/issues)
- **Discussions**: [GitHub Discussions](https://github.com/yourusername/markup-ui/discussions)
- **Documentation**: [Wiki](https://github.com/yourusername/markup-ui/wiki)

---

**MarkUp UI** - Making C GUI development simple, robust, and beautiful! 🎨