/**
 * @file common.c
 * @author Steven Kolamkuzhiyil <stevenkolamkuzhiyil@gmail.com>
 * @date 31.12.2019
 *
 * @brief A collection of common variables and functions the supervisor and generators share.
 */

#include "common.h"

void print_error(const char *msg) {
    fprintf(stderr, "Error in %s: %s\n", myprog, msg);
    if (errno != 0)
        fprintf(stderr, "Error in %s: %s\n", myprog, strerror(errno));
}

void error_exit(const char *msg) {
    fprintf(stderr, "Error in %s: %s\n", myprog, msg);
    if (errno != 0)
        fprintf(stderr, "Error in %s: %s\n", myprog, strerror(errno));

    exit(EXIT_FAILURE);
}

void unmap_close_SHM(void) {
    if (munmap(myshm, sizeof(myshm_t)) == -1) {
        print_error("Failed to unmap shared memory");
    }

    if (close(shmfd) == -1)
        print_error("Failed to close shared memory file descriptor");
}

void closeSemUsed(void) {
    if (sem_close(sem_used) == -1)
        print_error("Failed to close /01528992_3col_used");
}

void closeSemFree(void) {
    if (sem_close(sem_free) == -1)
        print_error("Failed to close /01528992_3col_free");
}

void closeSemMutex(void) {
    if (sem_close(sem_mutex) == -1)
        print_error("Failed to close /01528992_3col_mutex");
}