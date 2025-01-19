#ifndef __MYOS__GDT_H
#define __MYOS__GDT_H

#include <common/types.h>

namespace myos
{
    // The GlobalDescriptorTable (GDT) class is responsible for defining and managing
    // the GDT, which is used by the CPU to handle memory segmentation. The GDT contains
    // descriptors that define the properties (base, limit, access rights) of memory segments.

    class GlobalDescriptorTable
    {
        public:

            // The SegmentDescriptor class represents a single segment descriptor
            // in the GDT. Each descriptor defines a segment's base address, limit,
            // and access rights/type. It is packed to ensure no padding is added by the compiler.
            class SegmentDescriptor
            {
                private:
                    myos::common::uint16_t limit_lo; // Lower 16 bits of the segment limit
                    myos::common::uint16_t base_lo; // Lower 16 bits of the base address
                    myos::common::uint8_t base_hi;  // Next 8 bits of the base address
                    myos::common::uint8_t type;     // Access rights and segment type
                    myos::common::uint8_t limit_hi; // Higher 4 bits of the segment limit and flags
                    myos::common::uint8_t base_vhi; // Final 8 bits of the base address

                public:
                    // Constructor to initialize the segment descriptor with the given
                    // base address, limit, and type. The type specifies the segment's
                    // access rights and purpose (e.g., code, data, or system segment).
                    SegmentDescriptor(myos::common::uint32_t base, myos::common::uint32_t limit, myos::common::uint8_t type);

                    // Returns the full 32-bit base address of the segment.
                    myos::common::uint32_t Base();

                    // Returns the full 32-bit limit of the segment.
                    myos::common::uint32_t Limit();
            } __attribute__((packed)); // Ensures the structure is tightly packed with no padding.

        private:
            // Predefined segment descriptors for the GDT.
            SegmentDescriptor nullSegmentSelector;   // Null segment (required by x86 architecture)
            SegmentDescriptor unusedSegmentSelector; // Unused segment for alignment
            SegmentDescriptor codeSegmentSelector;   // Code segment (executable memory)
            SegmentDescriptor dataSegmentSelector;   // Data segment (read/write memory)

        public:

            // Constructor to initialize the GDT and its segment descriptors.
            GlobalDescriptorTable();

            // Destructor to clean up any resources, if necessary.
            ~GlobalDescriptorTable();

            // Returns the selector for the code segment.
            // This is used to load the CS (Code Segment) register during runtime.
            myos::common::uint16_t CodeSegmentSelector();

            // Returns the selector for the data segment.
            // This is used to load the DS (Data Segment) register during runtime.
            myos::common::uint16_t DataSegmentSelector();
    };

}
    
#endif
