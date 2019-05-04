#include "hw2_syscalls.h"
#include "sched.h" 
#include <linux/slab.h>
#include <asm/uaccess.h> 
#include <linux/kernel.h>
#include <linux/timer.h>

int sys_is_short(pid_t pid)
{
    struct task_struct* task = find_task_by_pid(pid);
    //check - legal pid
    if(task == NULL) {
        return -ESRCH;
    }
    // check if policy is short
    if(task->policy == SCHED_SHORT) {
    return 1;
    }
    return 0;
}

int sys_short_remaining_time(pid_t pid)
{
    struct task_struct* task = find_task_by_pid(pid);
    //check - legal pid
    if(task == NULL) {
        return -ESRCH;
    }
    //check - this is a task with short prio
    if(!(sys_is_short(pid) == 1)) {
        return -EINVAL;
    }
    // calc remaining time in jiffies
    int requsted_time = task->time_slice;
    //convert time to ms
    requsted_time = requsted_time * HZ / 1000;
    return requsted_time;	
}

int sys_short_place_in_queue(pid_t pid)
{
    //check - legal pid
    struct task_struct* task = find_task_by_pid(pid);
    if(task == NULL) {
        return -ESRCH;
    }
    //check - a short task
    if(!(sys_is_short(pid) == 1)) {
        return -EINVAL;
    }
    //if the current task checks itself return 0
    if (current == pid) {
        return 0;
    }
    //go to the ready to run list and scan it. count the short tasks 
    int counter = 0;
    int array_prio = 0;
    struct runqueue run_list = task_rq(task);
    //init next to the first task in the list 
    struct task_struct* next_task_to_run = run_list->queue[0];
    while (task != next && array_prio < MAX_PRIO) {
        // if the task is short -> counter++
        if (next_task_to_run->prio == SCHED_SHORT) {
            counter++;
        }
        //update the next task to run
        next_task_to_run = next_task_to_run->next;
        //if we finished this prios list go to the next prio
        if (next_task_to_run == NULL && array_prio < MAX_PRIO -1 ) {
            array_prio++;
            next_task_to_run = run_list->queue[array_prio];
        }
    }
    return counter;	
}