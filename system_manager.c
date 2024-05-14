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
#include <stdbool.h>

#define MAX_BUF 1024
#define USER_PIPE "/tmp/user_pipe"
#define BACK_PIPE "/tmp/back_pipe"

#define DEBUG_PRINT(text)\
	printf(GREEN);\
	printf(text);\
	printf(DEFAULT);\

#define RED "\033[0;31m"
#define GREEN "\033[0;32m"
#define YELLOW "\033[0;33m"
#define BLUE "\033[0;36m"
#define DEFAULT "\033[0m"

#define DEBUG // Remove this line to remove debug messages

pthread_mutex_t queue_mutex = PTHREAD_MUTEX_INITIALIZER;

sem_t * LOG;
sem_t * SHM;
sem_t * MONITOR;
sem_t * MSQ;
sem_t * AUTH_ENG;

pid_t pid, pid2;

int teste;
volatile sig_atomic_t sigint_received = 0;
void *shared_memory; // Declare shared_memory as a global variable
int shmid; // Declare shmid as a global variable


typedef struct 
{
    int MOBILE_USER;
    int QUEUE_POS;
    int AUTH_SERVERS;
    int AUTH_PROC_TIME;
    int MAX_VIDEO_WAIT;
    int MAX_OTHERS_WAIT;
} Config;



typedef struct 
{
    int Plafond_inicial;
    int Plafond_atual;
    int ID;
}MobileUser;

typedef struct 
{
    int VideoData;
    int VideoReq;
    int MusicData;
    int MusicReq;
    int SocialData;
    int SocialReq;

} GeneralStats;

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
    pthread_mutex_t mutexFila;
} filas;

typedef struct {
    filas* f;
    int fd;
    int fd_backoffice;
    int (*pipefd)[2];
    int *auth_engine_status;
    Config config;
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
        }
        f->fim = n;
        if(f->inicio == NULL){
            f->inicio = n;
        }
        f->tamanho++;
    }
    //printf("\nInserted into queue: %s | SIze %d\n", n->s, f->tamanho);
        
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
        char *res = f->inicio->s;  // Just get the string directly
        //printf("Removing from queue: %s\n", res);  // Print the element being removed
        no* temp = f->inicio;
        f->inicio = f->inicio->prox;
        free(temp);  // Only free the node, not the string
        f->tamanho--;  // Decrement the queue size
        return res;
    }else {
        printf("Queue is empty but size is: %d\n", f->tamanho); // Add this line
        return 0;
    }
}

void imprime_fila(fila *f){
    no* aux = f->inicio;
    while(aux != NULL){
        printf("%s\n", aux->s);
        aux = aux->prox;
    }
}



void log_msg(char *msg){
    sem_wait(LOG);
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
    sem_post(LOG);
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
                    char type[10];
                    int id, value;
                    //printf("Received message: %s\n", buf);
                   
                    if(sscanf(buf, "%d#%d" , &id, &value) == 2){
                        pthread_mutex_lock(&f->mutexFila);
                        insere_fila(&f->Video_Streaming_Queue, buf, f->QUEUE_POS);
                        //printf("Inserted into Video queue: %s\n", buf);
                        pthread_mutex_unlock(&f->mutexFila);
                        }

                    if(sscanf(buf, "%d#%[^#]#%d", &id, type, &value) == 3 || strcmp(buf, "data_stats") == 0 || strcmp(buf, "reset") == 0){
                        pthread_mutex_lock(&f->mutexFila);
                        if(strcmp(type, "VIDEO") == 0){
                            if(insere_fila(&f->Video_Streaming_Queue, buf, f->QUEUE_POS) == NULL){
                                log_msg("Fila de Video cheia");
                            }else{
                                //printf("Inserted into Video queue: %s\n", buf);
                            }
                        } else {
                            if(insere_fila(&f->Others_Services_Queue, buf, f->QUEUE_POS) == NULL){
                                log_msg("Fila de Outros cheia");
                            }else{
                               //printf("Inserted into Others queue: %s\n", buf);
                            }
                        }
                        pthread_mutex_unlock(&f->mutexFila);
                 }
            }
        }
    }
    }
    return NULL;

}
// Sender_thread


void* sender_thread(void* args){

    log_msg("Criação da thread sender");

    thread_args *t_args = (thread_args*)args;

    filas *f = t_args->f;
    int (*pipefd)[2] = t_args->pipefd;
    int *auth_engine_status = t_args->auth_engine_status;
    Config config = t_args->config;

    while(1){
        pthread_mutex_lock(&f->mutexFila);
        while(f->Video_Streaming_Queue.tamanho > 0){
            char* removed_item = retira_fila(&f->Video_Streaming_Queue);
             
            for(int i =0; i < config.AUTH_SERVERS; i++){
                printf("%d\n", auth_engine_status[i]);
                if(auth_engine_status[i] == 0 && removed_item != NULL){
                
                write(pipefd[i][1], removed_item, strlen(removed_item) + 1);
                break;
            }
        }
        }
        
        while(f->Others_Services_Queue.tamanho > 0 && f->Video_Streaming_Queue.tamanho == 0){
            char* removed_item = retira_fila(&f->Others_Services_Queue);
            for(int i =0; i < config.AUTH_SERVERS; i++){
                printf("%d\n", auth_engine_status[i]);
                if(auth_engine_status[i] == 0 && removed_item != NULL){
                write(pipefd[i][1], removed_item, strlen(removed_item) + 1);
                break;
            }
        }

        }
        
        pthread_mutex_unlock(&f->mutexFila);
        
    }


    
    
    return NULL;
}


int find_user(MobileUser * shared_users, int id, int num_users){
    for(int i = 0; i < num_users; i++){
        if(shared_users[i].ID == id){
            return i;
        }
    }
    return -1;
}


void initialize(){
    
    #ifdef DEBUG
        DEBUG_PRINT("INITIALIZING!\n");
    #endif

    unlink(USER_PIPE);
    unlink(BACK_PIPE);
    sem_unlink("LOG");
    sem_unlink("SHM");
    sem_unlink("MONITOR");
    sem_unlink("MSQ");
    sem_unlink("AUTH_ENG");

    SHM = sem_open("SHM", O_CREAT|O_EXCL, 0700,1);
    #ifdef DEBUG
        DEBUG_PRINT("SEMPAHORE TO SHARED MEMORY CREATED!\n");
    #endif

    LOG = sem_open("LOG", O_CREAT|O_EXCL, 0700,1);
    #ifdef DEBUG
        DEBUG_PRINT("SEMAPHORE TO LOG CREATED!\n");
    #endif
    

    MONITOR = sem_open("MONITOR", O_CREAT|O_EXCL, 0700,1);
    #ifdef DEBUG
        DEBUG_PRINT("SEMAPHORE TO MONITOR ENGINE CREATED!\n");
    #endif

    
    AUTH_ENG = sem_open("AUTH_ENG", O_CREAT|O_EXCL, 0700,1);
    #ifdef DEBUG
        DEBUG_PRINT("SEMAPHORE TO AUTHORIZATION ENGINE CREATED!\n");
    #endif

}


void sigint_handler(int sig) {
    
        kill(pid, SIGTERM);
        kill(pid2, SIGTERM);

        waitpid(pid, NULL, 0);
        waitpid(pid2, NULL, 0);
        
        // Delete the named pipe USER_PIPE
        if (unlink(USER_PIPE) == -1 && errno != ENOENT) {
            perror("unlink USER_PIPE");
        } 
        #ifdef DEBUG
            DEBUG_PRINT("USER PIPE UNLINKED!\n");
        #endif

        // Delete the named pipe BACK_PIPE
        if (unlink(BACK_PIPE) == -1 && errno != ENOENT) {
            perror("unlink BACK_PIPE");
        } 
        #ifdef DEBUG
            DEBUG_PRINT("BACK PIPE UNLINKED!\n");
        #endif

        //desalocar memoria partilhada
       if (shmdt(shared_memory) == -1) {
        perror("shmdt failed");
        return ;
        }
        #ifdef DEBUG
            DEBUG_PRINT("SHARED MEMORY DEALOCATED!\n");
        #endif

        if (shmctl(shmid, IPC_RMID, NULL) == -1) {
            perror("shmctl failed");
            return ;
        }
        #ifdef DEBUG
            DEBUG_PRINT("SHARED MEMORY SEGMENT MARKED FOR DELETION!\n");
        #endif

        
        

        // Unlink semaphores
        if (sem_unlink("LOG") == -1 && errno != ENOENT) {
            perror("sem_unlink LOG");
        } 
        #ifdef DEBUG
            DEBUG_PRINT("LOG SEMAPHORE UNLINKED!\n");
        #endif

        if (sem_unlink("MONITOR") == -1 && errno != ENOENT) {
            perror("sem_unlink MONITOR");
        } 
        #ifdef DEBUG
            DEBUG_PRINT("MONITOR SEMAPHORE UNLINKED!\n");
        #endif

        if (sem_unlink("MSQ") == -1 && errno != ENOENT) {
            perror("sem_unlink MSQ");
        } 
        #ifdef DEBUG
            DEBUG_PRINT("MESSAGE QUEUE SEMAPHORE UNLINKED!\n");
        #endif

        if (sem_unlink("AUTH_ENG") == -1 && errno != ENOENT) {
            perror("sem_unlink AUTH_ENG");
        } 
        #ifdef DEBUG
            DEBUG_PRINT("AUTHORIZATION ENGINE SEMAPHORE UNLINKED!\n");
        #endif

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
    
    initialize();

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
    #ifdef DEBUG
            DEBUG_PRINT("CONFIG OPENED!\n");
    #endif

    Config config = {0,0,0,0,0,0};


    if(fscanf(file, "%d %d %d %d %d %d", &config.MOBILE_USER, &config.QUEUE_POS, &config.AUTH_SERVERS, &config.AUTH_PROC_TIME, &config.MAX_VIDEO_WAIT, &config.MAX_OTHERS_WAIT) != 6) {
       log_msg("Erro ao ler o ficheiro de configuração");
        return 1;
    }

    fclose(file);


    int num_mobile_users = config.MOBILE_USER;
    int num_auth_servers = config.AUTH_SERVERS;

    int total_size = sizeof(int) * (num_auth_servers + 1) + sizeof(MobileUser) * num_mobile_users + sizeof(GeneralStats);

    int shmid = shmget(IPC_PRIVATE, total_size, IPC_CREAT | 0700);
    if(shmid == -1){
        log_msg("Erro ao criar a memória partilhada");
        return 1;
    }

    shared_memory = shmat(shmid, NULL, 0);
    if(shared_memory == (void*)-1){
        log_msg("Erro ao associar a memória partilhada");
        return 1;
    }



    int *auth_engine_status = (int*)shared_memory;
    int* next_user_index = auth_engine_status + num_auth_servers;
    MobileUser *shared_users = (MobileUser*)(next_user_index + 1);
    GeneralStats *general_stats = (GeneralStats*)(shared_users + num_mobile_users);

    *next_user_index = 0;
    
    pid = fork();
    if(pid  < 0){
        log_msg("Erro ao criar processo Auth Request Manager");
        return 1;  
    }else if(pid == 0){ // Processo  Auth Request Manager
        log_msg("Processo Auth Request Manager criado");
        if (mkfifo(USER_PIPE, O_CREAT | O_EXCL | 0600 ) < 0){
            log_msg("Erro ao criar o named user_pipe");
            return 1;
        }
        #ifdef DEBUG
            DEBUG_PRINT("USER PIPE WAS CREATED!\n");
        #endif
        
        char buf[MAX_BUF];
        int fd = open(USER_PIPE, O_RDWR);
        if(fd < 0){
            log_msg("Erro ao abrir o named user_pipe");
            return 1;
        }
        #ifdef DEBUG
            DEBUG_PRINT("USER PIPE WAS OPENED!\n");
        #endif   
        if (mkfifo(BACK_PIPE, O_CREAT | O_EXCL | 0600 ) < 0){
            perror("Error creating back_pipe");
            return 1;
        }

        #ifdef DEBUG
            DEBUG_PRINT("BACK PIPE WAS CREATED!\n");
        #endif

    int fd_backoffice = open(BACK_PIPE, O_RDWR);
    if(fd_backoffice < 0){
        perror("Error opening back_pipe");
        return 1;
    }

    #ifdef DEBUG
            DEBUG_PRINT("BACK PIPE WAS OPENED!\n");
        #endif
    fflush(stdout);

    
    pthread_t thr_receiver, thr_sender;


    filas f;

    pthread_mutex_init(&f.mutexFila, NULL);
    f.QUEUE_POS = config.QUEUE_POS;
    cria_fila(&f.Video_Streaming_Queue);
    cria_fila(&f.Others_Services_Queue);

    int pipefd[config.AUTH_SERVERS][2];
    thread_args args;
    args.f = &f;
    args.fd = fd;
    args.fd_backoffice = fd_backoffice;
    args.pipefd = pipefd;
    args.auth_engine_status = auth_engine_status;
    args.config = config;
    
    pthread_create(&thr_receiver, NULL, receiver_thread, &args);
    pthread_create(&thr_sender, NULL, sender_thread, &args);
    
    for(int i = 0; i < config.AUTH_SERVERS; i++){ // Criar Auth Engines
        
        if(pipe(pipefd[i]) == -1){
            log_msg("Erro ao criar pipe");
            return 1;
        }

        pid_t pid_auth_engine = fork();
        if(pid_auth_engine < 0){
            log_msg("Erro ao criar processo Auth Engines");
            return 1;
        }else if(pid_auth_engine == 0){
            printf("Processo Auth Engines criado %d\n", i);
            close(pipefd[i][1]);
            char buf[MAX_BUF];
            while(1){
                ssize_t bytes_read = read(pipefd[i][0], buf, MAX_BUF);
                if(bytes_read > 0){
                    int id_user, value_user;
                    char type[10];

                    if(strcmp(buf, "data_stats") == 0){
                        printf("Estatísticas gerais: \n");
                        printf("Video Data: %d\n", general_stats->VideoData);
                        printf("Video Req: %d\n", general_stats->VideoReq);
                        printf("Music Data: %d\n", general_stats->MusicData);
                        printf("Music Req: %d\n", general_stats->MusicReq);
                        printf("Social Data: %d\n", general_stats->SocialData);
                        printf("Social Req: %d\n", general_stats->SocialReq);


                    }else if(strcmp(buf, "reset") == 0){
                        printf("Reset das estatísticas gerais\n");
                        general_stats->VideoData = 0;
                        general_stats->VideoReq = 0;
                        general_stats->MusicData = 0;
                        general_stats->MusicReq = 0;
                        general_stats->SocialData = 0;
                        general_stats->SocialReq = 0;

                           
                    
                    }else if(sscanf(buf, "%d#%d", &id_user, &value_user) == 2){
                        printf("Mensagem inicial: %d %d\n", id_user, value_user);
                        sem_wait(AUTH_ENG);
                        auth_engine_status[i] = 1;
                        shared_users[*next_user_index].ID = id_user;
                        shared_users[*next_user_index].Plafond_inicial = value_user;
                        shared_users[*next_user_index].Plafond_atual = value_user;
                        (*next_user_index)++;
                        auth_engine_status[i] = 0;
                        sem_post(AUTH_ENG);
                    }else if( sscanf(buf, "%d#%[^#]#%d", &id_user, type, &value_user) == 3){
                        sem_wait(AUTH_ENG);
                        auth_engine_status[i] = 1;
                        int index = find_user(shared_users, id_user, *next_user_index); 
                        
                        if(index == -1){
                        log_msg("Erro ao encontrar o utilizador"); 
                        }else{
                            if(shared_users[index].Plafond_atual <= 0){
                                //sem_wait(LOG);§
                                log_msg("Plafond insuficiente");
                                //sem_post(LOG);
                            }else{
                                shared_users[index].Plafond_atual -= value_user;
                                printf("Plafond atual: %d\n", shared_users[index].Plafond_atual);
                                printf("%s\n", type);

                                if(strcmp(type, "VIDEO")==0){
                                    general_stats->VideoReq++;
                                    general_stats->VideoData  += value_user;
                                }else if(strcmp(type, "MUSIC")==0){
                                    general_stats->MusicReq++;
                                    general_stats->MusicData += value_user;
                                } else if(strcmp(type, "SOCIAL")==0){
                                    general_stats->SocialReq ++;
                                    general_stats->SocialData += value_user;
                                }

                            }
                        }
                        sleep(config.AUTH_PROC_TIME/1000); 
                        auth_engine_status[i] = 0;
                        sem_post(AUTH_ENG);
                    }
                }
                
            }
            return 0;
        }else{
            close(pipefd[i][0]);
        }
    }




    



    pthread_join(thr_receiver, NULL);
    pthread_join(thr_sender, NULL);


    return 0;
    }



    pid2 = fork();
    if(pid2 < 0){
        log_msg("Erro ao criar processo Monitor Engine");
        return 1;
    }else if(pid2 == 0){ // Processo filho, referente ao Monitor Engine
        char* buf[MAX_BUF];
        log_msg("Processo Monitor Engine criado");
        return 0;
    }

    log_msg("Processo System Manager criado");

    waitpid(pid, NULL, 0);
    waitpid(pid2, NULL, 0);

    //desalocar memoria partilhada



    log_msg("Fim do programa");
    return 0;
    }

