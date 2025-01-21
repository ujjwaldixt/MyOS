#include <multitasking.h>

/*
 * We place classes related to multitasking (Tasks, TaskManager, etc.) in the 
 * 'myos' namespace, with common definitions (uint32_t, etc.) coming from 'myos::common'.
 */
using namespace myos;
using namespace myos::common;


/*
 * ----------------------------------------------------------------------------
 * Task Class
 * ----------------------------------------------------------------------------
 *
 * Represents a single execution context (a thread or process) in a simplistic 
 * multitasking environment. Each Task has its own stack and CPU state.
 */

/*
 * Constructor:
 *   - gdt: a pointer to the system's Global Descriptor Table 
 *          (used to get the code segment selector).
 *   - entrypoint: a function pointer for where the task will begin execution.
 *
 * Steps:
 *   1) Reserve a stack of 4096 bytes.
 *   2) Initialize a CPUState structure at the top of this stack 
 *      (stack grows downward, so we subtract sizeof(CPUState)).
 *   3) Clear general-purpose registers (eax, ebx, etc.).
 *   4) Set eip to the function pointer 'entrypoint'.
 *   5) Set 'cs' (code segment) from the GDT code segment selector.
 *   6) Set eflags to 0x202 (enabling interrupts, among other flags).
 */
Task::Task(GlobalDescriptorTable* gdt, void entrypoint())
{
    // Position the cpustate structure at the top of the 4 KB stack
    cpustate = (CPUState*)(stack + 4096 - sizeof(CPUState));
    
    cpustate->eax = 0;
    cpustate->ebx = 0;
    cpustate->ecx = 0;
    cpustate->edx = 0;

    cpustate->esi = 0;
    cpustate->edi = 0;
    cpustate->ebp = 0;
    
    /*
     * Commented out registers (like gs, fs, es, ds, and error)
     * can be initialized here if needed or for a more complete context.
     *
     * cpustate->gs = 0;
     * cpustate->fs = 0;
     * cpustate->es = 0;
     * cpustate->ds = 0;
     * cpustate->error = 0;   
     */

    // cpustate->esp = ... (we typically rely on the stack pointer 
    // already being set to point at cpustate)

    // Set the instruction pointer to the entry function
    cpustate->eip = (uint32_t)entrypoint;

    // Use the code segment selector from the GDT
    cpustate->cs = gdt->CodeSegmentSelector();

    // cpustate->ss = ... (stack segment can be set if needed)
    
    // 0x202 sets the IF bit (bit 9 = 1) for interrupts enabled, among other default flags
    cpustate->eflags = 0x202;
}

/*
 * Destructor:
 *  - If dynamic resources were used, they would be freed here. 
 *    Currently empty as the stack is local to the Task object.
 */
Task::~Task()
{
}


/*
 * ----------------------------------------------------------------------------
 * TaskManager Class
 * ----------------------------------------------------------------------------
 *
 * Maintains a list of Task objects and schedules them in a round-robin fashion.
 * The CPUState switching occurs in the Schedule(...) method.
 */

/*
 * Constructor:
 *   - Initializes no tasks, and sets currentTask to -1 (no current task).
 */
TaskManager::TaskManager()
{
    numTasks = 0;
    currentTask = -1;
}

/*
 * Destructor:
 *  - Currently does nothing. 
 *    If tasks allocated additional resources, they'd be cleaned up here.
 */
TaskManager::~TaskManager()
{
}

/*
 * AddTask:
 *  - Attempts to add a Task to the manager’s 'tasks' array.
 *  - If the array is full (>= 256), returns false; otherwise, stores the task
 *    and returns true.
 */
bool TaskManager::AddTask(Task* task)
{
    if(numTasks >= 256)
        return false;
    
    tasks[numTasks++] = task;
    return true;
}

/*
 * Schedule:
 *  - Called by the interrupt routine (usually timer IRQ) to choose the next task.
 *  - The 'cpustate' parameter is the current CPUState (registers) at the moment 
 *    of the interrupt, representing the outgoing task's state.
 *
 * Steps:
 *   1) If there are no tasks, just return the current CPUState.
 *   2) Save the outgoing task’s CPUState (if currentTask >= 0).
 *   3) Increment currentTask to choose the next index (round-robin). 
 *      Wrap around with modulus if needed.
 *   4) Return the new currentTask’s cpustate, which the interrupt routine will load.
 */
CPUState* TaskManager::Schedule(CPUState* cpustate)
{
    // If no tasks exist, keep running the same context
    if(numTasks <= 0)
        return cpustate;
    
    // Save the CPUState of the current task
    if(currentTask >= 0)
        tasks[currentTask]->cpustate = cpustate;
    
    // Move to next task in round-robin
    if(++currentTask >= numTasks)
        currentTask %= numTasks;

    // Return the chosen task's saved CPUState, so the CPU can switch context
    return tasks[currentTask]->cpustate;
}
