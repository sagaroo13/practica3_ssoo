#include "queue.h"

// Función que ejecutará el hilo productor
static void *producer(void *arg)
{
    t_tape *queue;
    t_element item;
    int i;

    queue = (t_tape *)arg; // Castea el argumento a un puntero de tipo t_tape
    for (i = 0; i < queue->num_elements; i++) // Itera para producir el número de elementos especificado
    {
        safe_mutex(&queue->queue_mtx, LOCK); // Bloquea el mutex de la cola
        while (queue->size == queue->max_size) // Si la cola está llena, espera
            safe_cond(&queue->not_full, &queue->queue_mtx, WAIT);

        queue_put(queue, &item); // Inserta un elemento en la cola
        printf("[OK][queue] Introduced element with id %d in belt %d.\n", item.num_edition, item.id_belt);

        safe_cond(&queue->not_empty, &queue->queue_mtx, SIGNAL); // Señaliza al consumidor que hay un nuevo elemento disponible

        safe_mutex(&queue->queue_mtx, UNLOCK); // Desbloquea el mutex de la cola
    }

    safe_mutex(&queue->queue_mtx, LOCK); // Bloquea el mutex para marcar la cola como terminada
    queue->finished = true; // Indica que la producción ha terminado
    safe_cond(&queue->not_empty, &queue->queue_mtx, SIGNAL); // Señaliza al consumidor que puede terminar
    safe_mutex(&queue->queue_mtx, UNLOCK); // Desbloquea el mutex

    return (NULL); // Retorna NULL al finalizar
}

// Función que ejecutará el hilo consumidor
static void *consumer(void *arg)
{
    t_tape *queue;
    t_element *item;

    queue = (t_tape *)arg; // Castea el argumento a un puntero de tipo t_tape
    while (true) // Bucle infinito hasta que se cumpla la condición de salida
    {
        safe_mutex(&queue->queue_mtx, LOCK); // Bloquea el mutex de la cola
        while (queue->size < queue->max_size && !queue->finished) // Espera si la cola está vacía y no ha terminado
            safe_cond(&queue->not_empty, &queue->queue_mtx, WAIT);

        // Salir si ya no habrá más producción y la cola está vacía
        if (queue->finished && !queue->size)
        {
            safe_mutex(&queue->queue_mtx, UNLOCK); // Desbloquea el mutex antes de salir
            break;
        }

        item = queue_get(queue); // Obtiene un elemento de la cola
        printf("[OK][queue] Obtained element with id %d in belt %d.\n", item->num_edition, item->id_belt);

        safe_cond(&queue->not_full, &queue->queue_mtx, SIGNAL); // Avisa al producer si estaba bloqueado
        safe_mutex(&queue->queue_mtx, UNLOCK);
    }

    return (NULL);
}

// Función para sincronizar los procesos en el que se emplea una doble barrera, la primera para esperar a que todos los procesos estén listos
// y la segunda para que todos los procesos estén esperando a que la fábrica esté lista para empezar
// Se emplea una condición y un semáforo para la primera barrera y una doble condición para la segunda barrera (empleando un broadcast activado de desde factory_manager)
static void	synchro(t_tape *queue, bool flag)
{
    safe_mutex(&queue->factory->mtx, LOCK); // Bloquea el mutex de la fábrica
    queue->factory->ready_tapes++; // Incrementa el contador de cintas listas
    if (queue->factory->ready_tapes == queue->factory->n_tapes) // Si todas las cintas están listas
        safe_cond(&queue->factory->ready_threads, NULL, SIGNAL); // Señaliza a la fábrica que todos los procesos están listos
    safe_mutex(&queue->factory->mtx, UNLOCK); // Desbloquea el mutex
    safe_sem(&queue->factory->sem, 0, WAIT); // Espera a que el resto de procesos estén listos

    if (flag) // Si el flag está activado, imprime un mensaje informativo
        printf("[OK][process_manager] Process_manager with id %d waiting to produce %d elements.\n", queue->id, queue->num_elements);

    safe_mutex(&queue->factory->mtx, LOCK); // Bloquea el mutex de la fábrica
    queue->factory->waiting_tapes++; // Incrementa el contador de cintas esperando
    if (queue->factory->waiting_tapes == queue->factory->n_tapes) // Si todas las cintas están esperando
        safe_cond(&queue->factory->waiting_threads, NULL, SIGNAL); // Señaliza a la fábrica que todas las cintas están esperando
    safe_mutex(&queue->factory->mtx, UNLOCK); // Desbloquea el mutex
    safe_mutex(&queue->factory->mtx, LOCK); // Bloquea el mutex nuevamente
    safe_cond(&queue->factory->broadcast, &queue->factory->mtx, WAIT); // Espera a que la fábrica esté lista para empezar
    safe_mutex(&queue->factory->mtx, UNLOCK); // Desbloquea el mutex
}

// Función principal que gestiona el proceso de una cinta
void *process_manager (void *arg)
{
    pthread_t threads[NUM_THREADS]; // Arreglo de hilos para productor y consumidor
    t_tape *queue;
    int *status;
    int ret;

    status = safe_malloc(sizeof(int), false); // Asigna memoria para el estado del hilo
    *status = 0; // Inicializa el estado como exitoso

    queue = (t_tape *)arg;
	// Aunque ya hayamos comprobado estas condiciones al hacer el parseo, lo implementamos
    // para verificar que el fallo de un hilo no interrumpe la ejecución del resto
    if (queue->num_elements <= 0 || queue->max_size <= 0)
    {
        *status = -1; // Marca el estado como error
        synchro(queue, false); // Sincroniza el proceso con error
        return (fprintf(stderr, "[ERROR][process_manager] Arguments not valid.\n"), status);
    }

    synchro(queue, true); // Sincroniza el proceso exitoso

    if (queue_init(queue, queue->max_size) == -1) // Inicializa la cola
    {
        *status = -1; // Marca el estado como error
        return (fprintf(stderr, "[ERROR][process_manager] There was an error executing process_manager with id %d\n", queue->id), status);
    }
    printf("[OK][process_manager] Belt with id %d has been created with a maximum of %d elements.\n", queue->id, queue->max_size);

    // Crea los hilos productor y consumidor
    safe_thread(&threads[0], producer, queue, NULL, CREATE);
    safe_thread(&threads[1], consumer, queue, NULL, CREATE);

    // Espera a que el hilo productor termine
    if ((ret = pthread_join(threads[0], NULL)))
    {
        *status = -1; // Marca el estado como error
        return (fprintf(stderr, "[ERROR][process_manager] There was an error executing process_manager with id %d\n", queue->id), status);
    }
    // Espera a que el hilo consumidor termine
    if ((ret = pthread_join(threads[1], NULL)))
    {
        *status = -1; // Marca el estado como error
        return (fprintf(stderr, "[ERROR][process_manager] There was an error executing process_manager with id %d\n", queue->id), status);
    }

    queue_destroy(queue); // Destruye la cola
    printf("[OK][process_manager] Process_manager with id %d has produced %d elements.\n", queue->id, queue->num_created);

    return (status); // Retorna el estado del proceso
}
