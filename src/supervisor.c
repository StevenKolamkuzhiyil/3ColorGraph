/**
 * @file supervisor.c
 * @author Steven Kolamkuzhiyil <stevenkolamkuzhiyil@gmail.com>
 * @date 01.01.2020
 *
 * @brief Print the solution with the lowest amount of removed edges so that the graph is 3-colorable.
 * 
 * @details Set up the shared memory and the semaphores and initialize the circular buffer for communication 
 * with the generators. Wait for the generators to write solutions to the circular buffer. Remeber the solution 
 * with the least edges and print it to stdout. If a solution with 0 edges is read or SIGINT or SIGTERM is caught 
 * terminate the program. Before terminating notify all generators that they should terminate. Unlink all shared 
 * resources and terminate.
 */
#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <signal.h>
#include <string.h>
#include <limits.h>
#include "common.h"


// Global variables
volatile __sig_atomic_t quit = 0;   /**< The quit flag if SIGINT or SIGTERM is triggered */


// Prototypes
/**
 * @brief Write helpful usage information about the program to stderr.
 * 
 * @details Global variables: myprog.
 */
static void usage(void);

/**
 * @brief Sets the quit flag to 1.
 * 
 * @param signal    The incoming signal that needs to be handled
 */
static void handle_signal(int signal);

/**
 * @brief Initializes the shared memory.
 * 
 * @details Creates the shared memory if it doesn't exist. Sets it size and maps it. If the creation was 
 * successful a file descriptor to the shared memory is returned. If the initialization fails the program 
 * terminates with EXIT_FAILURE.
 * 
 * @param myshm The address of the shared memory pointer
 * @return Returns the shared memory file descriptor.
 */
static int initSHM(myshm_t **myshm);

/**
 * @brief Unmap, close and unlink the shared memory.
 * 
 * @details If an error occurs the error and errno is printed to stderr.
 */
static void cleanupSHM(void);

/**
 * @brief Create a semaphore.
 * 
 * @details Create a semaphore with its initial value set to value. On error the program is terminated with 
 * EXIT_FAILURE.
 * 
 * @param sem_name  The name of the semaphore.
 * @param value     The initial value of the semaphore.
 * @return Return a pointer to the semaphore.
 */
static sem_t * initSEM(const char * sem_name, int value);

/**
 * @brief Close and unlink the '/3col_used' semaphore.
 */
static void cleanupSemUsed(void);

/**
 * @brief Close and unlink the '/3col_free' semaphore.
 */
static void cleanupSemFree(void);

/**
 * @brief Close and unlink the '/3col_mutex' semaphore.
 */
static void cleanupSemMutex(void);

/**
 * @brief Read a string from the circular buffer in the shared memory.
 * 
 * @details Read a string from the circular buffer at read_pos and store it in buf. 
 * Global variables: myshm.
 * 
 * @param buf       The buffer where the read value should be stored.
 * @param read_pos  The index of the circular buffer from where the value should be read.
 */
static void circ_buf_read(char *buf, int *read_pos);

/**
 * @brief Count the number of edges.
 * 
 * @param edges The string representing the edges.
 * @return Returns the edge count.
 */
static uint countEdges(char *edges);


/**
 * @brief Main function
 * 
 * @details Handle command line arguments and execute the program. 
 * Global variables: shmfd, myshm, sem_used, sem_free, sem_mutex, quit, errno.
 * 
 * @param argc  The argument count.
 * @param argv  The list of arguments.
 * @return Returns EXIT_SUCCESS on successful exit otherwise returns EXIT_FAILURE.
 */
int main(int argc, char **argv) {
    myprog = argv[0];

    if (getopt(argc, argv, "") != -1)
        usage();
    if (argc != 1)
        usage();

    // Init signal handling
    struct sigaction sa;
    memset(&sa, 0, sizeof(sa));
    sa.sa_handler = handle_signal;
    sigaction(SIGINT, &sa, NULL);
    sigaction(SIGTERM, &sa, NULL);

    // Init shared memory
    shmfd = initSHM(&myshm);
    if (atexit(cleanupSHM) != 0)
        error_exit("atexit() failed");
    // Init semaphores
    sem_used = initSEM(SEM_USED, 0);
    if (atexit(cleanupSemUsed) != 0)
        error_exit("atexit() failed");
    
    sem_free = initSEM(SEM_FREE, BUF_LEN);
    if (atexit(cleanupSemFree) != 0)
        error_exit("atexit() failed");
    
    sem_mutex = initSEM(SEM_MUTEX, 1);
    if (atexit(cleanupSemMutex) != 0)
        error_exit("atexit() failed");


    if (sem_wait(sem_mutex) == -1) {
        if (errno != EINTR)
            error_exit("sem_wait() failed");
    }
    myshm->state = 0;
    myshm->write_pos = 0;
    if (sem_post(sem_mutex) == -1) {
        if (errno != EINTR)
            error_exit("sem_wait() failed");
    }


    int read_pos = 0;
    uint bestSolution = UINT_MAX;
    char edges[MAX_LINE];
    while (!quit) {
        memset(edges, '\0', sizeof(edges));

        /* Critical section start */
        if (sem_wait(sem_used) == -1) {
            if (errno != EINTR)
                error_exit("sem_wait() failed");
            quit = 1;
        }
        circ_buf_read(edges, &read_pos);
        if (sem_post(sem_free) == -1) {
            if (errno != EINTR)
                error_exit("sem_post() failed");
            quit = 1;
        }
        /* Critical section end */
        if (quit != 0)
            break;

        uint edgeCount = countEdges(edges);
        if (edgeCount > 0 && edgeCount < bestSolution) {
            bestSolution = edgeCount;
            printf("Solution with %d edges: %s\n", edgeCount, edges);
        } else if (edgeCount == 0) {
            myshm->state = 1;
            printf("The graph is 3-colorable!\n");
            break;
        }
    }


    myshm->state = 1;
    if (sem_post(sem_free) == -1) {
        if (errno != EINTR)
            error_exit("sem_post() failed");
    }

    exit(EXIT_SUCCESS);
}


static void usage(void) {
    fprintf(stderr, "Usage: %s\n", myprog);
    exit(EXIT_FAILURE);
}

static void handle_signal(int signal) {
    quit = 1;
}

static int initSHM(myshm_t **myshm) {
    int shmfd = shm_open(SHM_NAME, O_RDWR | O_CREAT, 0600);
    if (shmfd == -1)
        error_exit("Failed to open shared memory");
    
    if (ftruncate(shmfd, sizeof(myshm_t)) < 0) {
        close(shmfd);
        error_exit("Failed to set size of shared memory");
    }

    *myshm = mmap(NULL, sizeof(myshm_t), PROT_READ | PROT_WRITE, MAP_SHARED, shmfd, 0);
    if (*myshm == MAP_FAILED) {
        close(shmfd);
        error_exit("Failed to set size of shared memory");
    }

    return shmfd;
}

static void cleanupSHM(void) {
    unmap_close_SHM();
    if (shm_unlink(SHM_NAME) == -1)
        print_error("Failed to remove shared memory object");
}

static sem_t * initSEM(const char *sem_name, int value) {
    sem_t *sem = sem_open(sem_name, O_CREAT | O_EXCL, 0600, value);
    if (sem == SEM_FAILED)
        error_exit("sem_open() failed");
    
    return sem;
}

static void cleanupSemUsed(void) {
    closeSemUsed();
    if (sem_unlink(SEM_USED) == -1)
        print_error("Failed to unlink /01528992_3col_used");
}

static void cleanupSemFree(void) {
    closeSemFree();
    if (sem_unlink(SEM_FREE) == -1)
        print_error("Failed to unlink /01528992_3col_free");
}

static void cleanupSemMutex(void) {
    closeSemMutex();
    if (sem_unlink(SEM_MUTEX) == -1)
        print_error("Failed to unlink /01528992_3col_mutex");
}

static uint countEdges(char *edges) {
    if (strlen(edges) == 0)
        return 0;

    uint count = 0;
    char *ws = edges;
    while (ws != NULL) {
        count++;
        if (*ws == ' ')
            ws++;
        ws = strchr(ws, ' ');
    }
    return count;
}

static void circ_buf_read(char *buf, int *read_pos) {
    snprintf(buf, MAX_LINE, "%s", myshm->shm_buf[*read_pos]);
    *read_pos = (*read_pos + 1) % BUF_LEN;
}