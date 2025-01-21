#ifndef __MYOS__COMMON__TYPES_H               // Header guard to prevent multiple inclusions of this file
#define __MYOS__COMMON__TYPES_H

namespace myos
{
    namespace common
    {
        // The following typedef statements give names to built-in types.
        // They mirror common fixed-width integer definitions provided by stdint.h in C/C++.

        typedef char                     int8_t;   // Defines a signed 8-bit integer type
        typedef unsigned char           uint8_t;  // Defines an unsigned 8-bit integer type
        typedef short                   int16_t;  // Defines a signed 16-bit integer type
        typedef unsigned short         uint16_t;  // Defines an unsigned 16-bit integer type
        typedef int                     int32_t;  // Defines a signed 32-bit integer type
        typedef unsigned int           uint32_t;  // Defines an unsigned 32-bit integer type
        typedef long long int           int64_t;  // Defines a signed 64-bit integer type
        typedef unsigned long long int uint64_t;  // Defines an unsigned 64-bit integer type
    
        // These typedef statements provide convenient names for common usage.
        
        typedef const char*              string;   // Represents a pointer to a constant character array (C-style string)
        typedef uint32_t                 size_t;   // Defines a generic size type, typically used for memory sizes
    }
}
    
#endif  // End of header guard
