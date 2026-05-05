/*******************************************************************************
 * safety_task.h
 * Highest-priority Safety Task - monitors obstacle signal and enforces
 * immediate reverse-then-stop behaviour.
 ******************************************************************************/
#ifndef SAFETY_TASK_H
#define SAFETY_TASK_H

void SafetyTask(void *pvParameters);

#endif /* SAFETY_TASK_H */
