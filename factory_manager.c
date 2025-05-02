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
				safe_cond(&factory->tapes[i].not_full, NULL, DESTROY);
				safe_cond(&factory->tapes[i].not_empty, NULL, DESTROY);
			}
			free(factory->tapes);
		}
		safe_cond(&factory->ready_threads, NULL, DESTROY);
		safe_cond(&factory->waiting_threads, NULL, DESTROY);
		safe_cond(&factory->broadcast, NULL, DESTROY);
		safe_sem(&factory->sem, 0, DESTROY);
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

void *safe_malloc(size_t size, bool calloc_flag)
{
	void *ptr;

	if (calloc_flag)
		ptr = calloc(1, size);
	else
		ptr = malloc(size);
	if (!ptr)
		err_free_exit(NULL, "[ERROR] Memory allocation failed.");
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
		err_free_exit(NULL, "[ERROR][factory_manager] Mutex operation failed.");
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
		err_free_exit(NULL, "[ERROR][factory_manager] Semaphore operation failed.");
}

void safe_thread(pthread_t *thread, void *(*f)(void *), void *arg, void **retval, t_operations operation)
{
	int ret;

	if (operation == CREATE)
		ret = pthread_create(thread, NULL, f, arg);
	else if (operation == JOIN)
		ret = pthread_join(*thread, retval);
	else if (operation == DETACH)
		ret = pthread_detach(*thread);
	if (ret)
		err_free_exit(NULL, "[ERROR][factory_manager] Thread operation failed.");
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
	else if (operation == BROADCAST)
		ret = pthread_cond_broadcast(cond);
	if (ret)
		err_free_exit(NULL, "[ERROR][factory_manager] Condition variable operation failed.");
}

void safe_close(FILE *fd)
{
	int ret;

	ret = fclose(fd);
	if (ret == EOF)
		err_free_exit(NULL, "[ERROR][factory_manager] Error in fclose.");
}

bool check_input(FILE *fd)
{
    int count = 0;
	int max_tapes = 0;
	char *line;
	char *ptr;

	line = safe_malloc((size_t)1024, false);
    if (!fgets(line, 1024, fd))
	{
        free(line); // Liberar memoria si fgets falla
        return (false);
    }
	ptr = line;
    // Contar la cantidad de números en la línea
    while (*ptr)
    {
        // Saltar espacios
        while (isspace(*ptr))
            ptr++;

        // Verificar si hay un número
        if (isdigit(*ptr))
        {
            count++;
            // Avanzar hasta el final del número
            while (isdigit(*ptr))
                ptr++;
        }
		if (count == 1)
			max_tapes = count;
        else if (*ptr != '\0' && !isspace(*ptr))
		{
			free(line);
            return (false);
		}
    }
	free(line);
	fseek(fd, 0, SEEK_SET);
    if ((count - max_tapes) % 3 != 0)
		return (false);
    return (true);
}

t_factory	*parser(const char *filename)
{
    FILE *fd;
    t_tape temp;
    t_factory *factory;

    if (!(fd = fopen(filename, "r")))
	{
        err_free_exit(NULL, "[ERROR][factory_manager] Invalid file.");
	}
	if (!check_input(fd))
	{
		safe_close(fd);
		err_free_exit(NULL, "[ERROR][factory_manager] Invalid file.");
	}
	factory = safe_malloc(sizeof(t_factory), false);
    if (fscanf(fd, "%d", &factory->max_tapes) != 1 || factory->max_tapes <= 0)
	{
		safe_close(fd);
        err_free_exit(factory, "[ERROR][factory_manager] Invalid file.");
	}
    
    factory->tapes = safe_malloc(factory->max_tapes * sizeof(t_tape), false);
    factory->n_tapes = 0;
	factory->ready_tapes = 0;
	factory->waiting_tapes = 0;
    // factory->sems = safe_malloc(factory->max_tapes * sizeof(sem_t), false);
	safe_cond(&factory->ready_threads, NULL, INIT);
	safe_cond(&factory->waiting_threads, NULL, INIT);
	safe_cond(&factory->broadcast, NULL, INIT);
	safe_sem(&factory->sem, 0, INIT);
	safe_mutex(&factory->mtx, INIT);

    while (fscanf(fd, "%d %d %d", &temp.id, &temp.max_size, &temp.num_elements) == 3)
    {
        if (temp.max_size <= 0 || temp.num_elements <= 0)
		{
			safe_close(fd);
            err_free_exit(factory, "[ERROR][factory_manager] Invalid file.");
		}
		if (factory->n_tapes >= factory->max_tapes)
		{
			safe_close(fd);
            err_free_exit(factory, "[ERROR][factory_manager] Invalid file.");
		}
        // safe_sem(&factory->sems[factory->n_tapes], factory->n_tapes == 0 ? 1 : 0, INIT);
		// temp.semaphore_id = factory->n_tapes;
		temp.factory = factory;
		temp.finished = false;
		temp.num_created = 0;
		safe_cond(&temp.not_full, NULL, INIT);
		safe_cond(&temp.not_empty, NULL, INIT);
		safe_mutex(&temp.queue_mtx, INIT);
        factory->tapes[factory->n_tapes++] = temp;
    }
    
    safe_close(fd);
    return (factory);
}

// void	print_factory(t_factory *factory)
// {
// 	int i;

// 	printf(GREEN"[INFO][factory_manager] Factory with %d tapes:\n", factory->n_tapes);
// 	for (i = 0; i < factory->n_tapes; i++)
// 	{
// 		printf(BLUE"[INFO][factory_manager] Tape %d: size %d, elements %d\n",
// 			factory->tapes[i].id, factory->tapes[i].max_size, factory->tapes[i].num_elements);
// 	}
// }

// Se encarga de sincronizar los procesos en dos barreras:
// La primera espera a que todos los procesos estén listos y la segunda espera a que todos los procesos estén esperando
// La primera barrera se implementa con un semáforo y una condición, mientras que la segunda barrera se implementa con doble condición y broadcast
static void synchro(t_factory *factory)
{
	safe_mutex(&factory->mtx, LOCK);
	while (factory->ready_tapes < factory->n_tapes)
    	safe_cond(&factory->ready_threads, &factory->mtx, WAIT); // Espera a que todos los procesos estén listos
	safe_mutex(&factory->mtx, UNLOCK);

	for (int i = 0; i < factory->n_tapes; ++i)
    	safe_sem(&factory->sem, 0, POST); // Desbloquea todos los procesos

	safe_mutex(&factory->mtx, LOCK);
	while (factory->waiting_tapes < factory->n_tapes)
    	safe_cond(&factory->waiting_threads, &factory->mtx, WAIT); // Espera a que todos los procesos estén listos
	safe_cond(&factory->broadcast, NULL, BROADCAST); // Desbloquea todos los procesos
	safe_mutex(&factory->mtx, UNLOCK);
}

void	run_factory(t_factory *factory)
{
	int i;
	int *status;

	for (i = 0; i < factory->n_tapes; i++)
	{
		safe_thread(&factory->tapes[i].tape_id, process_manager, &factory->tapes[i], NULL, CREATE);
		printf("[OK][factory_manager] Process_manager with id %d has been created.\n", factory->tapes[i].id);
	}

	synchro(factory);

	for (i = 0; i < factory->n_tapes; i++)
	{
		safe_thread(&factory->tapes[i].tape_id, NULL, NULL, (void **)&status, JOIN); // Pasar la dirección de status
		if (!*status)
			printf("[OK][factory_manager] Process_manager with id %d has finished.\n", factory->tapes[i].id);
		else
			fprintf(stderr, "[ERROR][factory_manager] Process_manager with id %d has finished with errors.\n", factory->tapes[i].id);
		free(status);
		// if (factory->tapes[i].semaphore_id + 1 < factory->n_tapes)
		// 	safe_sem(&factory->sems[factory->tapes[i].semaphore_id + 1], 0, POST);
	}
	printf("[OK][factory_manager] Finishing.\n");
}

int main (int argc, const char **argv)
{
	t_factory *factory;

	if (argc != 2)
	{
		fprintf(stderr, "[ERROR][factory_manager] Usage: %s <input_file>\n", argv[0]);
		return (-1);
	}
	factory = parser(argv[1]);
	run_factory(factory);
	free_all(factory);
	return (EXIT_SUCCESS);
}
