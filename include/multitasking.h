#ifndef __MYOS__MULTITASKING_H
#define __MYOS__MULTITASKING_H

#include <common/types.h>
#include <gdt.h>

namespace myos
{
    // The CPUState structure represents the state of the CPU registers during a task's execution.
    // This is used for saving and restoring a task's context during multitasking.
    struct CPUState
    {
        common::uint32_t eax;  // Accumulator register
        common::uint32_t ebx;  // Base register
        common::uint32_t ecx;  // Counter register
        common::uint32_t edx;  // Data register

        common::uint32_t esi;  // Source index for string operations
        common::uint32_t edi;  // Destination index for string operations
        common::uint32_t ebp;  // Base pointer for stack frames

        /*
        Segment registers can be included for additional control, but they are commented out here.
        These registers are typically used for memory segmentation in real-mode or protected mode.
        common::uint32_t gs;
        common::uint32_t fs;
        common::uint32_t es;
        common::uint32_t ds;
        */

        common::uint32_t error;   // Error code pushed onto the stack by the CPU (if applicable)

        common::uint32_t eip;     // Instruction pointer (next instruction to execute)
        common::uint32_t cs;      // Code segment register
        common::uint32_t eflags;  // CPU flags register
        common::uint32_t esp;     // Stack pointer
        common::uint32_t ss;      // Stack segment register
    } __attribute__((packed)); // Ensures no padding is added by the compiler.

    
    // The Task class represents a single task (or process) in the operating system.
    // Each task has its own stack and CPU state, allowing it to be suspended and resumed independently.
    class Task
    {
        friend class TaskManager; // Allows TaskManager to access private members of Task.
    private:
        common::uint8_t stack[4096]; // Stack memory for the task (4 KiB per task)
        CPUState* cpustate;          // Pointer to the saved CPU state for the task
    public:
        // Constructor: Initializes a task with a given entry point function and sets up the stack.
        Task(GlobalDescriptorTable *gdt, void entrypoint());
        
        // Destructor: Cleans up resources associated with the task.
        ~Task();
    };

    
    // The TaskManager class is responsible for managing and scheduling tasks in the operating system.
    // It maintains a list of tasks and handles context switching between them.
    class TaskManager
    {
    private:
        Task* tasks[256];    // Array of pointers to tasks (supports up to 256 tasks)
        int numTasks;        // Number of tasks currently managed
        int currentTask;     // Index of the currently running task
    public:
        // Constructor: Initializes the task manager, preparing it for task management.
        TaskManager();

        // Destructor: Cleans up resources used by the task manager.
        ~TaskManager();

        // Adds a task to the task manager. Returns true if successful, false if the task list is full.
        bool AddTask(Task* task);

        // Schedules the next task to run. Takes the current CPU state as input and returns the next task's CPU state.
        // This method performs context switching between tasks.
        CPUState* Schedule(CPUState* cpustate);
    };

}

#endif
