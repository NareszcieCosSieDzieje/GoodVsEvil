#ifndef _OUR_THREADS_HPP_
#define _OUR_THREADS_HPP_


/* Thread loop responsible for coordinating other processes with user input*/
void monitorLoop(void);

/* Thread loop responsible for waiting for messages and responding accordingly*/
void communicationLoop(void);

/* Function returns bigger of two values */
int max(int, int);



#endif //_OUR_THREADS_HPP_