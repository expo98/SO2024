#include <stdio.h>
#include <stdlib.h>
#include <string.h>



typedef struct no {
    char *s;
    struct no* prox;
} no;

typedef struct fila{
    struct no* inicio;
    struct no* fim; 
    int tamanho;
}fila;

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
        char *res = malloc(sizeof(char) * (strlen(f->inicio->s) + 1));  // Allocate memory for the new string
        strcpy(res, f->inicio->s);  // Copy the string
        printf("Removing from queue: %s\n", res);  // Print the element being removed
        no* temp = f->inicio;
        f->inicio = f->inicio->prox;
        free(temp);
        f->tamanho--;
        return res;
    }else {
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

int main(){
    fila f;
    cria_fila(&f);
    insere_fila(&f, "A", 26);
    insere_fila(&f, "B", 26);
    insere_fila(&f, "C", 26);
    insere_fila(&f, "D", 26);
    insere_fila(&f, "E", 26);
    insere_fila(&f, "F", 26);
    insere_fila(&f, "G", 26);
    insere_fila(&f, "H", 26);
    insere_fila(&f, "I", 26);
    
    imprime_fila(&f);
    printf("%d\n", f.tamanho);
    retira_fila(&f);
    retira_fila(&f);
    retira_fila(&f);
    retira_fila(&f);
    imprime_fila(&f);
    printf("%d\n", f.tamanho);
}

