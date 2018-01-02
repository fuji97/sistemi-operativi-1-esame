#define _XOPEN_SOURCE

#include "smallsh.h"
#include <sys/types.h>
#include <signal.h>
#include <stdlib.h>
#include <stdio.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>

char *prompt = "Dare un comando >";
int bgProcess = 0;
struct sigaction sigStruct;

int procline(void) 	/* tratta una riga di input */
{
    char *arg[MAXARG+1];	/* array di puntatori per runcommand */
    int toktype;  	/* tipo del simbolo nel comando */
    int narg;		/* numero di argomenti considerati finora */
    int type;		/* FOREGROUND o BACKGROUND */
    int redirectType = STANDARD_OUTPUT;
    char * filePath = NULL;
    int porco;      /* TODO - DELETE THIS SHIT */
    
    narg=0;

    while (1) {	/* ciclo da cui si esce con il return */
	
        /* esegue un'azione a seconda del tipo di simbolo */
        
        /* mette un simbolo in arg[narg] */

        switch (toktype = gettok(&arg[narg])) {
	
        /* se argomento: passa al prossimo simbolo */
        case ARG:
		    if (redirectType != STANDARD_OUTPUT && filePath == NULL) {
                filePath = arg[narg];
                printf("File path: %s\n", filePath);
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
                if (redirectType != STANDARD_OUTPUT && filePath == NULL) {
                    printf("File di destinazione del redirezionamento assente\n");
                    return 0;
                }
                arg[narg] = NULL;
                printf("Lancio comandi:\n");
                
                for (porco = 0; porco < narg; porco++) {
                    printf("%s ", arg[porco]);
                }
                printf("\n");
                runcommand(arg, type, redirectType, filePath);
            }
      
            /* se fine riga, procline e' finita */
            
            if (toktype == EOL) return 1;
            
            /* altrimenti (caso del comando terminato da ';' o '&') 
                bisogna ricominciare a riempire arg dall'indice 0 */
            
            narg = 0;
            redirectType = STANDARD_OUTPUT;
            filePath = NULL;
            break;
        case ARROW:
        case DOUBLE_ARROW:
            printf("Trovato redirect\n");
            redirectType = (toktype == ARROW) ? NEW_FILE : APPEND_FILE;
            break;
        }
    }
}

void runcommand(char **cline, int where, int redirectType, char* filePath)	/* esegue un comando */
{
    pid_t pid;
    int exitstat, ret, i;
    int fd;

    printf("Runcommand with filePath: %s\n", filePath);
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
        }
        
        if (redirectType != STANDARD_OUTPUT) {
            if (redirectType == NEW_FILE) {
                fd = creat(filePath, NEW_FILE_PERMISSIONS);
            } else {
                fd = open(filePath, O_WRONLY | O_APPEND | O_CREAT, NEW_FILE_PERMISSIONS);
            }
            /* TODO Controllare che il controllo funzioni! */
            if (fd < 0) {
                perror("open");
                exit(1);
            }
            dup2(fd, STDOUT_FILENO);
        }
    
        execvp(*cline,cline);
        perror(*cline);
        
        
        /* Provo a fare il flush 
        if (redirectType != STANDARD_OUTPUT) {
            fflush(stdout);
        }
        */
        if (redirectType != STANDARD_OUTPUT) {
            if (close(fd) != 0) {
                perror("close");
            }
        }
        
        exit(1);
    }

    /* processo padre: avendo messo exec e exit non serve "else" */
    
    /* la seguente istruzione non tiene conto della possibilita'
     di comandi in background  (where == BACKGROUND) */
    if (where == FOREGROUND) {
        /* Ignoro SIGINT sul padre se in foreground */
        sigStruct.sa_handler = SIG_IGN;
        sigaction(SIGINT, &sigStruct, NULL);
        
        ret = waitpid(pid, &exitstat, 0);
        if (ret == -1)
            perror("wait");
        
        /* Ripristino SIGINT al termine del comando */
        sigStruct.sa_handler = killMessage;
        sigaction(SIGINT, &sigStruct, NULL);
    } else {
        bgProcess++;
        printf("[%5d] - Processo avviato in background\n", pid);
    }
    
    for (i = bgProcess; i > 0; i--) {
        ret = waitpid(-1, &exitstat, WNOHANG);
        if (ret > 0) {
            printf("[%5d] Processo terminato ", ret);
            if (WIFEXITED(exitstat))
                printf("correttamente.\n");
            else if (WIFSIGNALED(exitstat))
                printf("forzatamente.\n");
                
            bgProcess--;
        }
    }
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

void killMessage(int sig) {
    printf("\nSIGINT Ricevuto - Interruzione.\n");
}

void shiftBackElements(char ** array) {
    char ** pointer = array;
    while (pointer != NULL) {
        /* pointer = ++pointer; DISABLED */
    }
}