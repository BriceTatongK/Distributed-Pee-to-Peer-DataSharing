#include <arpa/inet.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <time.h>
#include <signal.h>
#include <string.h>

// #costanti#
#define BUFFER_SIZE     1024
#define BUF_LEN         1024
#define REQUEST_LEN     4  // REQ\0
#define MAX_PEER        20 // scelta progettuale !
#define polling         2
#define RES_LEN         100
#define STDIN           0      // imposto il valore di STDIN a "0",  



// #struct#
// semplice struct per dati di una data
struct data{
    int a;  //anno
    int m;  //mese
    int g;  //giorno
};


// ## strut dati peer lato DS
struct peer{
    int porta;
    struct sockaddr_in peer_addr;
    int v1, v2, v3;
    struct peer *next;
};


// #FUNZIONI#
// modifica la data di +/-(days) giorni
void DatePlusDays( struct tm* date, int days )
{
    // numero secondi di un giorno
    const time_t ONE_DAY = 24 * 60 * 60 ;

    // secondi since start of epoch e (+/-) quelli di un giorno
    time_t date_seconds = mktime( date ) + (days * ONE_DAY) ;

    // aggiorna la data "date"
    // converto i secondi ottenuti in local time
    *date = *localtime( &date_seconds ) ; ;
}


// crea nomi dei file
void make_file_name(char oggi[], char domani[], int porta){
    time_t rawtime;
    struct tm* timeinfo;
    //char _oggi[20];
    //char _domani[20];
    time(&rawtime);
    timeinfo = localtime(&rawtime);
    sprintf(oggi, "%d:%02d:%d_%d", timeinfo->tm_year+1900, timeinfo->tm_mon+1, timeinfo->tm_mday, porta);
    //strcpy(oggi, _oggi);
    DatePlusDays(timeinfo, +1);
    sprintf(domani, "%d:%02d:%d_%d", timeinfo->tm_year+1900, timeinfo->tm_mon+1, timeinfo->tm_mday, porta);
    //strcpy(domani, _domani);
}


// check l'ora
// 1: chiuso, 0: non chiuso
int registro_chiuso(){
    struct tm *timeinfo;
    time_t rawtime;
    time(&rawtime);
    timeinfo = localtime(&rawtime);
    if(timeinfo->tm_hour >= 18)
        return 1;
    else
        return 0;
}

// print puntini
void riga(){
    printf("'''''''''''''''''''''''''''''''''''''\n");
}


// build l'indirizzi dei vicini
void make_addr(struct sockaddr_in vet[], int v[]){
    int i;
    for(i=0; i<3; i++){
        if(v[i] != -1){
            memset(&vet[i], 0, sizeof(vet[i])); //Pulizia
            vet[i].sin_family = AF_INET;
            inet_pton(AF_INET, "127.0.0.1", &vet[i].sin_addr);
            vet[i].sin_port = htons(v[i]);
        }
    }
}

// copy char date[] ---> struct data;
struct data stoi(char date[]){
    struct data t;
    sscanf(date, "%d:%d:%d", &t.a, &t.m, &t.g);
    return t;
}

// copy struct date ----> char date[]
void itos(char vett[], struct data date){
    //char t[20];
    sprintf(vett, "%d:%02d:%d", date.a, date.m, date.g);
    //char *p = t;
    return;
}


// comfronto di due date !
// 1:d1>d2 2:d2>d1 0:d1=d2
int date_cmp(struct data* d1, struct data* d2){
    if(d1->a > d2->a)
        return 1;
    else if(d1->a < d2->a)
        return 2;
    else
    {
        if(d1->m > d2->m)
            return 1;
        else if(d1->m < d2->m)
            return 2;
        else
        {
            if(d1->g > d2->g)
                return 1;
            else if(d1->g < d2->g)
                return 2;
            else
                return 0;
        }
    }
}


// controllo del periodo del commando GET
int period_check(char d1[], char d2[], char d_avvio[])
{
    time_t rawtime;
    struct tm* timeinfo;
    struct data t1, t2, t3, t4;
    
    if((!d1 && d2) || (d1 && !d2))
        return 0;
    if(!strcmp(d2, "*") && !strcmp(d1, "*")){
        printf("due stelle *-* ???\n");
        return 0;
    }
    if(strlen(d1) >= 8 && strlen(d2) >= 8){
        t1 = stoi(d1);
        t2 = stoi(d2);
        if(date_cmp(&t1, &t2) == 1){
            printf("d1 == d2 ???\n");
            return 0;
        }
    }
    time(&rawtime);
    timeinfo = localtime(&rawtime);
    t3.a = timeinfo->tm_year+1900;
    t3.m = timeinfo->tm_mon+01;
    t3.g = timeinfo->tm_mday;

    // se d2 è maggire della data del giorno
    if(date_cmp(&t2, &t3) == 1){
        printf("d2 è maggire della data del giorno ??\n");
        return 0;
    }

    //se d1 è minore della data di avvio del sistema
    t4 = stoi(d_avvio);
    if(strcmp(d1, "*")!=0  &&   date_cmp(&t4, &t1) == 1){
        printf("d1 è minore della data del primo avvio ???\n");
        return 0;
    }
    
    return 1;
}



// out ...
void out(){
    int i,j;
    for(i=0; i<3; i++){
        printf("..");
    }
    printf("\n");
}


// menu server
void menu(){        
    printf("Digita un comando:..\n");
    printf("1) start <DS_addr> <DS_port> -->\n2) add <type> <quantity> -->\n3) get <aggr> <type> <period> -->\n4) stop --> ferma il peer\n");
    return;
}


// max tra due socket
int _max(int x, int y)
{ 
    if (x > y) 
        return x; 
    else
        return y; 
}


// menu DS
void menu_ds(){
    printf("Digita un comando:..\n");
    printf("1) Help --> mostra i dettagli dei comandi,\n2) status --> mostra un elenco dei peer connessi,\n3) showneighbor <peer> --> mostra i neighbor di un peer,\n4) esc --> chiude il DS\n");
    printf("\n");
    return;
}


// prossima posizione free nell'array
int next_free(int vet[]){
    int i;
    for(i=0; i<MAX_PEER; i++){
        if(vet[i] == -1)
            break;
    }
    return i;
}


// cancella peer dalla lista
void cancella_peer(struct peer **lista, int t){
    struct peer *t1 = *lista;
    struct peer *t2 = 0;
    if(t1 != 0 && t1->porta == t){
        *lista = t1->next;
        free(t1);
        return;
    }

    while (t1 != 0 && t1->porta != t){
        t2 = t1;
        t1 = t1->next;
    }
    if(t1 == 0)
        return;
    
    t2->next = t1->next;
    free(t1);
    return;
}

// inserisci in testa un peer
void inserisci_peer(struct peer **lista, struct peer* t){
    t->next = *lista;
    *lista = t;
    return;
}

// azzarra campi v di struct peer
void azzera_vicini(struct peer *p){
    p->v1 = -1; 
    p->v2 = -1;
    p->v3 = -1;
    return;
}

// aggiorno i vicini dopo che un peer se disconnect
void aggiorna_vicini_peer(struct peer *p, int porta){
    while(p!=0){
        if(p->v1 == porta){
            p->v1 = -1;
        }else if(p->v2 == porta)
            p->v2 = -1;
        else if(p->v3 == porta)
            p->v3 = -1;
        p=p->next;
    }
}


// trovo la posizione di porta, poi faccio scorrere che stanno davanti
// occupare il vecchio posto di porta.
void aggiorna_array_porte(int vet[], int dim, int porta){
    int i,j;
    for(i=0; i<dim; i++){
        if(vet[i] == porta){j=i;break;}
    }
    dim = dim -1;
    for(; j < dim; j++){vet[j] = vet[j+1];}
    vet[dim] = -1;

    for(i=0; i<dim; i++){
        printf("%d, ", vet[i]);
    }
    printf("\n");
}


// verifica se peer "b" nell'array
int check_peer(int vet[], int dim, int b){
    int i;
    for(i=0; i<dim; i++){
        if(vet[i] == b)
            return 1;
    }
    return 0;
}

// mette tutti elementi dell'array a -1
void azzera_array(int vet[]){
    int i;
    for(i=0; i<MAX_PEER; i++)
        vet[i]=-1;
}