/* Autores do projeto
        -Carlos Pereira 2022232042
        -Miguel Pereira 2022232552
*/

#include "functions.h"
#define MAX_BUF 1024
#define USER_PIPE "/tmp/user_pipe"

int max_pedidos_aut;
pid_t ID_mobile_user;
int dados_reservar;
int user_pipe_fd;
pthread_mutex_t lock;

char mensagem_inicial[100];
char mensagem_autorizacao_VIDEO[100];
char mensagem_autorizacao_MUSIC[100];
char mensagem_autorizacao_SOCIAL[100];

typedef struct 
{
    char* type;
    int interval;
}threadData;

void* send_auth_message(void* arg) {
    threadData* data = (threadData*)arg;
    char buf[BUFSIZ];
    while(1) {
        pthread_mutex_lock(&lock);
        if(max_pedidos_aut > 0) {
            sprintf(buf,"%d#%s#%d \n", ID_mobile_user, data->type, dados_reservar);
            // Write the message to the named pipe
            ssize_t bytes_written = write(user_pipe_fd, buf, strlen(buf));
            if (bytes_written == -1) {
                perror("write");
                pthread_mutex_unlock(&lock);
                return NULL;
            }
            max_pedidos_aut--;
            pthread_mutex_unlock(&lock);
            sleep(data->interval);
            if(max_pedidos_aut <= 0) break;
        } else {
            pthread_mutex_unlock(&lock);
            break;
        }
    }
    return NULL;
}

//ssize_t bytes_written = write(user_pipe_fd, mensagem, strlen(mensagem));

int main(int argc, char *argv[]) {
    // Verificar se os argumentos corretos foram passados
    pthread_mutex_init(&lock, NULL);
    if(argc != 7) {
        printf("Syntax Inválida: ./mobile_user  {plafond inicial} {número máximo de pedidos de autorização} {intervalo VIDEO} {intervalo MUSIC} {intervalo SOCIAL} {dados a reservar}\n");
        return 1;
    }

    ID_mobile_user = getpid();
    int plafond_inicial = atoi(argv[1]);
    max_pedidos_aut = atoi(argv[2]);
    float inter_VIDEO = atoi(argv[3])  ;
    float inter_MUSIC = atoi(argv[4]) ;
    float inter_SOCIAL  = atoi(argv[5]) ;
    dados_reservar = atoi(argv[6]);

    user_pipe_fd = open(USER_PIPE, O_WRONLY);
    if (user_pipe_fd == -1) {
        perror("open");
        return 1;
    }



    sprintf(mensagem_inicial, "%d#%d", ID_mobile_user, plafond_inicial);
    printf("Mensagem inicial: %s\n", mensagem_inicial);

    // Cria as instancias struct para guardar os argmentos necessários para cada thread
    threadData authArgs[3];
    authArgs[0].type = "VIDEO";
    authArgs[0].interval = inter_VIDEO;
    authArgs[1].type = "MUSIC";
    authArgs[1].interval = inter_MUSIC;
    authArgs[2].type = "SOCIAL";
    authArgs[2].interval = inter_SOCIAL;
    
     printf("Max pedidos autorização: %d\n", max_pedidos_aut);

    pthread_t threads[3];
    for(int i = 0; i < 3; i++) {
        int result = pthread_create(&threads[i], NULL, send_auth_message, &authArgs[i]);
        if (result != 0) {
            printf("Failed to create thread %d\n", i);
        }
        sleep(1); // Add this line
    }
       
        for(int i = 0; i < 3; i++) {
        pthread_join(threads[i], NULL);
    }

    return 0;
}