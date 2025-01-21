#ifndef __MYOS__COMMON__GRAPHICSCONTEXT_H  // Header guard to prevent multiple inclusions of this file
#define __MYOS__COMMON__GRAPHICSCONTEXT_H  // Defines the unique macro for this header file

#include <drivers/vga.h>  // Includes the definition/declaration of the VideoGraphicsArray class

namespace myos
{
    namespace common
    {
        // This typedef creates a shorter alias 'GraphicsContext' for 'myos::drivers::VideoGraphicsArray'.
        // It allows code to refer to VideoGraphicsArray functionality using 'GraphicsContext'.
        typedef myos::drivers::VideoGraphicsArray GraphicsContext;
    }
}

#endif  // End of header guard
