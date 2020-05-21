/**
 * @file common.h
 * @author Steven Kolamkuzhiyil <stevenkolamkuzhiyil@gmail.com>
 * @date 31.12.2019
 *
 * @brief A collection of common variables and functions the supervisor and generators share.
 */

#pragma once
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>

#include <fcntl.h>
#include <sys/mman.h>
#include <sys/types.h>
#include <semaphore.h>
#include <unistd.h>


#define SHM_NAME    "/3col"
#define SEM_FREE    "/3col_free"
#define SEM_USED    "/3col_used"
#define SEM_MUTEX   "/3col_mutex"
#define BUF_LEN     64
#define MAX_LINE    50


typedef struct myshm {                  /**< The shared memory. */
    int state;                          /**< The state flag. If state not equals 0 all generators should terminate. */
    int write_pos;                      /**< The index at which the generators should write to the circular buffer. */
    char shm_buf[BUF_LEN][MAX_LINE];    /**< The circular buffer. */
} myshm_t;

sem_t *sem_free;                        /**< The semaphore indicating the free space in the circular buffer. */
sem_t *sem_used;                        /**< The semaphore indicating the used space in the circular buffer. */
sem_t *sem_mutex;                       /**< The semaphore for mutual exclusion. */
myshm_t *myshm;                         /**< A pointer to the shared memory. */
int shmfd;                              /**< The file descriptor to the shared memory. */
char *myprog;                           /**< The program name. */

/**
 * @brief Print error to stderr.
 * 
 * @details Print the program name and the error message as well as the error string in 
 * errno to stderr. The program exits with EXIT_FAILURE. 
 * Global variables: myprog, errno.
 * 
 * @param msg   The error message.
 */
void error_exit(const char *msg);

/**
 * @brief Print error to stderr and return EXIT_FAILURE.
 * 
 * @details Print the program name and the error message as well as the error string in 
 * errno to stderr.
 * Global variables: myprog, errno.
 * 
 * @param msg   The error message.
 */
void print_error(const char *msg);

/**
 * @brief Unmap and close the shared memory.
 * 
 * @details Global variables: shmfd, myshm.
 */
void unmap_close_SHM(void);

/**
 * @brief Close the semaphore indicating the used space in the circular buffer.
 * 
 * @details Global variables: sem_used.
 */
void closeSemUsed(void);

/**
 * @brief Close the semaphore indicating the free space in the circular buffer.
 * 
 * @details Global variables: sem_free.
 * 
 * @param void
 */
void closeSemFree(void);

/**
 * @brief Close the semaphore for mutual exclusion.
 * 
 * @details Global variables: sem_mutex.
 */
void closeSemMutex(void);