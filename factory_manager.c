/*
 *
 * factory_manager.c
 *
 */

#include "queue.h"

void	free_all(t_factory *factory)
{
	if (factory)
	{
		if (factory->tapes)
		{
			for (int i = 0; i < factory->n_tapes; i++)
			{
				safe_mutex(&factory->tapes[i].queue_mtx, DESTROY);
				safe_cond(&factory->tapes[i].cond_full, NULL, DESTROY);
			}
			free(factory->tapes);
		}
		if (factory->sems)
		{
			for (int i = 0; i < factory->n_tapes; i++)
				safe_sem(&factory->sems[i], 0, DESTROY);
			free(factory->sems);
		}
		safe_mutex(&factory->mtx, DESTROY);
		free(factory);
	}
}

void	err_free_exit(t_factory *factory, const char *msg)
{
	free_all(factory);
	if (msg)
		fprintf(stderr, "%s\n", msg);
	exit(-1);
}

void *safe_malloc(size_t size)
{
	void *ptr = malloc(size);
	if (!ptr)
		err_free_exit(NULL, RED"[ERROR]"RESET" Memory allocation failed.");
	return (ptr);
}

void	safe_mutex(pthread_mutex_t *mutex, t_operations operation)
{
	int ret;

	if (operation == INIT)
		ret = pthread_mutex_init(mutex, NULL);
	else if (operation == DESTROY)
		ret = pthread_mutex_destroy(mutex);
	else if (operation == LOCK)
		ret = pthread_mutex_lock(mutex);
	else if (operation == UNLOCK)
		ret = pthread_mutex_unlock(mutex);
	if (ret)
		err_free_exit(NULL, RED"[ERROR]"RESET"[factory_manager] Mutex operation failed.");
}

void safe_sem(sem_t *sem, int value, t_operations operation)
{
	int ret;

	if (operation == INIT)
		ret = sem_init(sem, 0, value);
	else if (operation == DESTROY)
		ret = sem_destroy(sem);
	else if (operation == WAIT)
		ret = sem_wait(sem);
	else if (operation == POST)
		ret = sem_post(sem);
	if (ret)
		err_free_exit(NULL, RED"[ERROR]"RESET"[factory_manager] Semaphore operation failed.");
}

void safe_thread(pthread_t *thread, void *(*f)(void *), void *arg, void *retval, t_operations operation)
{
	int ret;

	if (operation == CREATE)
		ret = pthread_create(thread, NULL, f, arg);
	else if (operation == JOIN)
		ret = pthread_join(*thread, &retval);
	else if (operation == DETACH)
		ret = pthread_detach(*thread);
	if (ret)
		err_free_exit(NULL, RED"[ERROR]"RESET"[factory_manager] Thread operation failed.");
}

void safe_cond(pthread_cond_t *cond, pthread_mutex_t *mutex, t_operations operation)
{
	int ret;

	if (operation == INIT)
		ret = pthread_cond_init(cond, NULL);
	else if (operation == DESTROY)
		ret = pthread_cond_destroy(cond);
	else if (operation == SIGNAL)
		ret = pthread_cond_signal(cond);
	else if (operation == WAIT)
		ret = pthread_cond_wait(cond, mutex);
	if (ret)
		err_free_exit(NULL, RED"[ERROR]"RESET"[factory_manager] Condition variable operation failed.");
}

void safe_close(FILE *fd)
{
	int ret;

	ret = fclose(fd);
	if (ret == EOF)
		err_free_exit(NULL, RED"[ERROR]"RESET"[factory_manager] Invalid file.");
}

t_factory	*parser(const char *filename)
{
    FILE *fd;
    t_tape temp;
    t_factory *factory;
    int last;

    factory = safe_malloc(sizeof(t_factory));
    if (!(fd = fopen(filename, "r")))
        err_free_exit(factory, RED"[ERROR]"RESET"[factory_manager] Unable to open file.");
    
    if (fscanf(fd, "%d", &factory->max_tapes) != 1 || factory->max_tapes <= 0)
        err_free_exit(factory, RED"[ERROR]"RESET"[factory_manager] Invalid max_tapes value.");
    
    factory->tapes = safe_malloc(factory->max_tapes * sizeof(t_tape));
    factory->n_tapes = 0;
    factory->sems = safe_malloc(factory->max_tapes * sizeof(sem_t));
	factory->ready = false;
	safe_mutex(&factory->mtx, INIT);

    while (fscanf(fd, "%d %d %d", &temp.id, &temp.max_size, &temp.num_elements) == 3)
    {
        if (temp.max_size <= 0 || temp.num_elements <= 0)
            err_free_exit(factory, RED"[ERROR]"RESET"[factory_manager] Invalid tape data.");
		if (factory->n_tapes >= factory->max_tapes)
            err_free_exit(factory, RED"[ERROR]"RESET"[factory_manager] Too many tapes in input file.");
        safe_sem(&factory->sems[factory->n_tapes], factory->n_tapes == 0 ? 1 : 0, INIT);
		temp.semaphore_id = factory->n_tapes;
		temp.factory = factory;
		temp.finished = false;
		temp.num_created = 0;
		safe_mutex(&temp.queue_mtx, INIT);
		safe_cond(&temp.cond_full, NULL, INIT);
        factory->tapes[factory->n_tapes++] = temp;
    }
    
    safe_close(fd);
    return (factory);
}

void	print_factory(t_factory *factory)
{
	int i;

	printf(GREEN"[INFO]"RESET"[factory_manager] Factory with %d tapes:\n", factory->n_tapes);
	for (i = 0; i < factory->n_tapes; i++)
	{
		printf(BLUE"[INFO]"RESET"[factory_manager] Tape %d: size %d, elements %d\n",
			factory->tapes[i].id, factory->tapes[i].max_size, factory->tapes[i].num_elements);
	}
}

void	run_factory(t_factory *factory)
{
	int i;
	void *status;

	for (i = 0; i < factory->n_tapes; i++)
	{
		safe_thread(&factory->tapes[i].tape_id, process_manager, &factory->tapes[i], NULL, CREATE);
		printf(GREEN"[OK]"RESET"[factory_manager] Process_manager with id %d has been created.\n", factory->tapes[i].id);
	}
	set_bool(&factory->mtx, &factory->ready, true);
	for (i = 0; i < factory->n_tapes; i++)
	{
		safe_thread(&factory->tapes[i].tape_id, NULL, NULL, NULL, JOIN); // Pasar la dirección de status
		printf(GREEN"[OK]"RESET"[factory_manager] Process_manager with id %d has finished.\n", factory->tapes[i].id);
		if (factory->tapes[i].semaphore_id + 1 < factory->n_tapes)
			safe_sem(&factory->sems[factory->tapes[i].semaphore_id + 1], 0, POST);
	}
	printf(GREEN"[OK]"RESET"[factory_manager] Finishing.\n");
}

int main (int argc, const char **argv)
{
	int* status;
	t_factory *factory;

	if (argc != 2)
	{
		fprintf(stderr, RED"[ERROR]"RESET"[factory_manager] Usage: %s <input_file>\n", argv[0]);
		return (-1);
	}
	factory = parser(argv[1]);
	run_factory(factory);
	print_factory(factory);
	free_all(factory);

	return (0);
}
