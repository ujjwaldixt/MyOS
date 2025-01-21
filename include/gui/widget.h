#ifndef __MYOS__GUI__WIDGET_H                     // Header guard to prevent multiple inclusions
#define __MYOS__GUI__WIDGET_H

#include <common/types.h>                         // Provides fixed-size integer types (int32_t, uint8_t, etc.)
#include <common/graphicscontext.h>               // Provides the GraphicsContext class used for drawing
#include <drivers/keyboard.h>                     // Provides the KeyboardEventHandler interface

namespace myos
{
    namespace gui
    {
        /*
         * Widget:
         *  Represents a basic graphical user interface element. 
         *  It can handle keyboard events (by inheriting from KeyboardEventHandler) 
         *  and basic mouse interaction methods. 
         *  Widgets can be nested hierarchically by specifying a parent widget.
         */
        class Widget : public myos::drivers::KeyboardEventHandler
        {
        protected:
            // Pointer to the parent widget. If null, this widget is at the top-level.
            Widget* parent;

            // Position (x, y) and size (w, h) of the widget.
            // Coordinates are typically relative to the parent widget's coordinate space.
            common::int32_t x;
            common::int32_t y;
            common::int32_t w;
            common::int32_t h;
            
            // Color components used to draw the widget background or other visual elements.
            common::uint8_t r;  
            common::uint8_t g;
            common::uint8_t b;

            // Indicates if the widget can receive focus for keyboard input.
            bool Focussable;

        public:
            /*
             * Constructor:
             *   - parent: the parent widget (or nullptr if this is a top-level widget)
             *   - x, y: the widget's position relative to its parent
             *   - w, h: the widget's width and height
             *   - r, g, b: the widget's color (RGB)
             */
            Widget(Widget* parent,
                   common::int32_t x, common::int32_t y, common::int32_t w, common::int32_t h,
                   common::uint8_t r, common::uint8_t g, common::uint8_t b);

            // Destructor (empty by default, could be used for resource cleanup).
            ~Widget();
            
            /*
             * GetFocus:
             *  Requests that the given widget receives focus for input events (e.g., keyboard events).
             *  This can change internal state to track which widget is currently focussed.
             */
            virtual void GetFocus(Widget* widget);

            /*
             * ModelToScreen:
             *  Translates local widget coordinates (relative to this widget’s origin) into 
             *  screen/global coordinates by walking up the parent hierarchy.
             */
            virtual void ModelToScreen(common::int32_t &x, common::int32_t &y);

            /*
             * ContainsCoordinate:
             *  Checks if a given (x, y) coordinate (in this widget’s parent's coordinate space)
             *  lies within this widget. Returns true if inside, false otherwise.
             */
            virtual bool ContainsCoordinate(common::int32_t x, common::int32_t y);
            
            /*
             * Draw:
             *  Called to render the widget onto the provided GraphicsContext. 
             *  By default, it might fill a rectangle with the widget’s color.
             */
            virtual void Draw(common::GraphicsContext* gc);

            /*
             * OnMouseDown:
             *  Called when a mouse button is pressed within the widget. 
             *  Coordinates x, y are typically relative to the widget’s own origin.
             */
            virtual void OnMouseDown(common::int32_t x, common::int32_t y, common::uint8_t button);

            /*
             * OnMouseUp:
             *  Called when a mouse button is released within the widget.
             */
            virtual void OnMouseUp(common::int32_t x, common::int32_t y, common::uint8_t button);

            /*
             * OnMouseMove:
             *  Called when the mouse moves. 
             *  oldx, oldy: the previous mouse position relative to the widget
             *  newx, newy: the new mouse position relative to the widget
             */
            virtual void OnMouseMove(common::int32_t oldx, common::int32_t oldy, 
                                     common::int32_t newx, common::int32_t newy);
        };
        
        
        /*
         * CompositeWidget:
         *  A special type of Widget that can contain child widgets. 
         *  It manages a list of children, handles focus changes, and 
         *  delegates drawing and input events to them.
         */
        class CompositeWidget : public Widget
        {
        private:
            // A fixed array of child widget pointers. 
            // In a more dynamic implementation, this might be a vector or linked list.
            Widget* children[100];

            // The number of child widgets currently stored in 'children'.
            int numChildren;

            // Points to the child widget that currently has input focus, if any.
            Widget* focussedChild;
            
        public:
            /*
             * Constructor:
             *  Similar to Widget, but initializes data structures to hold child widgets.
             */
            CompositeWidget(Widget* parent,
                            common::int32_t x, common::int32_t y, common::int32_t w, common::int32_t h,
                            common::uint8_t r, common::uint8_t g, common::uint8_t b);

            // Destructor: may handle cleanup of child widgets if necessary.
            ~CompositeWidget();

            /*
             * GetFocus:
             *  Overrides the Widget's GetFocus method to allow child widgets to request focus. 
             *  If the child is focusable, sets 'focussedChild' to that child.
             */
            virtual void GetFocus(Widget* widget);

            /*
             * AddChild:
             *  Attempts to add a child widget to this CompositeWidget. 
             *  Returns true if successful (and there is space in 'children'), or false otherwise.
             */
            virtual bool AddChild(Widget* child);
            
            /*
             * Draw:
             *  Renders this widget, then iterates through its children and calls their Draw() methods.
             */
            virtual void Draw(common::GraphicsContext* gc);

            /*
             * OnMouseDown:
             *  Checks if the click happened on any child widget; if so, passes the event to that child. 
             *  Otherwise, processes the event for this widget itself.
             */
            virtual void OnMouseDown(common::int32_t x, common::int32_t y, common::uint8_t button);

            /*
             * OnMouseUp:
             *  Similar to OnMouseDown, passes the mouse-up event to the appropriate child widget if needed.
             */
            virtual void OnMouseUp(common::int32_t x, common::int32_t y, common::uint8_t button);

            /*
             * OnMouseMove:
             *  Called when the mouse moves within this CompositeWidget. 
             *  Passes the event to the child that previously received focus or currently contains the mouse.
             */
            virtual void OnMouseMove(common::int32_t oldx, common::int32_t oldy, 
                                     common::int32_t newx, common::int32_t newy);
            
            /*
             * OnKeyDown:
             *  If a child widget is focussed, forwards the keyboard event to that child.
             */
            virtual void OnKeyDown(char);

            /*
             * OnKeyUp:
             *  If a child widget is focussed, forwards the key release event to that child.
             */
            virtual void OnKeyUp(char);
        };
        
    }
}

#endif // __MYOS__GUI__WIDGET_H
