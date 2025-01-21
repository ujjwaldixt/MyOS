#ifndef __MYOS__DRIVERS__VGA_H                               // Header guard to prevent multiple inclusion
#define __MYOS__DRIVERS__VGA_H

#include <common/types.h>                                    // Provides integral type definitions (e.g., uint8_t, uint32_t)
#include <hardwarecommunication/port.h>                      // I/O port abstractions for reading/writing to VGA hardware
#include <drivers/driver.h>                                  // Base classes for drivers (not directly used here, but often included for consistency)

namespace myos
{
    namespace drivers
    {
        /*
         * The VideoGraphicsArray class encapsulates control and interaction with a VGA-compatible graphics device.
         * It exposes functionality to set the display mode and to draw basic graphics (pixels, rectangles).
         * Internally, it uses I/O ports to communicate with the VGA hardware registers.
         */
        class VideoGraphicsArray
        {
        protected:
            // Ports for accessing various VGA registers (miscellaneous, CRTC, sequencer, graphics controller, attribute controller).
            // These ports are used to configure the VGA mode, memory mapping, and palette.
            hardwarecommunication::Port8Bit miscPort;                      // Port for miscellaneous VGA settings
            hardwarecommunication::Port8Bit crtcIndexPort;                 // CRTC (Cathode Ray Tube Controller) index port
            hardwarecommunication::Port8Bit crtcDataPort;                  // CRTC data port (writes register values specified by crtcIndexPort)
            hardwarecommunication::Port8Bit sequencerIndexPort;            // Sequencer index port
            hardwarecommunication::Port8Bit sequencerDataPort;             // Sequencer data port (writes register values specified by sequencerIndexPort)
            hardwarecommunication::Port8Bit graphicsControllerIndexPort;   // Graphics controller index port
            hardwarecommunication::Port8Bit graphicsControllerDataPort;    // Graphics controller data port (writes register values specified by graphicsControllerIndexPort)
            hardwarecommunication::Port8Bit attributeControllerIndexPort;  // Attribute controller index port
            hardwarecommunication::Port8Bit attributeControllerReadPort;   // Attribute controller read port
            hardwarecommunication::Port8Bit attributeControllerWritePort;  // Attribute controller write port
            hardwarecommunication::Port8Bit attributeControllerResetPort;  // Attribute controller reset port
            
            /*
             * WriteRegisters:
             *  Writes a given array of register values to the VGA ports in the correct sequence.
             *  This is typically used when switching the video mode or reconfiguring VGA settings.
             *  The array format typically follows the sequence for the VGA registers: 
             *   - Miscellaneous Output
             *   - Sequencer Registers
             *   - CRTC Registers
             *   - Graphics Controller Registers
             *   - Attribute Controller Registers
             */
            void WriteRegisters(common::uint8_t* registers);

            /*
             * GetFrameBufferSegment:
             *  Determines which segment of memory is used as the frame buffer (where actual pixel data is stored).
             *  The VGA can map its video memory into different segments in the PC’s address space.
             *  Returns a pointer to the start of the current frame buffer in memory (or an offset to it).
             */
            common::uint8_t* GetFrameBufferSegment();
            
            /*
             * GetColorIndex:
             *  Given an RGB value, returns an 8-bit color index. In VGA's 256-color mode,
             *  colors are often indexed rather than stored directly as 24-bit color.
             *  The default implementation may return an index that approximates the input RGB.
             *  Derived classes or specific implementations could override this to implement 
             *  different color palettes or direct color modes.
             */
            virtual common::uint8_t GetColorIndex(common::uint8_t r, common::uint8_t g, common::uint8_t b);
            
        public:
            // Constructor initializes the VGA port objects with known I/O port numbers.
            VideoGraphicsArray();
            
            // Destructor (empty for now; could be used for cleanup if needed).
            ~VideoGraphicsArray();
            
            /*
             * SupportsMode:
             *  Checks if a given resolution (width x height) and color depth is supported by this VGA driver.
             *  Returns true if supported, false if not.
             */
            virtual bool SupportsMode(common::uint32_t width, common::uint32_t height, common::uint32_t colordepth);
            
            /*
             * SetMode:
             *  Attempts to configure the VGA hardware for the given mode (width, height, color depth).
             *  If successful, returns true. Otherwise, returns false.
             */
            virtual bool SetMode(common::uint32_t width, common::uint32_t height, common::uint32_t colordepth);
            
            /*
             * PutPixel (overload 1):
             *  Draws a pixel at coordinates (x, y) with the specified RGB color. 
             *  Internally, this is usually converted to a color index for 256-color modes.
             */
            virtual void PutPixel(common::int32_t x, common::int32_t y, common::uint8_t r, common::uint8_t g, common::uint8_t b);

            /*
             * PutPixel (overload 2):
             *  Draws a pixel at coordinates (x, y) with an already computed 8-bit colorIndex.
             *  This allows faster drawing if the color index is known beforehand.
             */
            virtual void PutPixel(common::int32_t x, common::int32_t y, common::uint8_t colorIndex);
            
            /*
             * FillRectangle:
             *  Fills a rectangular area at (x, y) with dimensions w x h using the given RGB color.
             *  Internally, each pixel within that rectangle will be set using the color index derived from (r,g,b).
             */
            virtual void FillRectangle(common::uint32_t x, common::uint32_t y, 
                                       common::uint32_t w, common::uint32_t h, 
                                       common::uint8_t r, common::uint8_t g, common::uint8_t b);

        };
        
    }
}

#endif
