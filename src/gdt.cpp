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
      codeSegmentSelector(0, 64*1024*1024, 0x9A),
      dataSegmentSelector(0, 64*1024*1024, 0x92)
{
    // i is a structure that holds the size of this GDT and its base address
    uint32_t i[2];
    
    // i[1] = base address of this table
    i[1] = (uint32_t)this;
    // i[0] = size in bytes (limit), shifted left 16 bits
    // The size is "sizeof(GlobalDescriptorTable)" => number of bytes in our GDT
    i[0] = sizeof(GlobalDescriptorTable) << 16;
    
    // lgdt expects a 6-byte operand: 2 bytes for size (limit), then 4 bytes for base
    // We place them in 'i' and pass a pointer to the 2nd byte so it matches (limit, base).
    // The "asm volatile" block executes the LGDT instruction to load this GDT.
    asm volatile("lgdt (%0)": :"p" (((uint8_t *) i)+2));
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
 *   - Returns the offset in bytes from the start of this GlobalDescriptorTable 
 *     to the 'dataSegmentSelector'. This offset (in GDT terms) becomes the segment selector value.
 */
uint16_t GlobalDescriptorTable::DataSegmentSelector()
{
    // Subtract base pointer from address of dataSegmentSelector to get the offset in bytes
    return (uint8_t*)&dataSegmentSelector - (uint8_t*)this;
}

/*
 * CodeSegmentSelector:
 *   - Similar to DataSegmentSelector, returns the offset to the codeSegmentSelector descriptor.
 */
uint16_t GlobalDescriptorTable::CodeSegmentSelector()
{
    // Subtract base pointer from address of codeSegmentSelector to get the offset in bytes
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
 *   - access rights/type (e.g. code/data, privilege level),
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

    // If limit fits in 16 bits, use 16-bit mode. Otherwise, we enable 4 KB granularity.
    if (limit <= 65536)
    {
        // 16-bit address space
        target[6] = 0x40;
    }
    else
    {
        // 32-bit address space with 4KB granularity
        // We shift the limit right by 12 bits to fit into the 20-bit limit field.
        // If the lower 12 bits aren't 0xFFF, we decrease it by 1 after the shift.
        if((limit & 0xFFF) != 0xFFF)
            limit = (limit >> 12) - 1;
        else
            limit = limit >> 12;

        // 0xC0 => set granularity bit (4K) and 32-bit mode bit
        target[6] = 0xC0;
    }

    // Now we pack the limit (20 bits) into bytes [0..1, 6(4 bits)]
    target[0] = limit & 0xFF;            // low byte
    target[1] = (limit >> 8) & 0xFF;     // next byte
    target[6] |= (limit >> 16) & 0xF;    // top 4 bits of limit

    // Encode the base (32 bits) into bytes [2..4, 7]
    target[2] = base & 0xFF;            // base low byte
    target[3] = (base >> 8) & 0xFF;     // base mid
    target[4] = (base >> 16) & 0xFF;    // base high
    target[7] = (base >> 24) & 0xFF;    // base highest

    // Type/access byte goes in target[5]
    target[5] = type;
}

/*
 * Base:
 *   - Reconstructs the 32-bit base address from the 4 bytes stored in the descriptor.
 */
uint32_t GlobalDescriptorTable::SegmentDescriptor::Base()
{
    uint8_t* target = (uint8_t*)this;

    // Highest byte is at target[7], then [4], [3], [2]
    uint32_t result = target[7];
    result = (result << 8) + target[4];
    result = (result << 8) + target[3];
    result = (result << 8) + target[2];

    return result;
}

/*
 * Limit:
 *   - Reconstructs the segment limit from the descriptor. 
 *   - The limit can be extended to 4KB granularity if bits [7..6] in target[6] == 0xC0.
 *     In that case, we shift the 20-bit limit left by 12 bits and set the lowest 12 bits to 0xFFF.
 */
uint32_t GlobalDescriptorTable::SegmentDescriptor::Limit()
{
    uint8_t* target = (uint8_t*)this;

    // Lower 20 bits of the limit are split among target[0], target[1], and low 4 bits of target[6].
    uint32_t result = target[6] & 0xF; // top nibble of the 20-bit limit
    result = (result << 8) + target[1];
    result = (result << 8) + target[0];

    // If granularity is set (0xC0), then we shift by 12 and add 0xFFF
    if((target[6] & 0xC0) == 0xC0)
        result = (result << 12) | 0xFFF;

    return result;
}
