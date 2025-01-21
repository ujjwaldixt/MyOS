#ifndef __MYOS__GUI__WINDOW_H                     // Header guard to prevent multiple inclusions
#define __MYOS__GUI__WINDOW_H

#include <gui/widget.h>                           // Includes the base Widget and CompositeWidget classes
#include <drivers/mouse.h>                        // Includes MouseEventHandler (indirectly used by CompositeWidget)

namespace myos
{
    namespace gui
    {
        /*
         * Window:
         *  A specialized CompositeWidget that represents a movable, draggable window in a GUI.
         *  It can contain other widgets (e.g., buttons, text fields). 
         *  The 'Dragging' flag indicates whether the window is currently being moved via mouse interaction.
         */
        class Window : public CompositeWidget
        { 
        protected:
            // Keeps track of whether the user is currently dragging (moving) this window around.
            bool Dragging;
            
        public:
            /*
             * Constructor:
             *   - parent: The parent widget that contains this window (often the desktop).
             *   - x, y: The window's position relative to its parent.
             *   - w, h: The window's width and height.
             *   - r, g, b: The background color (RGB) of the window.
             *  Initializes the CompositeWidget base class with these parameters,
             *  so the window can host child widgets and handle them in a hierarchical manner.
             */
            Window(Widget* parent,
                   common::int32_t x, common::int32_t y, common::int32_t w, common::int32_t h,
                   common::uint8_t r, common::uint8_t g, common::uint8_t b);

            // Destructor for cleanup if necessary (currently empty).
            ~Window();

            /*
             * OnMouseDown:
             *  Called when a mouse button is pressed within the window's area.
             *  If the press occurs on the window’s “title bar” or a designated draggable region,
             *  sets Dragging = true to allow movement. Also propagates the event to child widgets if needed.
             */
            void OnMouseDown(common::int32_t x, common::int32_t y, common::uint8_t button);

            /*
             * OnMouseUp:
             *  Called when a mouse button is released within the window's area.
             *  Typically sets Dragging = false, ending any window dragging operation.
             *  Also passes the event to child widgets if necessary.
             */
            void OnMouseUp(common::int32_t x, common::int32_t y, common::uint8_t button);

            /*
             * OnMouseMove:
             *  Called when the mouse moves within the window (or while dragging).
             *  If Dragging is true, the window's position is updated based on the mouse's movement.
             *  Child widgets also receive this event if they need to handle mouse movement internally.
             */
            void OnMouseMove(common::int32_t oldx, common::int32_t oldy, 
                             common::int32_t newx, common::int32_t newy);
        };
    }
}

#endif // __MYOS__GUI__WINDOW_H
