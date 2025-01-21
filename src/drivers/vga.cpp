#include <drivers/vga.h>

/*
 * We place our code in the 'myos::drivers' namespace hierarchy:
 *   - myos::common contains definitions for basic types (e.g., uint8_t)
 *   - myos::drivers groups driver-related classes
 */
using namespace myos::common;
using namespace myos::drivers;

/*
 * ---------------------------------------------------------------------------------
 * VideoGraphicsArray Class
 * ---------------------------------------------------------------------------------
 *
 * This class encapsulates control of a standard VGA-compatible card. It allows us 
 * to configure the VGA card to a particular mode (e.g., 320x200x256-color mode) and 
 * provides basic drawing primitives (PutPixel, FillRectangle). It interacts with 
 * specific VGA registers via port I/O.
 */

/*
 * Constructor:
 *   - Initializes all the port objects (misc, CRTC, sequencer, graphics controller, 
 *     attribute controller). These ports correspond to specific I/O addresses used 
 *     by the VGA hardware.
 */
VideoGraphicsArray::VideoGraphicsArray() 
  : miscPort(0x3c2),
    crtcIndexPort(0x3d4),
    crtcDataPort(0x3d5),
    sequencerIndexPort(0x3c4),
    sequencerDataPort(0x3c5),
    graphicsControllerIndexPort(0x3ce),
    graphicsControllerDataPort(0x3cf),
    attributeControllerIndexPort(0x3c0),
    attributeControllerReadPort(0x3c1),
    attributeControllerWritePort(0x3c0),
    attributeControllerResetPort(0x3da)
{
}

/*
 * Destructor:
 *   - Currently does nothing. If allocated resources needed to be released, 
 *     that logic would be added here.
 */
VideoGraphicsArray::~VideoGraphicsArray()
{
}


/*
 * WriteRegisters:
 *   - Writes an array of VGA register values to the various VGA register ports 
 *     in the correct sequence:
 *       1) Miscellaneous register
 *       2) Sequencer registers
 *       3) CRTC (Cathode Ray Tube Controller) registers
 *       4) Graphics controller registers
 *       5) Attribute controller registers
 *
 *   - The function carefully manages certain bits in the CRTC registers (especially 
 *     register #0x03 and #0x11) to allow writing to them (locking/unlocking).
 */
void VideoGraphicsArray::WriteRegisters(uint8_t* registers)
{
    // Write the Misc register
    miscPort.Write(*(registers++));
    
    // Write 5 Sequencer registers
    for(uint8_t i = 0; i < 5; i++)
    {
        sequencerIndexPort.Write(i);
        sequencerDataPort.Write(*(registers++));
    }
    
    // Unlock certain CRTC registers (#0x03 and #0x11) to allow writes
    crtcIndexPort.Write(0x03);
    crtcDataPort.Write(crtcDataPort.Read() | 0x80);
    crtcIndexPort.Write(0x11);
    crtcDataPort.Write(crtcDataPort.Read() & ~0x80);
    
    // The code modifies the in-memory copy of these register values as well
    registers[0x03] = registers[0x03] | 0x80;
    registers[0x11] = registers[0x11] & ~0x80;
    
    // Write 25 CRTC registers
    for(uint8_t i = 0; i < 25; i++)
    {
        crtcIndexPort.Write(i);
        crtcDataPort.Write(*(registers++));
    }
    
    // Write 9 Graphics controller registers
    for(uint8_t i = 0; i < 9; i++)
    {
        graphicsControllerIndexPort.Write(i);
        graphicsControllerDataPort.Write(*(registers++));
    }
    
    // Write 21 Attribute controller registers
    for(uint8_t i = 0; i < 21; i++)
    {
        // Reading from 'attributeControllerResetPort' toggles a flip-flop, 
        // allowing us to write attribute index/values properly.
        attributeControllerResetPort.Read();  
        attributeControllerIndexPort.Write(i);
        attributeControllerWritePort.Write(*(registers++));
    }
    
    // Send the 'exit attribute controller' command by writing 0x20 
    // to the attribute controller index port (after a read).
    attributeControllerResetPort.Read();
    attributeControllerIndexPort.Write(0x20);
}

/*
 * SupportsMode:
 *   - Returns true if the requested resolution (width x height) and color depth 
 *     matches 320x200x8-bit color (256 colors). This is a minimal check to confirm 
 *     we only support this one specific mode in this implementation.
 */
bool VideoGraphicsArray::SupportsMode(uint32_t width, uint32_t height, uint32_t colordepth)
{
    return width == 320 && height == 200 && colordepth == 8;
}

/*
 * SetMode:
 *   - Attempts to configure the VGA card for a given resolution and color depth.
 *   - Here, we check if the mode is 320x200x8 and, if so, load 'g_320x200x256' 
 *     register settings. If the mode is unsupported, returns false.
 *   - 'g_320x200x256' is a register state table that programs the VGA into this mode.
 */
bool VideoGraphicsArray::SetMode(uint32_t width, uint32_t height, uint32_t colordepth)
{
    if(!SupportsMode(width, height, colordepth))
        return false;
    
    // Register values for 320x200x256 mode
    unsigned char g_320x200x256[] =
    {
        /* MISC */
            0x63,
        /* SEQ */
            0x03, 0x01, 0x0F, 0x00, 0x0E,
        /* CRTC */
            0x5F, 0x4F, 0x50, 0x82, 0x54, 0x80, 0xBF, 0x1F,
            0x00, 0x41, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
            0x9C, 0x0E, 0x8F, 0x28, 0x40, 0x96, 0xB9, 0xA3,
            0xFF,
        /* GC */
            0x00, 0x00, 0x00, 0x00, 0x00, 0x40, 0x05, 0x0F,
            0xFF,
        /* AC */
            0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07,
            0x08, 0x09, 0x0A, 0x0B, 0x0C, 0x0D, 0x0E, 0x0F,
            0x41, 0x00, 0x0F, 0x00, 0x00
    };
    
    // Write the register table to the VGA hardware
    WriteRegisters(g_320x200x256);
    return true;
}

/*
 * GetFrameBufferSegment:
 *   - Determines which segment of memory is currently mapped as the VGA frame buffer. 
 *   - Reads register #0x06 (Graphics controller) to find bits [3:2], indicating the memory segment.
 *   - Returns a pointer (a CPU address) for where we can write pixel data.
 */
uint8_t* VideoGraphicsArray::GetFrameBufferSegment()
{
    graphicsControllerIndexPort.Write(0x06);
    uint8_t segmentNumber = graphicsControllerDataPort.Read() & (3 << 2);
    
    switch(segmentNumber)
    {
        default:
        case 0 << 2: return (uint8_t*)0x00000;  // Uncommon
        case 1 << 2: return (uint8_t*)0xA0000; // Typical for mode 0x13 (320x200)
        case 2 << 2: return (uint8_t*)0xB0000; // Monochrome text?
        case 3 << 2: return (uint8_t*)0xB8000; // Color text mode region
    }
}

/*
 * PutPixel (overload #1):
 *   - Writes an 8-bit color index into the VGA frame buffer at (x,y).
 *   - If x or y is out of bounds for 320x200, we simply return (no drawing).
 */
void VideoGraphicsArray::PutPixel(int32_t x, int32_t y, uint8_t colorIndex)
{
    if(x < 0 || 320 <= x
    || y < 0 || 200 <= y)
        return;

    // Compute the address of the pixel in the linear 320*200 buffer
    uint8_t* pixelAddress = GetFrameBufferSegment() + 320*y + x;
    *pixelAddress = colorIndex;
}

/*
 * GetColorIndex:
 *   - A simple color lookup that returns a VGA palette index for a given RGB triple.
 *   - This is a minimal approach for demonstration, translating a few predefined colors 
 *     to specific 8-bit palette indices. Others map to black (0x00).
 */
uint8_t VideoGraphicsArray::GetColorIndex(uint8_t r, uint8_t g, uint8_t b)
{
    if(r == 0x00 && g == 0x00 && b == 0x00) return 0x00; // black
    if(r == 0x00 && g == 0x00 && b == 0xA8) return 0x01; // blue
    if(r == 0x00 && g == 0xA8 && b == 0x00) return 0x02; // green
    if(r == 0xA8 && g == 0x00 && b == 0x00) return 0x04; // red
    if(r == 0xFF && g == 0xFF && b == 0xFF) return 0x3F; // white
    return 0x00; // default to black for unknown colors
}

/*
 * PutPixel (overload #2):
 *   - Draws a pixel at (x,y) in the color derived from the RGB triple (r,g,b).
 *   - Internally, we map (r,g,b) to a palette index using GetColorIndex().
 */
void VideoGraphicsArray::PutPixel(int32_t x, int32_t y,  uint8_t r, uint8_t g, uint8_t b)
{
    PutPixel(x, y, GetColorIndex(r, g, b));
}

/*
 * FillRectangle:
 *   - Fills a rectangular region at (x,y) with width 'w' and height 'h' 
 *     with a given RGB color.
 *   - Internally, calls PutPixel on each pixel in that region.
 */
void VideoGraphicsArray::FillRectangle(uint32_t x, uint32_t y, uint32_t w, uint32_t h,
                                       uint8_t r, uint8_t g, uint8_t b)
{
    for(int32_t Y = y; Y < (int32_t)(y + h); Y++)
        for(int32_t X = x; X < (int32_t)(x + w); X++)
            PutPixel(X, Y, r, g, b);
}
