#include <gui/desktop.h>

/*
 * Namespace usage:
 *   - myos: main namespace for our OS
 *   - myos::common: contains integral types (int32_t, uint8_t, etc.)
 *   - myos::gui: GUI components like Desktop, Widgets, etc.
 */
using namespace myos;
using namespace myos::common;
using namespace myos::gui;


/*
 * Desktop:
 *  A specialized CompositeWidget that implements MouseEventHandler, representing
 *  the top-level graphical container (the desktop) in our GUI. It manages child
 *  widgets and tracks mouse position for rendering a cursor.
 */

/*
 * Constructor:
 *   - w, h: width and height of the desktop
 *   - (r,g,b): background color
 *   - Calls the CompositeWidget constructor with no parent (0) at coordinate (0,0).
 *   - Initializes the mouse position at the center of the desktop.
 */
Desktop::Desktop(common::int32_t w, common::int32_t h,
                 common::uint8_t r, common::uint8_t g, common::uint8_t b)
: CompositeWidget(0, 0, 0, w, h, r, g, b), 
  MouseEventHandler()
{
    MouseX = w/2;
    MouseY = h/2;
}

/*
 * Destructor:
 *  - Currently empty; the CompositeWidget destructor handles removing child widgets.
 */
Desktop::~Desktop()
{
}

/*
 * Draw:
 *  - First calls CompositeWidget::Draw to render the desktop background 
 *    and its child widgets.
 *  - Then draws a simple cross for the mouse cursor at (MouseX, MouseY), using white pixels.
 *  - The cross is 4 pixels in each direction from the center.
 */
void Desktop::Draw(common::GraphicsContext* gc)
{
    // Draw the background and child widgets
    CompositeWidget::Draw(gc);

    // Draw a mouse cursor as a cross
    for(int i = 0; i < 4; i++)
    {
        gc->PutPixel(MouseX - i, MouseY,   0xFF, 0xFF, 0xFF);
        gc->PutPixel(MouseX + i, MouseY,   0xFF, 0xFF, 0xFF);
        gc->PutPixel(MouseX,   MouseY - i, 0xFF, 0xFF, 0xFF);
        gc->PutPixel(MouseX,   MouseY + i, 0xFF, 0xFF, 0xFF);
    }
}
            
/*
 * OnMouseDown:
 *  - Called when a mouse button is pressed.
 *  - The desktop forwards the click event to its child widgets by calling 
 *    CompositeWidget::OnMouseDown, passing the current mouse coordinates.
 */
void Desktop::OnMouseDown(myos::common::uint8_t button)
{
    CompositeWidget::OnMouseDown(MouseX, MouseY, button);
}

/*
 * OnMouseUp:
 *  - Called when a mouse button is released.
 *  - Forwards the event similarly, passing the desktop's current mouse position.
 */
void Desktop::OnMouseUp(myos::common::uint8_t button)
{
    CompositeWidget::OnMouseUp(MouseX, MouseY, button);
}

/*
 * OnMouseMove:
 *  - Called whenever the mouse moves.
 *  - x, y are the mouse movement offsets (commonly from the PS/2 mouse driver).
 *  - Here, we scale down the movement by dividing by 4, providing "slower" cursor movement.
 *  - We compute newMouseX/newMouseY, clamping them to [0, w-1] and [0, h-1] to avoid going off-screen.
 *  - We notify child widgets by calling CompositeWidget::OnMouseMove with old and new coordinates.
 *  - Finally, we update MouseX and MouseY with the new position.
 */
void Desktop::OnMouseMove(int x, int y)
{
    // Adjust cursor speed (divide movement by 4)
    x /= 4;
    y /= 4;
    
    // Calculate new X, clamp within desktop width
    int32_t newMouseX = MouseX + x;
    if(newMouseX < 0) 
        newMouseX = 0;
    if(newMouseX >= w) 
        newMouseX = w - 1;
    
    // Calculate new Y, clamp within desktop height
    int32_t newMouseY = MouseY + y;
    if(newMouseY < 0) 
        newMouseY = 0;
    if(newMouseY >= h) 
        newMouseY = h - 1;
    
    // Notify child widgets that the mouse moved (from old coords to new coords)
    CompositeWidget::OnMouseMove(MouseX, MouseY, newMouseX, newMouseY);
    
    // Update the desktop's stored mouse position
    MouseX = newMouseX;
    MouseY = newMouseY;
}
