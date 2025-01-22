#include <gui/window.h>

/*
 * Namespaces:
 *   - myos::common: provides basic integer types.
 *   - myos::gui: provides the Widget, CompositeWidget, and Window classes for GUI construction.
 */
using namespace myos::common;
using namespace myos::gui;

/*
 * Window:
 *  A specialized CompositeWidget that can be dragged around with the mouse when the user clicks 
 *  and holds the left mouse button (button == 1). 
 *  The 'Dragging' boolean flag tracks if the user is currently dragging this window.
 */

/*
 * Constructor:
 *   - Forwards the arguments to the CompositeWidget constructor, which sets up position, size, and background color.
 *   - Initializes 'Dragging' to false, indicating the window isn't being moved initially.
 */
Window::Window(Widget* parent,
               common::int32_t x, common::int32_t y, 
               common::int32_t w, common::int32_t h,
               common::uint8_t r, common::uint8_t g, common::uint8_t b)
: CompositeWidget(parent, x, y, w, h, r, g, b)
{
    Dragging = false;
}

/*
 * Destructor:
 *  - Currently does nothing special. If the window allocated additional resources, 
 *    they would be released here.
 */
Window::~Window()
{
}

/*
 * OnMouseDown:
 *  - Called when a mouse button is pressed on this window. 
 *  - If button == 1 (left mouse button), sets Dragging = true, 
 *    indicating the user might move the window by dragging.
 *  - Forwards the event to the CompositeWidget for any child widgets to receive the click.
 */
void Window::OnMouseDown(common::int32_t x, common::int32_t y, common::uint8_t button)
{
    // If left button is pressed, enter dragging mode
    Dragging = (button == 1);
    
    // Pass the event to child widgets if needed
    CompositeWidget::OnMouseDown(x, y, button);
}

/*
 * OnMouseUp:
 *  - Called when a mouse button is released. 
 *  - Sets Dragging = false, ending any active window dragging.
 *  - Forwards the event to the CompositeWidget for child widgets to process.
 */
void Window::OnMouseUp(common::int32_t x, common::int32_t y, common::uint8_t button)
{
    Dragging = false;
    CompositeWidget::OnMouseUp(x, y, button);
}

/*
 * OnMouseMove:
 *  - Called whenever the mouse moves while over this window. 
 *  - If Dragging is true, we update the window’s position by the offset 
 *    (newx - oldx, newy - oldy).
 *  - Then, we call CompositeWidget::OnMouseMove so child widgets can handle the movement as well.
 */
void Window::OnMouseMove(common::int32_t oldx, common::int32_t oldy, 
                         common::int32_t newx, common::int32_t newy)
{
    // If we're dragging, shift the window by the mouse delta
    if(Dragging)
    {
        this->x += newx - oldx;
        this->y += newy - oldy;
    }
    
    // Let children know the mouse moved
    CompositeWidget::OnMouseMove(oldx, oldy, newx, newy);
}
