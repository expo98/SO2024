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
#include <signal.h>

#define MAX_BUF 1024
#define USER_PIPE "/tmp/user_pipe"
#define BACK_PIPE "/tmp/back_pipe"


typedef struct 
{
    int MOBILE_USER;
    int QUEUE_POS;
    int AUTH_SERVERS;
    int AUTH_PROC_TIME;
    int MAX_VIDEO_WAIT;
    int MAX_OTHERS_WAIT;
} Config;



typedef struct no {
    char *s;
    struct no* prox;
} no;

typedef struct fila{
    struct no* inicio;
    struct no* fim; 
    int tamanho;
}fila;

typedef struct filas{
    fila Video_Streaming_Queue;
    fila Others_Services_Queue;
    int QUEUE_POS;
} filas;

typedef struct {
    filas* f;
    int fd;
    int fd_backoffice;
    int fd_pipe_write;
} thread_args;

void cria_fila(fila* f){
    f->inicio = NULL;
    f->fim = NULL;
    f->tamanho = 0;
}


no* insere_fila(fila *f, char *s, int QUEUE_POS){
    if(f->tamanho == QUEUE_POS){
        return NULL;
    }

    no* n = malloc(sizeof(no));
    if(n != NULL){
        n->s = malloc(sizeof(char) * (strlen(s)+1));
        if( n->s == NULL){
            free(n);
            return NULL;
        }
        strcpy(n->s, s);
        n->prox = NULL;
        if(f->fim != NULL){
            f->fim->prox = n;
            f->fim = n;
        } else{
            f->inicio = n;
            f->fim = n;
        }
        f->tamanho++;
    }
    return n;
}

void destroi_fila(fila *f){
    while(f->inicio != NULL){
        no* aux = f->inicio;
        f->inicio = f->inicio->prox;
        free(aux);
    }
    f->fim = NULL;
}

int vazia_fila(fila *f){
    return f->inicio == NULL;
}

char* retira_fila(fila *f){
    if( f->inicio != NULL){
        char *res = f->inicio->s;
        no* temp = f->inicio;
        f->inicio = f->inicio->prox;
        free(temp);
        f->tamanho--;
        return res;
    }else {
        return 0;
    }
}



void log_msg(char *msg){
    FILE *file = fopen("log.txt", "a");
    if(file == NULL){
        printf("Erro ao abrir o ficheiro log\n");
        return ;
    }

    time_t tempo = time(NULL);
    char tempo_str[100];
    strftime(tempo_str, 100, "%Y-%m-%d %H:%M:%S", localtime(&tempo));
    fprintf(file, "%s - ", tempo_str);
    fprintf(file, "%s\n", msg);
    printf("%s - ", tempo_str);
    printf("%s\n", msg);
    fclose(file);
}


void* receiver_thread(void* args){
    log_msg("Criação da thread receiver");

    thread_args *t_args = (thread_args*)args;
    int fd = t_args->fd;
    int fd_backoffice = t_args->fd_backoffice;
    filas *f = t_args->f;


    char buf[MAX_BUF];

    fd_set set;
    FD_ZERO(&set); /* clear the set */
    FD_SET(fd, &set); /* add our file descriptor to the set */
    FD_SET(fd_backoffice, &set);

    while(1){
        fd_set temp_set = set;
        if (select(FD_SETSIZE, &temp_set, NULL, NULL, NULL) == -1) {
            perror("select");
            return NULL;
        }

        for(int i = 0; i < FD_SETSIZE; ++i) {
            if (FD_ISSET(i, &temp_set)) {
                ssize_t bytes_read = read(i, buf, MAX_BUF);
                if(bytes_read== -1){
                    log_msg("Erro ao ler do named_pipe");
                    return NULL;
                }else if(bytes_read == 0){
                    log_msg("Fim do named_pipe");
                    close(i);
                    if (i == fd) {
                        unlink(USER_PIPE);
                    } else if (i == fd_backoffice) {
                        unlink(BACK_PIPE);
                    }
                    FD_CLR(i, &set);
                } else {
                    buf[bytes_read] = '\0';
                    printf("Recebido: %s\n", buf);
                 }
            }
        }
    }
    
    


    /*if(insere_fila(&f->Video_Streaming_Queue, pedidos, f->QUEUE_POS) == NULL){
        log_msg("Video_Streaming_Queue cheia");
        return NULL;
    }else if (insere_fila(&f->Others_Services_Queue, pedidos, f->QUEUE_POS) == NULL){
       log_msg("Others_Services_Queue cheia");
        return NULL;
    } */

    return NULL;

}
// Sender_thread
void* sender_thread(void* args){
    log_msg("Criação da thread sender");

    thread_args *t_args = (thread_args*)args;

    
    
    fila VIDEO_QUEUE = t_args->f->Video_Streaming_Queue;
    fila OTHERS_QUEUE = t_args->f->Others_Services_Queue;
    no temp_no;
    while (VIDEO_QUEUE.tamanho>0)
    {
        write(t_args->fd_pipe_write,VIDEO_QUEUE.inicio->s,sizeof(VIDEO_QUEUE.inicio->s));
        temp_no = *VIDEO_QUEUE.inicio->prox;
        *VIDEO_QUEUE.inicio = temp_no;
        VIDEO_QUEUE.tamanho--;
    }
    while (OTHERS_QUEUE.tamanho>0)
    {
        write(t_args->fd_pipe_write,OTHERS_QUEUE.inicio->s,sizeof(OTHERS_QUEUE.inicio->s));
        temp_no = *OTHERS_QUEUE.inicio->prox;
        *OTHERS_QUEUE.inicio = temp_no;
        OTHERS_QUEUE.tamanho--;
    }
    
    
    return NULL;
}


void sigint_handler(int sig) {
    // Delete the named pipe
    if (unlink(USER_PIPE) == -1) {
        perror("unlink");
    }

    if (unlink(BACK_PIPE) == -1) {
        perror("unlink");
    }
    exit(0);
}
 

int main(int argc, char *argv[]) {
    struct sigaction sa;
    sa.sa_handler = sigint_handler;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = 0;
    if (sigaction(SIGINT, &sa, NULL) == -1) {
        perror("sigaction");
        return 1;
    }

    log_msg("Início do programa");
    char *filename;

    if(argc != 2) {
        printf("Syntax Inválida: ./system_manager  {config-file}\n");
        return 1;
    }

    filename = argv[1];
    FILE *file = fopen(filename, "r");
    if(file == NULL) {
        log_msg("Erro ao abrir o ficheiro de configuração");
        return 1;
    }

    Config config = {0,0,0,0,0,0};


    if(fscanf(file, "%d %d %d %d %d %d", &config.MOBILE_USER, &config.QUEUE_POS, &config.AUTH_SERVERS, &config.AUTH_PROC_TIME, &config.MAX_VIDEO_WAIT, &config.MAX_OTHERS_WAIT) != 6) {
       log_msg("Erro ao ler o ficheiro de configuração");
        return 1;
    }

   /*
    printf("MOBILE_USER: %d\n", config.MOBILE_USER);
    printf("QUEUE_POS: %d\n", config.QUEUE_POS);
    printf("AUTH_SERVERS: %d\n", config.AUTH_SERVERS);
    printf("AUTH_PROC_TIME: %d\n", config.AUTH_PROC_TIME);
    printf("MAX_VIDEO_WAIT: %d\n", config.MAX_VIDEO_WAIT);
    printf("MAX_OTHERS_WAIT: %d\n", config.MAX_OTHERS_WAIT);
    */
    fclose(file);

    int shmid = shmget(IPC_PRIVATE, sizeof(int), IPC_CREAT  | 0777);
    if( shmid < 0){
        log_msg("Erro ao criar a shared memory");
        return 1;
    }

    int *shared_var = (int *)shmat(shmid,NULL, 0);

    int unnamedpipefd[2];
    if(pipe(unnamedpipefd) == -1){
        perror("Errocor creating unnamed pipe");
        return 1;
    }

    pid_t pid = fork();
    if(pid  < 0){
        log_msg("Erro ao criar processo Auth Request Manager");
        return 1;  

    }else if(pid == 0){ // Processo filho, referente ao Auth Request Manager
    log_msg("Processo Auth Request Manager criado");


    if (mkfifo(USER_PIPE, O_CREAT | O_EXCL | 0600 ) < 0){
        log_msg("Erro ao criar o named user_pipe");
        return 1;
    }


    char buf[MAX_BUF];
    int fd = open(USER_PIPE, O_RDWR);
    if(fd < 0){
        log_msg("Erro ao abrir o named user_pipe");
        return 1;
    }

    
    

    if (mkfifo(BACK_PIPE, O_CREAT | O_EXCL | 0600 ) < 0){
    perror("Error creating back_pipe");
    return 1;
}

printf("Back_pipe was created successfully.\n");
fflush(stdout);

int fd_backoffice = open(BACK_PIPE, O_RDWR);
if(fd_backoffice < 0){
    perror("Error opening back_pipe");
    return 1;
}

printf("Back_pipe was opened successfully.\n");
fflush(stdout);

    
    pthread_t thr_receiver, thr_sender;

    filas f;

    f.QUEUE_POS = config.QUEUE_POS;
    cria_fila(&f.Video_Streaming_Queue);
    cria_fila(&f.Others_Services_Queue);

    thread_args args;
    args.f = &f;
    args.fd = fd;
    args.fd_backoffice = fd_backoffice;
    args.fd_pipe_write = unnamedpipefd[1]; // Passa o write end do pipe para o sender thread
    close(unnamedpipefd[0]);// Fecha o read end do pipe no Auth Req Manager

    pthread_create(&thr_receiver, NULL, receiver_thread, &args);
    pthread_create(&thr_sender, NULL, sender_thread, &args);

    //

    pthread_join(thr_receiver, NULL);
    pthread_join(thr_sender, NULL);
    return 0;
    }


    pid_t pid2 = fork();
    if(pid2 < 0){
        log_msg("Erro ao criar processo Monitor Engine");
        return 1;
    }else if(pid2 == 0){ // Processo filho, referente ao Monitor Engine
        char* buf[BUFSIZ];
        log_msg("Processo Monitor Engine criado");
        log_msg("Limite plafond atingido");
        close(unnamedpipefd[1]); // Fecha o pipe end de writing no Monitor Engine visto que irá apenas ler
        read(unnamedpipefd[0],buf,BUFSIZ);
        
        
        return 0;
    }



    log_msg("Processo System Manager criado");

    waitpid(pid, NULL, 0);
    waitpid(pid2, NULL, 0);

    



    shmdt(shared_var);
    log_msg("Fim do programa");
    return 0;
    }
