#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/wait.h>

#define EOL 1 		/* end of line */
#define ARG 2		/* argomento normale */
#define AMPERSAND 3 	/* & */
#define SEMICOLON 4	/* ; */
#define ARROW 6     /* > */
#define DOUBLE_ARROW 7  /* >> */

#define MAXARG 512	/* numero massimo di argomenti */
#define MAXBUF 512	/* lunghezza massima riga di input */
#define NEW_FILE_PERMISSIONS S_IRUSR | S_IWUSR  /* Permessi del file creato per il redirezionamento dell'output */

#define	FOREGROUND 0
#define	BACKGROUND 1

#define STANDARD_OUTPUT 10  /* Tipo di redirect di default, il terminale */
#define NEW_FILE 11         /* Tipo di redirect su nuovo file: '>' */
#define APPEND_FILE 12      /* Tipo di redirect append su file esistente '>>' */

int inarg(char c);		/* verifica se c non e' un carattere speciale */

int userin(char *p);		/* stampa il prompt e legge una riga */	

int gettok(char **outptr);	/* legge un simbolo */

int procline();			/* tratta una riga di input */

void runcommand(char **cline,int where, int redirectType, char* filePath);	/* esegue un comando */

void killMessage(int sig);     /* Stampa un messaggio di avviso */