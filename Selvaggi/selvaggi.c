#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <semaphore.h>

#define N_SAVAGES 7
#define N_PORTIONS 3
#define N_CYCLES 14

/* Distruggere i semafori */

int portions = 0;
int waitingSavages[N_SAVAGES];
int savagesTerminated;
sem_t portions_mutex;
sem_t savages_mutex[N_SAVAGES];
sem_t empty_mutex;
sem_t full_mutex;
sem_t manager_mutex;
pthread_t savage[N_SAVAGES];
pthread_t cook;
pthread_t manager;


void* manager_body(void *args) {
    int activeThreads = 1;
    int i = 0;
    while (activeThreads != 2) {
        sem_wait(&manager_mutex);
        for (i = 0; 1; i++) {
            if (i > N_SAVAGES-1) {
                if (activeThreads != 0) {
                    printf("Manager - Tutti gli altri servitori terminati\n");
                    activeThreads = 2;
                    break;
                } else {
                    i = 0;
                    activeThreads = 1;
                }
            } 
            if (waitingSavages[i] != 2) {
                activeThreads = 0;
            }
            
            if (waitingSavages[i] == 1) {
                sem_post(&savages_mutex[i]);
                printf("Risvegliato servitore [%d]\n", i);
                break;
            }
        }
    }
    pthread_exit(0);
}

void think(int id) {
    int i;
    if (id+1 > N_SAVAGES / 2) {
        printf("[%2d] Servitore - Pensando\n", id);
    } else {
        printf("[%2d] Guerriero - Pensando\n", id);
    }
    
    for (i = 0; i < 30000000 + 50000000 * id; i++);
}

void eat(int id) {
    int i;
    if (id+1 > N_SAVAGES / 2) {
        printf("[%2d] Servitore - Mangiando [porzioni rimaste: %d]\n", id, portions-1);
    } else {
        printf("[%2d] Guerriero - Mangiando [porzioni rimaste: %d]\n", id, portions-1);
    }
    for (i = 0; i < 1000000; i++);
}

void* savages_body(void* args) {
    int id;
    int i;
    int cycles;
    /* int activeThreads; */
    
    id = *(int*)args;
    printf("ID del selvaggio: %d\n", id);
    free(args);
    for (cycles = 0; cycles < N_CYCLES; cycles++) {
        think(id);
        waitingSavages[id] = 1;
        sem_wait(&savages_mutex[id]);
        waitingSavages[id] = 0;
        if (portions == 0) {
            printf("[%2d] - Selvaggio - Porzioni terminate, chiamo il fottuto cuoco comunista di merda\n", id);
            sem_post(&empty_mutex);
            sem_wait(&full_mutex);
        }
        eat(id);
        portions--;
        
        printf("[%2d] Servitore - Inizio risveglio altri servitori", id);
        printf(" [");
        for (i = 0; i < N_SAVAGES; i++) {
            printf("%d ", waitingSavages[i]);
        }
        printf("]\n");
        printf("[%2d] Ciclo terminato - cicli: %d/%d\n", id, cycles+1, N_CYCLES);
        sem_post(&manager_mutex);
    }
    
    waitingSavages[id] = 2;
    printf("[%2d] Terminato\n", id);
    pthread_exit(0);
}

void* cook_body(void* args) {
    while(1) {
        sem_wait(&empty_mutex);
        if (savagesTerminated == 1) {
            break;
        }
        printf("Cuoco - riempendo porzioni\n");
        portions = portions + N_PORTIONS;
        sem_post(&full_mutex);
    }
    
    printf("Cuoco - Terminato\n");
    pthread_exit(0);
}

int main() {
    int i;
    void **retval = NULL;
    int errno;
    int* arg;
    
    /* Inizializzazione */
    savagesTerminated = 0;
    sem_init(&portions_mutex, 0, 1);
    sem_init(&empty_mutex, 0, 1);
    sem_init(&full_mutex, 0, 1);
    sem_init(&manager_mutex, 0, 1);
    for (i = 0; i < N_SAVAGES; i++) {
        sem_init(&savages_mutex[i], 0, 1);
        waitingSavages[i] = 0;
    }
    
    printf("[MAIN] Creazione del cuoco\n");
    if (pthread_create(&cook, NULL, cook_body, NULL) != 0) {
        fprintf(stderr, "Errore nella creazione del cuoco\n");
    }
    
    for (i = 0; i < N_SAVAGES; i++) {
        arg = malloc(sizeof(int));
        *arg = i;
        printf("[MAIN] Creazione del selvaggio %d\n", *arg);
        if (pthread_create(&savage[i], NULL, savages_body, (void*) arg) != 0) {
            fprintf(stderr, "Errore nella creazione nel selvaggio %d\n", i);
        }
    }
    
    /* Avvia manager */
    if (pthread_create(&manager, NULL, manager_body, NULL) != 0) {
        fprintf(stderr, "Errore nella creazione del manager\n");
    }
    
    for (i = 0; i < N_SAVAGES; i++) {
        errno = pthread_join(savage[i], retval);
        if (errno != 0) {
            printf("Errore nel terminare il selvaggio %d con errore %d\n", i, errno);
        }
        printf("[MAIN] Terminato il selvaggio %d\n", i);
    }
    
    printf("[MAIN] Terminati tutti i selvaggi\n");
    
    savagesTerminated = 1;
    sem_post(&empty_mutex);
    if (pthread_join(cook, retval) != 0) {
        fprintf(stderr, "Errore in qualcosa di buono %d\n", i);
    }
    
    return 0;
}