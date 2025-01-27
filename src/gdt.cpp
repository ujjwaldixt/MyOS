#include <gdt.h>

/*
 * We use the myos namespace for OS-level classes, and myos::common for basic type definitions.
 * The GlobalDescriptorTable (GDT) is a fundamental structure in x86 architecture, which defines
 * memory segments for code and data, among other things.
 */
using namespace myos;
using namespace myos::common;

/*
 * ----------------------------------------------------------------------------
 * GlobalDescriptorTable Class
 * ----------------------------------------------------------------------------
 *
 * The GDT is an array of segment descriptors. In protected mode on x86,
 * segment registers (CS, DS, etc.) contain indices (selectors) into this table.
 * Each segment descriptor defines the base address, limit, and access flags
 * (e.g., readable code segment, writable data segment) for a particular segment.
 */

/*
 * Struct GDTR
 * ----------------------------------------------------------------------------
 *
 * Represents the Global Descriptor Table Register (GDTR), which holds the limit
 * and base address of the GDT. This structure is used by the `lgdt` instruction
 * to load the GDT into the CPU.
 */
struct __attribute__((packed)) GDTR {
    uint16_t limit;  // Size of the GDT in bytes minus one
    uint32_t base;   // Base address of the GDT
};

/*
 * Constructor:
 *   - Initializes four segments:
 *       1) nullSegmentSelector: a null (unused) segment descriptor
 *       2) unusedSegmentSelector: another unused descriptor
 *       3) codeSegmentSelector: a code segment descriptor (0x9A type => present, executable, readable)
 *       4) dataSegmentSelector: a data segment descriptor (0x92 type => present, writable)
 *   - Sets each segment to have a 64MB limit. (64*1024*1024 bytes)
 *   - Loads this GDT into the CPU via the lgdt instruction.
 */
GlobalDescriptorTable::GlobalDescriptorTable()
    : nullSegmentSelector(0, 0, 0),
      unusedSegmentSelector(0, 0, 0),
      codeSegmentSelector(0, 64 * 1024 * 1024, 0x9A),
      dataSegmentSelector(0, 64 * 1024 * 1024, 0x92)
{
    // Create a GDTR structure to hold the size and base address of the GDT
    GDTR gdtr;
    
    // Set the limit to the size of the GDT minus one
    gdtr.limit = sizeof(GlobalDescriptorTable) - 1;
    
    // Set the base to the address of this GDT instance
    gdtr.base = (uint32_t)this;
    
    // Load the GDTR using the LGDT instruction
    asm volatile("lgdt (%0)" : : "p" (&gdtr));
}

/*
 * Destructor:
 *   - Currently, there's no cleanup needed.
 *   - The GDT generally remains in place unless we switch to another.
 */
GlobalDescriptorTable::~GlobalDescriptorTable()
{
}

/*
 * DataSegmentSelector:
 *   - Returns the selector for the data segment.
 *   - The selector is the byte offset of the dataSegmentSelector descriptor within the GDT.
 */
uint16_t GlobalDescriptorTable::DataSegmentSelector()
{
    return (uint8_t*)&dataSegmentSelector - (uint8_t*)this;
}

/*
 * CodeSegmentSelector:
 *   - Returns the selector for the code segment.
 *   - The selector is the byte offset of the codeSegmentSelector descriptor within the GDT.
 */
uint16_t GlobalDescriptorTable::CodeSegmentSelector()
{
    return (uint8_t*)&codeSegmentSelector - (uint8_t*)this;
}

/*
 * ----------------------------------------------------------------------------
 * GlobalDescriptorTable::SegmentDescriptor
 * ----------------------------------------------------------------------------
 *
 * Each entry in the GDT is an 8-byte segment descriptor. This structure encodes:
 *   - limit (segment size),
 *   - base (starting address),
 *   - access rights/type (e.g., 0x9A for code segments, 0x92 for data segments),
 *   - flags like granularity (4K pages vs. 1 byte), 16-bit vs. 32-bit mode, etc.
 */

/*
 * SegmentDescriptor Constructor:
 *   - 'base': the starting address of the segment.
 *   - 'limit': the size of the segment (in bytes).
 *   - 'type': access flags/type (e.g., 0x9A = present, ring 0, code segment).
 *
 *   If 'limit' is <= 65536, we use 16-bit address space; otherwise, we shift 
 *   down by 12 bits (setting granularity to 4 KB) if possible. The data is 
 *   packed into an 8-byte descriptor structure in memory.
 */
GlobalDescriptorTable::SegmentDescriptor::SegmentDescriptor(uint32_t base, uint32_t limit, uint8_t type)
{
    uint8_t* target = (uint8_t*)this;

    // If limit fits in 16 bits, use 16-bit mode. Otherwise, enable 4 KB granularity and 32-bit mode
    if (limit <= 65536) {
        // 16-bit address space
        target[6] = 0x40;
    } else {
        // 32-bit address space with 4KB granularity
        if ((limit & 0xFFF) != 0xFFF)
            limit = (limit >> 12) - 1;
        else
            limit = limit >> 12;

        // 0xC0 => set granularity bit (4K) and 32-bit mode bit
        target[6] = 0xC0;
    }

    // Pack the limit (20 bits) into bytes [0..1, 6(4 bits)]
    target[0] = limit & 0xFF;            // Low byte
    target[1] = (limit >> 8) & 0xFF;     // Next byte
    target[6] |= (limit >> 16) & 0xF;    // Top 4 bits of limit

    // Encode the base (32 bits) into bytes [2..4, 7]
    target[2] = base & 0xFF;              // Base low byte
    target[3] = (base >> 8) & 0xFF;       // Base middle byte
    target[4] = (base >> 16) & 0xFF;      // Base high byte
    target[7] = (base >> 24) & 0xFF;      // Base highest byte

    // Set the access byte
    target[5] = type;
}

/*
 * Base:
 *   - Reconstructs the 32-bit base address from the 4 bytes stored in the descriptor.
 */
uint32_t GlobalDescriptorTable::SegmentDescriptor::Base()
{
    uint8_t* target = (uint8_t*)this;

    uint32_t result = target[7];
    result = (result << 8) + target[4];
    result = (result << 8) + target[3];
    result = (result << 8) + target[2];

    return result;
}

/*
 * Limit:
 *   - Reconstructs the segment limit from the descriptor. 
 *   - If granularity is set (4KB), the limit is shifted left by 12 bits and 0xFFF is added.
 */
uint32_t GlobalDescriptorTable::SegmentDescriptor::Limit()
{
    uint8_t* target = (uint8_t*)this;

    uint32_t result = target[6] & 0xF; // Top nibble of the 20-bit limit
    result = (result << 8) + target[1];
    result = (result << 8) + target[0];

    // If granularity is set (0xC0 in byte 6), adjust the limit
    if ((target[6] & 0xC0) == 0xC0)
        result = (result << 12) | 0xFFF;

    return result;
}
