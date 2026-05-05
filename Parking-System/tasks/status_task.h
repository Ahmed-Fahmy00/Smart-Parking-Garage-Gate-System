/*******************************************************************************
 * status_task.h
 * Optional low-priority Status Task.
 * Consumes the xLimitSemaphore and gives visual feedback (Blue LED flash)
 * each time a limit button is activated.
 ******************************************************************************/
#ifndef STATUS_TASK_H
#define STATUS_TASK_H

void StatusTask(void *pvParameters);

#endif /* STATUS_TASK_H */
