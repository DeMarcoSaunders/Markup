# Contributing to MarkUp UI

Thank you for your interest in contributing to MarkUp UI! This document provides guidelines and information for contributors.

## 🚀 Getting Started

### Prerequisites

- C/C++ compiler (GCC, Clang, or MSVC)
- CMake 3.15 or higher
- Git
- Basic knowledge of C programming and raylib

### Development Setup

1. **Fork the repository**
   ```bash
   # Click "Fork" on GitHub, then clone your fork
   git clone https://github.com/yourusername/markup-ui.git
   cd markup-ui
   ```

2. **Set up the development environment**
   ```bash
   # Initialize submodules
   git submodule update --init --recursive
   
   # Create build directory
   mkdir build && cd build
   
   # Configure for development
   cmake .. -DCMAKE_BUILD_TYPE=Debug
   
   # Build
   cmake --build .
   ```

3. **Test your setup**
   ```bash
   # Run the showcase to verify everything works
   ./showcase        # Linux/macOS
   .\showcase.exe    # Windows
   ```

## 🎯 How to Contribute

### Reporting Issues

- Use the GitHub issue tracker
- Provide clear, detailed descriptions
- Include steps to reproduce bugs
- Mention your operating system and compiler
- Include relevant code snippets or screenshots

### Suggesting Features

- Open an issue with the "enhancement" label
- Describe the feature and its use case
- Explain how it fits with the library's goals
- Consider providing a mockup or example

### Code Contributions

1. **Create a feature branch**
   ```bash
   git checkout -b feature/your-feature-name
   ```

2. **Make your changes**
   - Follow the coding standards (see below)
   - Add tests if applicable
   - Update documentation
   - Test with the showcase application

3. **Commit your changes**
   ```bash
   git add .
   git commit -m "Add: brief description of your changes"
   ```

4. **Push and create a Pull Request**
   ```bash
   git push origin feature/your-feature-name
   ```

## 📝 Coding Standards

### Code Style

- **Indentation**: 4 spaces (no tabs)
- **Naming**: 
  - Functions: `PascalCase` (e.g., `Button_Create`)
  - Variables: `snake_case` (e.g., `button_rect`)
  - Constants: `UPPER_CASE` (e.g., `MAX_BUTTONS`)
  - Types: `PascalCase` (e.g., `ButtonState`)

### File Organization

- **Headers**: Place in `components/` directory
- **Implementation**: Corresponding `.c` file in `components/`
- **Documentation**: Update relevant files in `docs/`

### Component Structure

New components should follow this pattern:

```c
// component_name.h
#ifndef COMPONENT_NAME_H
#define COMPONENT_NAME_H

#include "raylib.h"
#include "theme.h"

typedef struct {
    Rectangle bounds;
    bool is_visible;
    // ... component-specific fields
} ComponentName;

// Core functions
ComponentName ComponentName_Create(/* parameters */);
void ComponentName_Update(ComponentName* component);
void ComponentName_Draw(const ComponentName* component);
void ComponentName_Destroy(ComponentName* component);

// Additional functions...

#endif // COMPONENT_NAME_H
```

### Memory Management

- Always provide `Create` and `Destroy` functions
- Use `malloc`/`free` for dynamic allocation
- Check for NULL pointers
- Clean up resources in `Destroy` functions

### Error Handling

- Check parameters for NULL values
- Return early on invalid input
- Use meaningful return values (bool for success/failure)
- Print warnings for non-critical issues

## 🧪 Testing

### Manual Testing

- Test your changes with the showcase application
- Verify components work in different states
- Check memory usage (no leaks)
- Test on different screen sizes if relevant

### Adding to Showcase

When adding new components, please:

1. Add them to the appropriate tab in `main.c`
2. Demonstrate key features and interactions
3. Include proper cleanup in the destruction section
4. Update the showcase documentation

## 📚 Documentation

### Code Documentation

- Use clear, descriptive comments
- Document complex algorithms
- Explain non-obvious design decisions
- Include usage examples in headers

### User Documentation

- Update `README.md` for new features
- Add component documentation to `docs/components.md`
- Include code examples
- Update the showcase description if relevant

## 🎨 Component Guidelines

### Design Principles

- **Consistency**: Follow existing component patterns
- **Flexibility**: Allow customization through styling
- **Performance**: Minimize allocations and drawing calls
- **Accessibility**: Consider different use cases and needs

### Theme Integration

- Use the theme system for colors and styling
- Support custom styling overrides
- Follow the existing color palette structure
- Test with different themes

### State Management

- Implement proper state tracking (hover, pressed, etc.)
- Handle state transitions smoothly
- Reset state appropriately
- Consider animation needs

## 🔄 Pull Request Process

1. **Before submitting**:
   - Ensure code compiles without warnings
   - Test thoroughly with the showcase
   - Update documentation
   - Follow coding standards

2. **PR Description**:
   - Clearly describe what the PR does
   - Reference related issues
   - Include screenshots for visual changes
   - List any breaking changes

3. **Review Process**:
   - Maintainers will review your code
   - Address feedback promptly
   - Be open to suggestions and changes
   - Update your branch as needed

## 🏷️ Commit Message Guidelines

Use clear, descriptive commit messages:

- `Add: new component or feature`
- `Fix: bug description`
- `Update: existing feature improvement`
- `Docs: documentation changes`
- `Style: formatting, no code change`
- `Refactor: code restructuring`

## ❓ Questions?

- Open an issue for questions about contributing
- Check existing issues and documentation first
- Be respectful and patient
- Help others when you can

## 🙏 Recognition

Contributors will be recognized in:
- The project README
- Release notes for significant contributions
- The project's contributor list

Thank you for helping make MarkUp UI better! 🎉