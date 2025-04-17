#ifndef HEADER_FILE
#define HEADER_FILE

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <stddef.h>
#include <semaphore.h>
#include <sys/stat.h>
#include <pthread.h>

# define BLACK "\033[30m"
# define RED "\033[31m"
# define GREEN "\033[32m"
# define YELLOW "\033[33m"
# define BLUE "\033[34m"
# define MAGENTA "\033[35m"
# define CYAN "\033[36m"
# define WHITE "\033[37m"
# define RESET "\033[0m"

typedef struct s_factory t_factory;

typedef struct s_tape
{
	int id;
	int semaphore_id;
	int tape_size;
	int num_elements;
	t_factory *factory;
	pthread_t tape_id;
} t_tape;

typedef struct s_factory
{
	int max_tapes;
	int n_tapes;
	t_tape *tapes;
	sem_t *sems;
} t_factory;

typedef enum e_operation
{
	CREATE,
	DETACH,
	JOIN,
	INIT,
	DESTROY,
	LOCK,
	UNLOCK,
	WAIT,
	POST,
} t_operation;

typedef struct element {
	int num_edition;
	int id_belt;
	int last;
} element;

int queue_init (int size);
int queue_destroy (void);
int queue_put (struct element* elem);
struct element * queue_get(void);
int queue_empty (void);
int queue_full(void);
void safe_sem(sem_t *sem, int value, t_operation operation);
void safe_thread(pthread_t *thread, void *(*f)(void *), void *arg, t_operation operation);
void *process_manager (void *arg);

#endif
