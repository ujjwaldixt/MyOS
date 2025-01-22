#include <gui/widget.h>

/*
 * The namespaces:
 *   - myos::common: contains integer types and other common definitions
 *   - myos::gui: contains GUI-related classes (Widget, CompositeWidget, etc.)
 */
using namespace myos::common;
using namespace myos::gui;


/*
 * --------------------------------------------------------------------------
 * Widget Class
 * --------------------------------------------------------------------------
 *
 * A basic GUI element with position (x, y), size (w, h), and a background color (r, g, b).
 * It can handle mouse and keyboard events, though by default, most handlers do nothing.
 * A Widget can be part of a parent-child hierarchy, where coordinates are relative 
 * to the parent.
 */

/*
 * Constructor:
 *   - parent: the parent Widget (or null if this is a top-level).
 *   - (x, y): position of the widget relative to its parent.
 *   - (w, h): width and height of the widget.
 *   - (r, g, b): the widget’s background color.
 *
 *   Also sets Focussable = true by default, meaning this widget can be 
 *   given focus for keyboard input.
 */
Widget::Widget(Widget* parent, int32_t x, int32_t y, int32_t w, int32_t h,
               uint8_t r, uint8_t g, uint8_t b)
: KeyboardEventHandler()  // Inherits from a class that can handle keyboard events
{
    this->parent = parent;
    this->x = x;
    this->y = y;
    this->w = w;
    this->h = h;
    this->r = r;
    this->g = g;
    this->b = b;
    this->Focussable = true;
}

/*
 * Destructor: Currently does nothing. 
 * If the widget dynamically allocated resources, they would be cleaned up here.
 */
Widget::~Widget()
{
}

/*
 * GetFocus:
 *  - Called when the system or user attempts to give this widget focus. 
 *  - By default, passes the request up to the parent. 
 *    The parent might track which child has focus.
 */
void Widget::GetFocus(Widget* widget)
{
    if(parent != 0)
        parent->GetFocus(widget);
}

/*
 * ModelToScreen:
 *  - Translates local widget coordinates into screen coordinates by recursively adding 
 *    the positions of parent widgets. 
 *  - The updated values of x and y become absolute coordinates on the screen.
 */
void Widget::ModelToScreen(int32_t &x, int32_t &y)
{
    if(parent != 0)
        parent->ModelToScreen(x, y);
    x += this->x;
    y += this->y;
}

/*
 * Draw:
 *  - Draws this widget by filling its rectangle with its (r, g, b) color.
 *  - The coordinates are converted to absolute screen coordinates by calling ModelToScreen.
 *  - A GraphicsContext (gc) provides the drawing functions (FillRectangle, PutPixel, etc.).
 */
void Widget::Draw(GraphicsContext* gc)
{
    int X = 0;
    int Y = 0;
    ModelToScreen(X, Y);
    gc->FillRectangle(X, Y, w, h, r, g, b);
}

/*
 * OnMouseDown:
 *  - Triggered when a mouse button is pressed within the widget’s area.
 *  - By default, if the widget is focussable, requests focus for this widget.
 */
void Widget::OnMouseDown(int32_t x, int32_t y, uint8_t button)
{
    if(Focussable)
        GetFocus(this);
}

/*
 * ContainsCoordinate:
 *  - Checks if (x, y) lies within this widget’s bounds. Coordinates are relative 
 *    to the widget’s parent, so typically the caller subtracts the parent’s position.
 */
bool Widget::ContainsCoordinate(int32_t x, int32_t y)
{
    return (this->x <= x) && (x < this->x + this->w)
        && (this->y <= y) && (y < this->y + this->h);
}

/*
 * OnMouseUp:
 *  - Triggered when a mouse button is released. 
 *  - The default implementation does nothing.
 */
void Widget::OnMouseUp(int32_t x, int32_t y, uint8_t button)
{
}

/*
 * OnMouseMove:
 *  - Triggered when the mouse moves within or out of this widget. 
 *  - Default does nothing. Overridden in subclasses that need special drag/move behavior.
 */
void Widget::OnMouseMove(int32_t oldx, int32_t oldy, int32_t newx, int32_t newy)
{
}


/*
 * --------------------------------------------------------------------------
 * CompositeWidget Class
 * --------------------------------------------------------------------------
 *
 * A widget that can contain child widgets. Coordinates of children are relative 
 * to this widget's position. This class manages focus, drawing, and event dispatch 
 * to the appropriate child.
 */

/*
 * Constructor:
 *   - Forwards arguments to the base Widget constructor.
 *   - Initializes no focussedChild and zero children.
 */
CompositeWidget::CompositeWidget(Widget* parent,
                                 int32_t x, int32_t y, 
                                 int32_t w, int32_t h,
                                 uint8_t r, uint8_t g, uint8_t b)
: Widget(parent, x, y, w, h, r, g, b)
{
    focussedChild = 0;
    numChildren = 0;
}

/*
 * Destructor: 
 *  - Here, if the widget owned children that were dynamically allocated, 
 *    we might free them. Currently empty.
 */
CompositeWidget::~CompositeWidget()
{
}

/*
 * GetFocus:
 *  - Called when a child widget (or this widget itself) needs focus.
 *  - The composite widget stores 'widget' as the new focussedChild.
 *  - Passes focus up to the parent as well, so the parent can track which child subtree has focus.
 */
void CompositeWidget::GetFocus(Widget* widget)
{
    this->focussedChild = widget;
    if(parent != 0)
        parent->GetFocus(this);
}

/*
 * AddChild:
 *  - Adds a new child to the 'children' array, up to 100 children.
 *  - Returns false if there's no space left, otherwise true.
 */
bool CompositeWidget::AddChild(Widget* child)
{
    if(numChildren >= 100)
        return false;
    children[numChildren++] = child;
    return true;
}

/*
 * Draw:
 *  - First draws itself (background, etc.) via Widget::Draw.
 *  - Then iterates through the children in reverse order, drawing each child on top.
 *  - The reverse order ensures the first child in the array is drawn last (potentially 
 *    ensuring it’s "on top" if overlapping).
 */
void CompositeWidget::Draw(GraphicsContext* gc)
{
    Widget::Draw(gc);
    for(int i = numChildren - 1; i >= 0; --i)
        children[i]->Draw(gc);
}

/*
 * OnMouseDown:
 *  - Called when a mouse button is pressed, with coordinates (x, y) relative to the parent.
 *  - We check each child in order. The first child that contains (x - this->x, y - this->y) 
 *    gets the OnMouseDown event. We break after dispatching to one child.
 */
void CompositeWidget::OnMouseDown(int32_t x, int32_t y, uint8_t button)
{
    for(int i = 0; i < numChildren; ++i)
        if(children[i]->ContainsCoordinate(x - this->x, y - this->y))
        {
            children[i]->OnMouseDown(x - this->x, y - this->y, button);
            break;
        }
}

/*
 * OnMouseUp:
 *  - Similar to OnMouseDown, passes the event to the first child containing the coordinate.
 */
void CompositeWidget::OnMouseUp(int32_t x, int32_t y, uint8_t button)
{
    for(int i = 0; i < numChildren; ++i)
        if(children[i]->ContainsCoordinate(x - this->x, y - this->y))
        {
            children[i]->OnMouseUp(x - this->x, y - this->y, button);
            break;
        }
}

/*
 * OnMouseMove:
 *  - We check which child (if any) contained the old mouse coordinates, 
 *    and pass the movement to it. Then we check if a different child (or the same) 
 *    contains the new coordinates, passing it the movement if needed.
 *  - This allows widgets to handle cases like mouse entering or leaving them.
 */
void CompositeWidget::OnMouseMove(int32_t oldx, int32_t oldy, int32_t newx, int32_t newy)
{
    int firstchild = -1;
    
    // Find child under the old mouse position
    for(int i = 0; i < numChildren; ++i)
        if(children[i]->ContainsCoordinate(oldx - this->x, oldy - this->y))
        {
            children[i]->OnMouseMove(oldx - this->x, oldy - this->y, 
                                     newx - this->x, newy - this->y);
            firstchild = i;
            break;
        }

    // Find child under the new mouse position
    for(int i = 0; i < numChildren; ++i)
        if(children[i]->ContainsCoordinate(newx - this->x, newy - this->y))
        {
            // If it's a different child than before, pass the event too 
            if(firstchild != i)
                children[i]->OnMouseMove(oldx - this->x, oldy - this->y, 
                                         newx - this->x, newy - this->y);
            break;
        }
}

/*
 * OnKeyDown:
 *  - If there's a focussed child, forward the keystroke to that child's OnKeyDown.
 */
void CompositeWidget::OnKeyDown(char str)
{
    if(focussedChild != 0)
        focussedChild->OnKeyDown(str);
}

/*
 * OnKeyUp:
 *  - Similarly, forward the key release event if a child is focussed.
 */
void CompositeWidget::OnKeyUp(char str)
{
    if(focussedChild != 0)
        focussedChild->OnKeyUp(str);
}
