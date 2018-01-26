#define _XOPEN_SOURCE

#include "smallsh.h"
#include <sys/types.h>
#include <signal.h>
#include <stdlib.h>
#include <stdio.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>

char *prompt = "Dare un comando>";
struct sigaction sigStruct;

int procline(void) 	/* tratta una riga di input */
{
    char *arg[MAXARG+1];	/* array di puntatori per runcommand */
    int toktype;  	/* tipo del simbolo nel comando */
    int narg;		/* numero di argomenti considerati finora */
    int type;		/* FOREGROUND o BACKGROUND */
    int redirectType = STANDARD_OUTPUT; /* Tipo di redirect da applicare al
                                            comando in corso */
    char * filePath = NULL; /* Percorso del file su cui effettuare il redirect */
    
    narg=0;

    while (1) {	/* ciclo da cui si esce con il return */
	
        /* esegue un'azione a seconda del tipo di simbolo */
        
        /* mette un simbolo in arg[narg] */

        switch (toktype = gettok(&arg[narg])) {
	
        /* se argomento: passa al prossimo simbolo */
        case ARG:
            /* Verifica se il tipo di redirect è diverso dallo standard output e
                se il percorso del file su cui fare il redirect è NULL.
                Se la condizione è rispettata allora imposta l'argomento attuale
                come file su cui fare il redirect e non incrementa narg */
		    if (redirectType != STANDARD_OUTPUT && filePath == NULL) {
                filePath = arg[narg];
                break;
		    }
		    
            if (narg < MAXARG)
                narg++;
            break;

      /* se fine riga o ';' o '&' esegue il comando ora contenuto in arg,
	 mettendo NULL per segnalare la fine degli argomenti: serve a execvp */
        case EOL:
        case SEMICOLON:
        case AMPERSAND:
            type = (toktype == AMPERSAND) ? BACKGROUND : FOREGROUND;
      
            if (narg != 0) {
                /* Verifica se il tipo di redirect è diverso dallo standard output e
                se il percorso del file su cui fare il redirect è NULL.
                Se la condizione è rispettata allora ritorna avvisando che non
                è stato impostato il file su cui fare il redirect. */
                if (redirectType != STANDARD_OUTPUT && filePath == NULL) {
                    printf("File di destinazione del redirezionamento assente\n");
                    return 0;
                }
                arg[narg] = NULL;
                runcommand(arg, type, redirectType, filePath);
            }
      
            /* se fine riga, procline e' finita */
            
            if (toktype == EOL) return 1;
            
            /* altrimenti (caso del comando terminato da ';' o '&') 
                bisogna ricominciare a riempire arg dall'indice 0,
                viene reimpostato anche il tipo di redirect allo standard output
                e il percorso del file di redirect a NULL */
            
            narg = 0;
            redirectType = STANDARD_OUTPUT;
            filePath = NULL;
            break;
        case ARROW:
        case DOUBLE_ARROW:
            /* Se il simbolo è '>' o '>>', allora imposta il tipo di redirect 
                a, rispettivamente, new_file o append */
            redirectType = (toktype == ARROW) ? NEW_FILE : APPEND_FILE;
            break;
        }
    }
}

void runcommand(char **cline, int where, int redirectType, char* filePath)	/* esegue un comando */
{
    pid_t pid;
    int exitstat, ret;
    int fd;

    pid = fork();
    if (pid == (pid_t) -1) {
        perror("smallsh: fork fallita");
        return;
    }

    if (pid == (pid_t) 0) { 	/* processo figlio */

        /* esegue il comando il cui nome e' il primo elemento di cline,
           passando cline come vettore di argomenti */
           
        /* Ignora SIGINT se il processo è in background */
        if (where == BACKGROUND) {
            sigStruct.sa_handler = SIG_IGN;
            sigaction(SIGINT, &sigStruct, NULL);
            printf("[%d] Avviato\n", getpid());
        }
        
        /* Se il tipo di redirect è diverso dallo standard output, allora
        creo un nuovo file 'fd' con nome 'filePath' e coi permessi 'NEW_FILE_PERMISSIONS'
        se il tipo di redirect è NEW_FILE.
        Altrimenti apro il file 'filePath' in sola scrittura e in append,
        se non esiste lo crea con i permessi 'NEW_FILE_PERMISSIONS', e lo assegna
        a 'fd' */
        if (redirectType != STANDARD_OUTPUT) {
            if (redirectType == NEW_FILE) {
                fd = creat(filePath, NEW_FILE_PERMISSIONS);
            } else {
                fd = open(filePath, O_WRONLY | O_APPEND | O_CREAT, NEW_FILE_PERMISSIONS);
            }
            /* TODO Controllare che il controllo funzioni! */
            if (fd < 0) {
                fprintf(stderr, "Impossibile impostare il file '%s' come redirect: ", filePath);
                perror("");
                exit(1);
            }
            /* Imposta dup2 in modo da fare il redirect dello standard output
                su 'fd' */
            dup2(fd, STDOUT_FILENO);
            
            /* Chiudo il file */
            if (close(fd) != 0) {
                perror("close");
            }
        }
    
        execvp(*cline,cline);
        perror(*cline);
        
        exit(1);
    }

    /* processo padre: avendo messo exec e exit non serve "else" */
    
    if (where == FOREGROUND) {
        /* Ignoro SIGINT sul padre se in foreground */
        sigStruct.sa_handler = SIG_IGN;
        sigaction(SIGINT, &sigStruct, NULL);
        
        /* Aspetto che termina il processo figlio */
        ret = waitpid(pid, &exitstat, 0);
        if (ret == -1)
            perror("wait");
        if (WIFSIGNALED(exitstat))
            printf("\nComando terminato forzatamente da un segnale.\n");
        
        /* Ripristino SIGINT al termine del comando */
        sigStruct.sa_handler = killMessage;
        sigaction(SIGINT, &sigStruct, NULL);
    }
    
    /* Per ogni processo figlio in esecuzione, chiamo in waitpid con PID
        generico (-1), se il valore ritornato è maggiore di 0 allora processo
        è terminato e posso controllare se è uscito normalmente (WIFEXITED' o
        forzatamente 'WIFSIGNALED' */
    
    while ((ret = waitpid(-1, &exitstat, WNOHANG)) > 0) {
        printf("[%5d] Processo terminato ", ret);
        if (WIFEXITED(exitstat))
            printf("correttamente.\n");
        else if (WIFSIGNALED(exitstat))
            printf("forzatamente da un segnale.\n");
    }
}

/* Metodo chiamato quando viene generato un segnale di tipo SIGINT */
void killMessage(int sig) {
    printf("\nSIGINT Ricevuto - Interruzione.\n");
}

int main() {
    /* Inizializzo la gestione dei segnali */
    sigStruct.sa_handler = killMessage;
    sigemptyset(&sigStruct.sa_mask);
    sigStruct.sa_flags = 0;
    sigaction(SIGINT, &sigStruct, NULL);
    
    while(userin(prompt) != EOF)
        procline();
        
    return 0;
}