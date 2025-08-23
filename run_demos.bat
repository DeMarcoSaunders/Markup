@echo off
echo ========================================
echo    Markup UI - Component Showcase
echo ========================================
echo.
echo Available demos:
echo 1. demo.exe - Comprehensive component showcase
echo 2. showcase.exe - Basic component examples  
echo 3. test_dropdown.exe - Dropdown component test
echo.
echo Press a number to run the corresponding demo:
echo 1 - Comprehensive Demo (recommended)
echo 2 - Basic Showcase
echo 3 - Dropdown Test
echo q - Quit
echo.

:input_loop
set /p choice="Enter your choice (1-3 or q): "

if "%choice%"=="1" (
    echo.
    echo Starting comprehensive demo...
    echo This demo showcases all available components with interactive examples.
    echo.
    .\Debug\demo.exe
    goto end
)

if "%choice%"=="2" (
    echo.
    echo Starting basic showcase...
    echo This demo shows basic component usage.
    echo.
    .\Debug\showcase.exe
    goto end
)

if "%choice%"=="3" (
    echo.
    echo Starting dropdown test...
    echo This demo focuses on dropdown component features.
    echo.
    .\Debug\test_dropdown.exe
    goto end
)

if "%choice%"=="q" (
    echo Goodbye!
    goto end
)

echo Invalid choice. Please enter 1, 2, 3, or q.
goto input_loop

:end
pause 