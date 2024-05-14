/* Autores do projeto
        -Carlos Pereira 2022232042
        -Miguel Pereira 2022232552
*/

#include <sys/ipc.h>
#include <sys/shm.h>
#include <semaphore.h> 
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>
#include <sys/wait.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <pthread.h>

#define MAX_BUF 1024
#define BACK_PIPE "/tmp/back_pipe"


int main( ){
    sem_t *sem = sem_open("semaforo", O_CREAT | O_EXCL, 0700, 1);
    char comando[100];
    int fd_backoffice = open(BACK_PIPE, O_WRONLY);
    if (fd_backoffice == -1) {
        perror("open");
        return 1;
    }


    if(sem == SEM_FAILED){

        printf("Programa já a correr\n");
        return 1;
    }
    

    while(1){
        printf("\nComando ( lista ):  \n");
        fgets(comando, 100, stdin);
        comando[strlen(comando) - 1] = '\0';

        if(strcmp(comando, "exit") == 0){
            sem_close(sem);
            sem_unlink("semaforo");
            return 0;
        }else if (strcmp(comando, "data_stats") == 0){
            write(fd_backoffice, "data_stats", strlen("data_stats") + 1);
    
        }else if (strcmp(comando, "reset") == 0){
            write(fd_backoffice, "reset", strlen("reset") + 1); 
            
        }else if(strcmp(comando, "lista") == 0){
            printf(" \nComandos disponíveis: \n");
            printf("    data_stats\n");
            printf("    reset\n");
            printf("    exit\n");
        }else{
            printf("Comando inválido\n");
        }

    }

    
    sem_close(sem);
    sem_unlink("semaforo");
    

    return 0;
}