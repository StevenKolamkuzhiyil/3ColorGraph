/**
 * @file generator.c
 * @author Steven Kolamkuzhiyil <stevenkolamkuzhiyil@gmail.com>
 * @date 31.12.2019
 *
 * @brief Read a graph representation from command line and write 3-coloring solutions to the shared memory 
 * till the supervisor sets the state flag in the shared memory to a non-zero value.
 * 
 * @details Connect to a shared memory and generate 3-coloring solutions for the parsed graph. The generator randomly 
 * assigns "colors" to each node of an edge if it doesn't have one already and removes each edge which consists of 
 * two nodes with the same "color". The removed edges are added to the solution and written to the shared memory. If 
 * the supervisor notifies the generator to terminate all resources will be cleaned up before exiting. Writing to 
 * the shared memory abbides to mutual exclusion since multiple generators can opperate at the same time.
 */

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <limits.h>
#include "common.h"

// Global variables
struct edge {   /**< The container for the parsed edge nodes. */
    int nodeU;  /**< The node value wich is connected to nodeV */
    int nodeV;  /**< The node value wich is connected to nodeU */
};

// Prototypes
/**
 * @brief Write helpful usage information about the program to stderr.
 * 
 * @details Global variables: myprog.
 */
static void usage(void);

/**
 * @brief Parse an integer from string.
 * 
 * @details Parse an integer from the start of the string. The address till where a valid number was found will be stored 
 * in <endptr>. On error status will be set to a negative number and errno will be set.
 * 
 * @param str       The string from which the number should be parsed.
 * @param endptr    The address of the end pointer.
 * @param status    The address of the integer where the parse status should be set.
 * @return Returns the parsed integer on success.
 */
static int parseNumber(char *str, char **endptr, int *status);

/**
 * @brief Parse an edge of the format U-V with U and V being positive integers.
 * 
 * @details The parsed nodes are stored in the nodes buffer. If the edge is valid 0 is returned 
 * otherwise -1 is returned. Global variables: errno.
 * 
 * @param nodes The buffer where the parsed numbers will be stored.
 * @param edge  The edge string which needs to be parsed.
 * @return If the edge is valid 0 is returned otherwise -1 is returned
 */
static int parseEdge(int nodes[2], char *edge);

/**
 * @brief Parse an array of edge strings and store the parsed nodes in an array of structs.
 * 
 * @details The structs contain the nodes which are connected. If all edges are valid the number 
 * of nodes is returned and endptr will be set to NULL.
 * 
 * @param edges     The array where the parsed edges should be stored.
 * @param str       The array of strings which sould be parsed.
 * @param n         The number of edges to parse.
 * @param offset    The offset index from which parsing should start.
 * @param endptr    The address at which the last valid edge should be stored.
 * @return Returns the number of nodes on success otherwise -1 is returned and endptr will be set to the first invalid 
 * edge.
 */
static int parseEdges(struct edge *edges, char **str, int n, int offset, char **endptr);


/**
 * @brief Generates a 3-coloring for the graph.
 * 
 * @details Iterate through all edges and assigns random integers from the range of 1 to 3 to the nodes of an edge. 
 * Add the string representation of the edge to the output buffer if the assigned numbers are the same. If the buffer 
 * doesn't have enough space -1 is returned. On success 0 is returned. The return value indicates if a solution should 
 * be discarded.
 * 
 * @param nodes     The node array containing the assigned "colors".
 * @param nodeN     the size of the node array.
 * @param edges     The parsed edges array.
 * @param edgeN     The size of the parsed edges array.
 * @param edgeStr   The array containing the string representation of the edges.
 * @param offset    The starting index of the edgeStr array.
 * @param buf       The buffer where the solution should be safed at. Size of the buffer is MAX_LINE.
 * @return Returns 0 on success and -1 if the solution should be discarded.
 */
static int generate3coloring(char *nodes, int nodeN, struct edge *edges, int edgeN, char **edgeStr, int offset, char *buf);

/**
 * @brief Open an existing shared memory.
 * 
 * @details Open an existing shared memory and assign it to myshm. If an error occurs the program is terminated with 
 * EXIT_FAILURE.
 * 
 * @param myshm The address of the pointer where the address of the shared memory should be stored.
 * @return Returns the file descriptor to the shared memory on success and sets myshm to the address of the shared memory.
 */
static int openSHM(myshm_t **myshm);

/**
 * @brief Open an existing semaphore.
 * 
 * @details Open an existing semaphore named <sem_name>. If sem_open failed the program terminates with EXIT_FAILURE.
 * 
 * @param sem_name  The name of the semaphore which should be opened.
 * @return Returns a pointer to the opened semaphore on success.
 */
static sem_t * openSEM(const char * sem_name);

/**
 * @brief Write a string to the shared memory.
 * 
 * @details Write a string to the shared memory under mutual exclusion. After writing to the shared memory 
 * increase the wirte index and print a debug message to stdout.
 * Global variables: myshm, myprog.
 * 
 * @param str   The string which sould be written to the shared memory.
 * @return return 0 on success otherwise return -1.
 */
static void circ_buf_write(char *str);

/**
 * @brief Main function.
 * 
 * @details Handle command line arguments and execute the program. 
 * Global variables: shmfd, myshm, sem_used, sem_free, sem_mutex.
 * 
 * @param argc  The argument count.
 * @param argv  The list of arguments.
 * @return Returns EXIT_SUCCESS on successful exit otherwise returns EXIT_FAILURE.
 */
int main(int argc, char **argv) {
    myprog = argv[0];

    if (getopt(argc, argv, "") != -1)
        usage();
    if (argc <= 1)
        usage();

    // Open shared memory
    shmfd = openSHM(&myshm);
    if (atexit(unmap_close_SHM) != 0)
        error_exit("atexit() failed");
    // Open semaphores
    sem_used = openSEM(SEM_USED);
    if (atexit(closeSemUsed) != 0)
        error_exit("atexit() failed");
    sem_free = openSEM(SEM_FREE);
    if (atexit(closeSemFree) != 0)
        error_exit("atexit() failed");
    sem_mutex = openSEM(SEM_MUTEX);
    if (atexit(closeSemMutex) != 0)
        error_exit("atexit() failed");


    char *ptr = NULL;
    struct edge edges[argc-1];
    const int NODE_NUM = parseEdges(edges, argv, argc, 1, &ptr);
    if (NODE_NUM < 0 || ptr != NULL) {
        const int L = 23 + strlen(ptr);
        char errstr[L];
        sprintf(errstr, "Failed to parse edge %s", ptr);
        error_exit(errstr);
    }

    char nodes[NODE_NUM];
    char buf[MAX_LINE];
    srandom(getpid());
    int quit = 0;

    while (quit == 0) {
        int discard = generate3coloring(nodes, NODE_NUM, edges, argc-1, argv, 1, buf);
        if (discard != 0)
            continue;
        
        if (sem_wait(sem_mutex) == -1)
            error_exit("sem_wait() failed");
        if (myshm->state == 0) {
            if (sem_wait(sem_free) == -1)
                error_exit("sem_wait() failed");
            circ_buf_write(buf);
            if (sem_post(sem_used) == -1)
                error_exit("sem_wait() failed");
        } else
            quit = 1;
        if (sem_post(sem_mutex) == -1)
            error_exit("sem_wait() failed");
    }
    
    exit(EXIT_SUCCESS);
}


static void usage(void) {
    fprintf(stderr, "Usage: %s EDGE1...\n\tEDGE1: U-V, where U and V are vertex numbers\n", myprog);
    exit(EXIT_FAILURE);
}

static int parseNumber(char *str, char **endptr, int *status) {
    errno = 0;
    long val = strtol(str, endptr, 10);

    if ((errno == ERANGE && (val == LONG_MAX || val == LONG_MIN)) || (errno != 0 && val == 0))
        *status = -1;
    else if (*endptr == str)
        *status = -2;
    else if (val > INT_MAX || val < INT_MIN) {
        errno = ERANGE;
        *status = -3;
    }
    else
        *status = 0;

    return (int) val;
}

static int parseEdge(int nodes[2], char *edge) {
    int status;
    char *ptrU = NULL, *ptrV = NULL;
    nodes[0] = parseNumber(edge, &ptrU, &status);
    if (*ptrU != '-' || status < 0)
        return -1;
    nodes[1] = parseNumber(ptrU+1, &ptrV, &status);
    if (strcmp(ptrV, "") != 0 || status < 0)
        return -1;
    return 0;
}

static int parseEdges(struct edge *edges, char **str, int n, int offset, char **endptr) {
    int nodeN = 0;

    int currentNodes[2];
    for (int i = offset; i < n; i++) {
        *endptr = str[i];
        if (parseEdge(currentNodes, str[i]) < 0)
            return -1;
        *endptr = NULL;

        edges[i-offset].nodeU = currentNodes[0];
        edges[i-offset].nodeV = currentNodes[1];

        if (currentNodes[0] + 1 > nodeN)
            nodeN = currentNodes[0] + 1;
        if (currentNodes[1] + 1 > nodeN)
            nodeN = currentNodes[1] + 1;
    }
    return nodeN;
}

static int generate3coloring(char *nodes, int nodeN, struct edge *edges, int edgeN, char **edgeStr, int offset, char *buf) {
    memset(nodes, 0, nodeN);
    memset(buf, '\0', MAX_LINE);
    for (int i = 0; i < edgeN; i++) {
        if (nodes[edges[i].nodeU] == 0)
            nodes[edges[i].nodeU] = (random() % 3) + 1;
        if (nodes[edges[i].nodeV] == 0)
            nodes[edges[i].nodeV] = (random() % 3) + 1;
        
        if (nodes[edges[i].nodeU] == nodes[edges[i].nodeV]) {
            if (strlen(buf) > 0) {
                strncat(buf, " ", MAX_LINE-strlen(buf));
            }
            if (MAX_LINE-strlen(buf) < strlen(edgeStr[i+offset])) {
                return -1;
            }
            strncat(buf, edgeStr[i+offset], MAX_LINE-strlen(buf));
        }
    }
    return 0;
}


static int openSHM(myshm_t **myshm) {
    int shmfd = shm_open(SHM_NAME, O_RDWR, 0600);
    if (shmfd == -1)
        error_exit("Failed to open shared memory");

    *myshm = mmap(NULL, sizeof(myshm_t), PROT_READ | PROT_WRITE, MAP_SHARED, shmfd, 0);
    if (*myshm == MAP_FAILED) {
        close(shmfd);
        error_exit("Failed to set size of shared memory");
    }

    return shmfd;
}

static sem_t * openSEM(const char * sem_name) {
    sem_t *sem = sem_open(sem_name, 0);
    if (sem == SEM_FAILED) {
        const int L = 17 + strlen(sem_name);
        char errstr[L];
        sprintf(errstr, "Failed to open %s", sem_name);
        error_exit(errstr);
    }
    
    return sem;
}

static void circ_buf_write(char *str) {    
    strncpy(myshm->shm_buf[myshm->write_pos], str, MAX_LINE);
    myshm->write_pos = (myshm->write_pos + 1) % BUF_LEN;
    printf("%s [%d]: shm[%d]::%s\n", myprog, getpid(), myshm->write_pos, str);
}