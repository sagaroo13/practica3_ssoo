#include "queue.h"

// Función para imprimir información de la fábrica (comentada)
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

void	free_all(t_factory *factory)
{
    if (factory) // Verifica si la fábrica existe
    {
        if (factory->tapes) // Verifica si las cintas de la fábrica existen
        {
            // Recorre todas las cintas y destruye sus recursos asociados
            for (int i = 0; i < factory->n_tapes; i++)
            {
                safe_mutex(&factory->tapes[i].queue_mtx, DESTROY); // Destruye el mutex de la cinta
                safe_cond(&factory->tapes[i].not_full, NULL, DESTROY); // Destruye la condición de "no llena"
                safe_cond(&factory->tapes[i].not_empty, NULL, DESTROY); // Destruye la condición de "no vacía"
            }
            free(factory->tapes); // Libera la memoria de las cintas
        }
        // Destruye las condiciones y semáforos globales de la fábrica
        safe_cond(&factory->ready_threads, NULL, DESTROY);
        safe_cond(&factory->waiting_threads, NULL, DESTROY);
        safe_cond(&factory->broadcast, NULL, DESTROY);
        safe_sem(&factory->sem, 0, DESTROY);
        safe_mutex(&factory->mtx, DESTROY); // Destruye el mutex global de la fábrica
        free(factory); // Libera la memoria de la estructura de la fábrica
    }
}

// Función para liberar recursos y salir con un mensaje de error
void	err_free_exit(t_factory *factory, const char *msg)
{
    free_all(factory); // Libera todos los recursos asociados a la fábrica
    if (msg) // Si hay un mensaje de error, lo imprime
        fprintf(stderr, "%s\n", msg);
    exit(-1); // Sale del programa con un código de error
}

// Función segura para asignar memoria
void *safe_malloc(size_t size, bool calloc_flag)
{
    void *ptr;

    // Asigna memoria con malloc o calloc según el flag
    if (calloc_flag)
        ptr = calloc(1, size);
    else
        ptr = malloc(size);

    // Si la asignación falla, libera recursos y sale con error
    if (!ptr)
        err_free_exit(NULL, "[ERROR] Memory allocation failed.");
    return (ptr); // Retorna el puntero a la memoria asignada
}

// Función segura para manejar operaciones con mutex
void	safe_mutex(pthread_mutex_t *mutex, t_operations operation)
{
    int ret;

    // Realiza la operación correspondiente sobre el mutex
    if (operation == INIT)
        ret = pthread_mutex_init(mutex, NULL);
    else if (operation == DESTROY)
        ret = pthread_mutex_destroy(mutex);
    else if (operation == LOCK)
        ret = pthread_mutex_lock(mutex);
    else if (operation == UNLOCK)
        ret = pthread_mutex_unlock(mutex);

    // Si ocurre un error, libera recursos y sale con error
    if (ret)
        err_free_exit(NULL, "[ERROR][factory_manager] Mutex operation failed.");
}

// Función segura para manejar operaciones con semáforos
void safe_sem(sem_t *sem, int value, t_operations operation)
{
    int ret;

    // Realiza la operación correspondiente sobre el semáforo
    if (operation == INIT)
        ret = sem_init(sem, 0, value);
    else if (operation == DESTROY)
        ret = sem_destroy(sem);
    else if (operation == WAIT)
        ret = sem_wait(sem);
    else if (operation == POST)
        ret = sem_post(sem);

    // Si ocurre un error, libera recursos y sale con error
    if (ret)
        err_free_exit(NULL, "[ERROR][factory_manager] Semaphore operation failed.");
}

// Función segura para manejar operaciones con hilos
void safe_thread(pthread_t *thread, void *(*f)(void *), void *arg, void **retval, t_operations operation)
{
    int ret;

    // Realiza la operación correspondiente sobre el hilo
    if (operation == CREATE)
        ret = pthread_create(thread, NULL, f, arg);
    else if (operation == JOIN)
        ret = pthread_join(*thread, retval);
    else if (operation == DETACH)
        ret = pthread_detach(*thread);

    // Si ocurre un error, libera recursos y sale con error
    if (ret)
        err_free_exit(NULL, "[ERROR][factory_manager] Thread operation failed.");
}

// Función segura para manejar operaciones con variables de condición
void safe_cond(pthread_cond_t *cond, pthread_mutex_t *mutex, t_operations operation)
{
    int ret;

    // Realiza la operación correspondiente sobre la variable de condición
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

    // Si ocurre un error, libera recursos y sale con error
    if (ret)
        err_free_exit(NULL, "[ERROR][factory_manager] Condition variable operation failed.");
}

// Función segura para cerrar un archivo
void safe_close(FILE *fd)
{
    int ret;

    ret = fclose(fd); // Cierra el archivo
    if (ret == EOF) // Si ocurre un error al cerrar, libera recursos y sale con error
        err_free_exit(NULL, "[ERROR][factory_manager] Error in fclose.");
}

// Función para verificar si el archivo de entrada tiene el formato correcto
static bool check_input(FILE *fd)
{
    int count = 0;
    int max_tapes = 0;
    char *line;
    char *ptr;

    line = safe_malloc((size_t)1024, false); // Asigna memoria para leer una línea
    if (!fgets(line, 1024, fd)) // Lee la primera línea del archivo
    {
        free(line); // Libera memoria si fgets falla
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
            max_tapes = count; // El primer número es el número máximo de cintas
        else if (*ptr != '\0' && !isspace(*ptr))
        {
            free(line); // Libera memoria si hay un carácter no válido
            return (false);
        }
    }
    free(line); // Libera la memoria de la línea
    fseek(fd, 0, SEEK_SET); // Mueve el puntero del archivo al inicio

    // Verifica que la cantidad de números cumpla con el formato esperado
    if ((count - max_tapes) % 3 != 0)
        return (false);
    return (true);
}

// Función para analizar el archivo de entrada y crear la fábrica
static t_factory	*parser(const char *filename)
{
    FILE *fd;
    t_tape temp;
    t_factory *factory;

    if (!(fd = fopen(filename, "r"))) // Abre el archivo
    {
        err_free_exit(NULL, "[ERROR][factory_manager] Invalid file.");
    }
    if (!check_input(fd)) // Verifica el formato del archivo
    {
        safe_close(fd);
        err_free_exit(NULL, "[ERROR][factory_manager] Invalid file.");
    }
    factory = safe_malloc(sizeof(t_factory), false); // Asigna memoria para la fábrica
    if (fscanf(fd, "%d", &factory->max_tapes) != 1 || factory->max_tapes <= 0) // Lee el número máximo de cintas
    {
        safe_close(fd);
        err_free_exit(factory, "[ERROR][factory_manager] Invalid file.");
    }
    
    factory->tapes = safe_malloc(factory->max_tapes * sizeof(t_tape), false); // Asigna memoria para las cintas
    factory->n_tapes = 0;
    factory->ready_tapes = 0;
    factory->waiting_tapes = 0;
    safe_cond(&factory->ready_threads, NULL, INIT); // Inicializa las variables de condición
    safe_cond(&factory->waiting_threads, NULL, INIT);
    safe_cond(&factory->broadcast, NULL, INIT);
    safe_sem(&factory->sem, 0, INIT); // Inicializa el semáforo
    safe_mutex(&factory->mtx, INIT); // Inicializa el mutex

    // Lee la información de las cintas del archivo y guarda dicha información en una variable temporal
	// Verifica que el formato sea correcto y que los valores sean válidos
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
        temp.factory = factory;
        temp.finished = false;
        temp.num_created = 0;
        safe_cond(&temp.not_full, NULL, INIT); // Inicializa las condiciones de la cinta
        safe_cond(&temp.not_empty, NULL, INIT);
        safe_mutex(&temp.queue_mtx, INIT);
        factory->tapes[factory->n_tapes++] = temp; // Agrega la cinta a la fábrica
    }
    
    safe_close(fd); // Cierra el archivo
    return (factory); // Retorna la fábrica creada
}

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
        safe_cond(&factory->waiting_threads, &factory->mtx, WAIT); // Espera a que todos los procesos estén esperando
    safe_cond(&factory->broadcast, NULL, BROADCAST); // Desbloquea todos los procesos
    safe_mutex(&factory->mtx, UNLOCK);
}

// Función principal para ejecutar la fábrica
static void	run_factory(t_factory *factory)
{
    int i;
    int *status;

    // Crea un hilo para cada cinta
    for (i = 0; i < factory->n_tapes; i++)
    {
        safe_thread(&factory->tapes[i].tape_id, process_manager, &factory->tapes[i], NULL, CREATE);
        printf("[OK][factory_manager] Process_manager with id %d has been created.\n", factory->tapes[i].id);
    }

    synchro(factory); // Sincroniza los procesos

    // Espera a que todos los hilos terminen
    for (i = 0; i < factory->n_tapes; i++)
    {
        safe_thread(&factory->tapes[i].tape_id, NULL, NULL, (void **)&status, JOIN); // Une los hilos
        if (!*status)
            printf("[OK][factory_manager] Process_manager with id %d has finished.\n", factory->tapes[i].id);
        else
            fprintf(stderr, "[ERROR][factory_manager] Process_manager with id %d has finished with errors.\n", factory->tapes[i].id);
        free(status); // Libera el estado del hilo
    }
    printf("[OK][factory_manager] Finishing.\n");
}

// Función principal del programa
int main (int argc, const char **argv)
{
    t_factory *factory;

    // Verifica que se pase el archivo de entrada como argumento
    if (argc != 2)
    {
        fprintf(stderr, "[ERROR][factory_manager] Usage: %s <input_file>\n", argv[0]);
        return (-1);
    }
    factory = parser(argv[1]); // Analiza el archivo de entrada y crea la fábrica
    run_factory(factory); // Ejecuta la fábrica
    free_all(factory); // Libera todos los recursos
    return (EXIT_SUCCESS);
}
