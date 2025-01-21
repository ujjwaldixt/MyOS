#ifndef __MYOS__GUI__DESKTOP_H                                     // Header guard to prevent multiple inclusions
#define __MYOS__GUI__DESKTOP_H

#include <gui/widget.h>                                           // Includes the definition of Widget and CompositeWidget
#include <drivers/mouse.h>                                        // Includes MouseEventHandler interface

namespace myos
{
    namespace gui
    {
        /*
         * Desktop:
         *  This class represents the top-level graphical user interface element.
         *  It is derived from CompositeWidget, meaning it can contain other widgets,
         *  and it also implements the MouseEventHandler to directly handle mouse inputs.
         */
        class Desktop : public CompositeWidget, 
                        public myos::drivers::MouseEventHandler
        {
        protected:
            // Tracks the current mouse position within the desktop (x and y coordinates).
            common::uint32_t MouseX;
            common::uint32_t MouseY;
            
        public:
            /*
             * Constructor: 
             *  Initializes the desktop with a given width (w) and height (h), 
             *  and a background color specified by (r, g, b).
             *  The desktop is treated as a container widget that can host multiple child widgets.
             */
            Desktop(common::int32_t w, common::int32_t h,
                    common::uint8_t r, common::uint8_t g, common::uint8_t b);

            // Destructor (empty for now but can be used to clean up resources).
            ~Desktop();
            
            /*
             * Draw:
             *  Renders the desktop and all its child widgets onto the provided GraphicsContext (gc).
             *  This includes filling the background, then recursively drawing child widgets.
             */
            void Draw(common::GraphicsContext* gc);
            
            /*
             * OnMouseDown:
             *  Called when a mouse button is pressed. 'button' indicates which button was pressed.
             *  In a desktop, this can be used to notify child widgets or for internal desktop behavior.
             */
            void OnMouseDown(myos::common::uint8_t button);

            /*
             * OnMouseUp:
             *  Called when a mouse button is released. 'button' indicates which button was released.
             *  Similar to OnMouseDown, this can trigger widget actions or system events.
             */
            void OnMouseUp(myos::common::uint8_t button);

            /*
             * OnMouseMove:
             *  Called when the mouse is moved. The parameters x and y indicate how much the mouse
             *  moved relative to its last position. The desktop updates the MouseX and MouseY
             *  coordinates and may forward the movement to child widgets for interaction (e.g., dragging).
             */
            void OnMouseMove(int x, int y);
        };
        
    }
}

#endif // __MYOS__GUI__DESKTOP_H
