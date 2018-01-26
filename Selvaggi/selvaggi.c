#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <semaphore.h>

/* Versione estesa dell'esercizio dei selvaggi. Per risolvere il problema della
sincronizzazione sono stati utilizzati dei semafori privati per ogni selvaggio
e un manager, che viene risvegliato dal selvaggio al termine del suo ciclo, si
occupa di scegliare quale tra i selvaggi in attesa svegliare per accedere alla
pentola. Per gestire gli stati di ogni selvaggio si è scelto di usare un array
di interi (si poteva fare anche con una lista, ma sarebbe diventato più pesante 
e complicato) dove ogni elemento dell'array corrisponde al relativo selvaggio
con lo stesso indice. Gli stati sono:
 0 - Attivo: Il selvaggio non è in attesa di mangiare
 1 - Attesa: Il selvaggio è in attesa di mangiare
 2 - Terminato: Il selvaggio ha terminato i suoi cicli e quindi si è terminato
Il manager continua a scorrere l'array di stati fino a quando tutti i selvaggi
sono in stato 2 o fino a quando non ne trova uno in attesa (1), a quel punto lo
risveglia e si mette in attesa che il selvaggio lo risvegli.
In quanto il manager può svegliare uno e un solo selvaggio alla volta, non è
necessario usare un mutex per acceso esclusivo alla pentola.
I selvaggi vengono suddivisi tra Guerrieri e Servitori a seconda del loro ID,
se sono minori della metà allora sono Guerrieri, altrimenti sono Servitori */

/* Variabili linea di comando */
int N_SAVAGES;
int N_PORTIONS;
int N_CYCLES;

int portions;           /* Porzioni disponibili nella pentola */
int* waitingSavages;    /* Array dello stato dei selvaggi */
int savagesTerminated;  /* Diverso da 0 se tutti i selvaggi sono terminati */
int fillingTimes = 0;   /* Contatore del numero di volte che la pentola è stata riempita */
sem_t* savages_sem;     /* Array variabile che contiene tutti i semafori privati */
sem_t empty_sem;        /* Semaforo su cui fare UP quando la pentola è vuota */
sem_t full_sem;         /* Semaforo su cui il cuoco fa UP quando la pentola è stata riempita */
sem_t manager_sem;    /* Semaforo su cui fare l'UP per risvegliare un selvaggio in attesa di mangiare */
pthread_t* savage;      /* Array variabile che contiene tutti i thread dei selvaggi */
pthread_t cook;         /* Thread del cuoco */
pthread_t manager;      /* Thread del manager */


void* manager_body(void *args) {
    int activeThreads = 1;
    int i;
    /* Rimane in loop fino a che non ci sono più selvaggi attivi */
    while (activeThreads != 2) {
        /* Attende un selvaggio che lo risvegli */
        sem_wait(&manager_sem);
        /* Ciclo che si ripete fino a che non viene trovato un selvaggio in
        attesa di mangiare o fino a che tutti i selvaggi non sono terminati */
        for (i = 0; 1; i++) {
            if (i > N_SAVAGES-1) {
                /* Se al termine di un ciclo, non è stato trovato un selvaggio
                non ancora terminato, allora vuol dire che tutti i selvaggi sono
                terminati e si termina anche il manager */
                if (activeThreads != 0) {
                    printf("[Manager] Tutti i selvaggi terminati - termino\n");
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
            
            /* Quando viene trovato un selvaggio in attesa, esso viene svegliato
            e il manager termina il ciclo interno */
            if (waitingSavages[i] == 1) {
                printf("[Manager] Risvegliato servitore [%d]\n", i);
                /* Prima di svegliarlo, imposta il suo stato a 0, in modo da
                evitare possibili problemi di race condition */
                waitingSavages[i] = 0;
                sem_post(&savages_sem[i]);
                break;
            }
        }
    }
    pthread_exit(0);
}

/* Funzione per ottenere il tipo di selvaggio, se il selvaggio è nella prima
metà di essi, allora è un guerriero, altrimenti è un servitore */
char* savageType(int id) {
    if (id+1 > N_SAVAGES / 2) {
        return "Servitore";
    } else {
       return "Guerriero";
    }
}

/* Il servitore non ha fame (per il nome si è preso spunto dal problema dei
filosofi)*/
void think(int id) {
    int i;
    printf("[%2d] - %s - Non ha fame\n", id, savageType(id));
    
    for (i = 0; i < 100000000 + 20000000 * id; i++);
}

/* Il selvaggio mangia */
void eat(int id) {
    int i;
    printf("[%2d] - %s - Mangiando [porzioni rimaste: %d]\n", id, savageType(id), portions-1);
    for (i = 0; i < 1000000; i++);
}

/* Corpo del selvaggio */
void* savages_body(void* args) {
    int id;
    int cycles;
    
    id = *(int*)args;
    printf("ID del selvaggio: %d - Tipo: %s\n", id, savageType(id));
    
    free(args); /* Libero lo spazio allocato dal malloc nel main */
    
    for (cycles = 0; cycles < N_CYCLES; cycles++) {
        /* Pensa (stato impostato a 0) */
        think(id);
        
        /* Al selvaggio viene fame, lo stato viene impostato a 1 e va in attesa
        sul proprio semaforo privato */
        waitingSavages[id] = 1;
        sem_wait(&savages_sem[id]);
        /* Se non ci sono più porzioni, sveglia il cuoco ed aspetta che finisca */
        if (portions == 0) {
            printf("[%2d] - %s - Porzioni terminate, risveglio il cuoco\n", id, savageType(id));
            sem_post(&empty_sem);
            sem_wait(&full_sem);
        }
        eat(id);
        portions--;
        
        printf("[%2d] - %s - Ciclo terminato - cicli: %d/%d\n", id, savageType(id), cycles+1, N_CYCLES);
        
        /* Al termine del ciclo, sveglia manager prima di ricominciare a pensare
        in modo che possa svegliare un altro selvaggio */
        sem_post(&manager_sem);
    }
    
    /* Quando i cicli sono terminati, lo stato del selvaggio viene impostato a
    2 (terminato) e il thread termina */
    waitingSavages[id] = 2;
    printf("[%2d] - %s - Terminato\n", id, savageType(id));
    pthread_exit(0);
}

/* Corpo del cuoco */
void* cook_body(void* args) {
    while(1) {
        /* Si mette in attesa che un selvaggio lo svegli */
        sem_wait(&empty_sem);
        /* Se la variabili savagesTerminated è impostata ad 1, allora vuol
        dire che tutti i selvaggi sono terminati, e quindi anche il cuoco
        si termina */
        if (savagesTerminated == 1) {
            break;
        }
        
        /* Se le porzioni sono terminate, allora riempe la pentola */
        if (portions == 0) {
            printf("[Cuoco] Riempendo pentola\n");
            portions = N_PORTIONS;
            fillingTimes++;
        } else {
            printf("[Cuoco] ERRORE - Pentola non vuota\n");
        }
        /* Al termine, sveglia il selvaggio in attesa */
        sem_post(&full_sem);
    }
    
    printf("Cuoco - Terminato\n");
    pthread_exit(0);
}

int main(int argc, char* argv[]) {
    int i;
    void **retval = NULL;
    int errno;
    int* arg;
    
    /* Lettura argomenti */
    if (argc != 4) {
        printf("Argomenti dalla linea di comando errati\n");
        return 0;
    }
    
    /* Imposto il numero di selvaggi, le porzioni e i cicli a seconda degli
    input dalla linea di comando */
    N_SAVAGES = atoi(argv[1]);
    N_PORTIONS = atoi(argv[2]);
    N_CYCLES = atoi(argv[3]);
    printf("Numero selvaggi: %d\nNumero porzioni: %d\nNumero cicli: %d\n", N_SAVAGES, N_PORTIONS, N_CYCLES);
    
    /* Inizializzazione delle variabili e dei semafori */
    savagesTerminated = 0;
    sem_init(&empty_sem, 0, 0);
    sem_init(&full_sem, 0, 0);
    sem_init(&manager_sem, 0, 1); /* Il manager viene inizializzato a 1 in
                                    modo da partire subito senza dover essere
                                    svegliato */
    
    /* Alloco lo spazio per gli array variabili */
    savages_sem = malloc(sizeof(sem_t) * N_SAVAGES);
    waitingSavages = malloc(sizeof(int) * N_SAVAGES);
    savage = malloc(sizeof(pthread_t) * N_SAVAGES);
    
    /* Imposto lo stato di tutti i selvaggi a 0 (attivi) */
    for (i = 0; i < N_SAVAGES; i++) {
        sem_init(&savages_sem[i], 0, 0);
        waitingSavages[i] = 0;
    }
    
    portions = N_PORTIONS;  /* Riempo la pentola */
    
    /* Inizializzo i vari thread */
    printf("[MAIN] Creazione del cuoco\n");
    if (pthread_create(&cook, NULL, cook_body, NULL) != 0) {
        fprintf(stderr, "Errore nella creazione del cuoco\n");
    }
    
    /* Per il numero dei selvaggi è stato un problema inizializzarli con ID diversi
    in quanto se passavo l'indirizzo di una variabile id che incrementavo ad ogni
    ciclo, il selvaggio non ottiene l'id che gli è stato passato, ma il valore
    in cui si trova id nel main quando il selvaggio entra in esecuzione.
    Quindi ho deciso di istanziare uno spazio di memoria temporaneo per ogni thread
    che creo che poi verrà liberato dal thread quando avrà letto correttamente
    il valore dell'ID */
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
    
    /* Attendo che tutti i selvaggi terminino */
    for (i = 0; i < N_SAVAGES; i++) {
        errno = pthread_join(savage[i], retval);
        if (errno != 0) {
            printf("Errore nel terminare il selvaggio %d con errore %d\n", i, errno);
        }
        printf("[MAIN] Terminato il selvaggio [%d]\n", i);
    }
    printf("[MAIN] Terminati tutti i selvaggi\n");
    
    /* Aspetto che termini il manager */
    errno = pthread_join(manager, retval);
    if (errno != 0) {
        fprintf(stderr, "Errore nel terminare il manager con errore %d\n", errno);
    }
    printf("[MAIN] Manager terminato\n");
    
    /* Imposto savagesTerminated a 1 per dire al cuoco che tutti i selvaggi
    sono terminati e lo risveglio, infine, aspetto che termini anche lui */
    savagesTerminated = 1;
    sem_post(&empty_sem);
    errno = pthread_join(cook, retval);
    if (errno != 0) {
        fprintf(stderr, "Errore nel terminare il cuoco con errore %d\n", errno);
    }
    printf("[MAIN] Cuoco terminato\n");
    
    /* Stampo il numero di volte che è stata riempita la pentola */
    printf("[MAIN] Il cuoco ha riempito la pentola %d volte\n", fillingTimes);
    
    return 0;
}