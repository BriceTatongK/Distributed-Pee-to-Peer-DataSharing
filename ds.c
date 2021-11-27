#include <arpa/inet.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <time.h>
#include "header.h"


int main(int argc, char* argv[])
{

    int  newfd, tcp_s, addrlen, i, j, len, udp_s, quanti_peer = 0;
    // per file e tempo
    FILE *fd;
    time_t rawtime;
    struct tm* timeinfo;
    ssize_t ret;

    char *cmd = 0, prima_data[15];
    //buffer invio e ricezione
    char buffer_in[BUFFER_SIZE];
    char buffer_go[BUFFER_SIZE];
    int array_porte[MAX_PEER], buffer_len;
    azzera_array(array_porte);
    int array_porte_next = next_free(array_porte);  // mi dice anche quanti peer ci sono
    struct peer *lista_peer = 0, *p = 0, *q = 0;

    fd_set master;
    fd_set read_fds;
    int fdmax;

    //struct socketlen_t *len;
    struct sockaddr_in my_addr, cl_addr, connect;
    char buffer[BUF_LEN];

    // Creazione socket tcp
    tcp_s = socket(AF_INET, SOCK_STREAM, 0);

    // Creazione indirizzo di bind
    memset(&my_addr, 0, sizeof(my_addr)); // Pulizia 
    my_addr.sin_family = AF_INET;
    my_addr.sin_port = htons((uint16_t)atoi(argv[1]));
    my_addr.sin_addr.s_addr = INADDR_ANY;
    ret = bind(tcp_s, (struct sockaddr*)&my_addr, sizeof(my_addr) );
    if( ret < 0 ){
        perror("Bind non riuscita tcp_s\n");
        exit(0);
    }
    listen(tcp_s, 10);

    //create udp socket e bind
    udp_s = socket(AF_INET,SOCK_DGRAM|SOCK_NONBLOCK,0);
    ret = bind(udp_s, (struct sockaddr*)&my_addr, sizeof(my_addr));
    if( ret < 0 ){
        perror("Bind non riuscita udp_s\n");
        exit(0);
    }

    // Reset FDs
    FD_ZERO(&master);
    FD_ZERO(&read_fds);

    // Add sockets to the set
    FD_SET(tcp_s, &master);
    FD_SET(STDIN, &master);
    FD_SET(udp_s, &master);

    fdmax = _max(tcp_s, udp_s);
    
    //# MENU
    printf("**|DS-STARTED|**\n");
    menu_ds();

    while(1)
    {
        // Reset the set
        read_fds = master;
        
        // Mi blocco in attesa di descrottori pronti
        select(fdmax + 1, &read_fds, NULL, NULL, NULL);
        for(j = 0; j <= fdmax; j++)
        {
            // se fa parte del set pronti selected by "select"
            if(FD_ISSET(j, &read_fds))
            {
                
                //#################################
                //# CASO  SOCKET TCP PER ASCOLTO ##
                //#################################
                if(j == tcp_s)
                {
                    // Ho appena ricevuto una richiesta di connessione
                    addrlen = sizeof(cl_addr);
                    newfd = accept(tcp_s,
                        (struct sockaddr *)&cl_addr, (socklen_t*)&addrlen);

                    // Aggiungo la richiesta al set
                    FD_SET(newfd, &master); 
                        
                    // Aggiorno l'ID del massimo descrittore
                    if(newfd > fdmax){ fdmax = newfd;}

                }



                //####################
                //# CASO SOCKET UDP ##
                //####################
                else if (j == udp_s)
                {
                    do
                    {
                        // Tento di ricevere i dati dal server
                        len = sizeof(connect);
                        ret = recvfrom(udp_s, buffer_in, RES_LEN, 0,
                            (struct sockaddr*)&connect, (socklen_t*)&len);
                        //Se non ricevo niente vado a dormire per un poco
                        if(ret < 0)
                            sleep(polling);
                    }while(ret < 0);
                    char a[20];
                    int b;


                    ///////////////////////////////////////////
                    // gestione "START" <ds_addr> <ds_port> ///
                    ///////////////////////////////////////////
                    if(strncmp(buffer_in, "start", 5) == 0)
                    {
                        sscanf(buffer_in, "%s %d",a, &b);
                        //if(check_peer(array_porte, quanti_peer, b) == 0){
                        quanti_peer ++;
                        p = malloc(sizeof(struct peer));
                        p->peer_addr = connect;
                        p->porta = b;
                        p->next = 0;
                        azzera_vicini(p);

                        //inserisco in coda
                        p->next = lista_peer;
                        lista_peer = p;
                        array_porte[array_porte_next] = b;

                        // il DS deve memorizzare dentro il file "DS_stuff", le info di connessione dei peer
                        // creo la entry del file ds_stuff **data porta**      
                        if(quanti_peer == 1)
                        {
                            // registro la data di inizio della rete  e la salvo nel "ds_stuff"
                            char temp[20];
                            char data[20];
                            struct data t;
                            time(&rawtime);
                            timeinfo = localtime(&rawtime);
                            t.a=timeinfo->tm_year+1900;
                            t.m=timeinfo->tm_mon+01;
                            t.g=timeinfo->tm_mday;
                            itos(data, t);
                            sprintf(temp, "%s %s", "avvio", data);
                            fd = fopen("DS_stuff", "a");
                            if(fd == NULL)
                                printf("error can't open DS_stuff");
                            fprintf(fd, "%s\n", temp);
                            fclose(fd);

                            // caso primo peer -> no vicini
                            cmd = "no vicini";
                            do
                            {
                                // Tento di inviare la richiesta continuamente
                                ret = sendto(udp_s, cmd, sizeof(cmd), 0,
                                    (struct sockaddr*)&connect, sizeof(connect));

                                // Se la richiesta non e' stata inviata vado
                                if(ret < 0)
                                    sleep(polling);
                            }while(ret < 0);
                        }

                        // inizio a distribuire i vicini ai peer appena sono almeno 4
                        else if(quanti_peer >= 4)
                        {
                            // scorro la lista dei peer, per ogni peer: mando i vicini
                            int i;
                            q = lista_peer;
                            while(q!=0)
                            {
                                for(i=0; i<quanti_peer; i++)
                                {
                                    if(q->porta == array_porte[i])
                                        break;
                                }
                                q->v1 = array_porte[(i+1)%quanti_peer];
                                q->v2 = array_porte[(i+2)%quanti_peer];
                                q->v3 = array_porte[(i+3)%quanti_peer];
                                sprintf(buffer_go, "%s %d %d %d", "VICINI", q->v1, q->v2, q->v3);
                                buffer_len = strlen(buffer_go) +1;
                                do
                                {
                                    // Tento di inviare la richiesta continuamente
                                    ret = sendto(udp_s, buffer_go, buffer_len, 0,
                                        (struct sockaddr*)&q->peer_addr, sizeof(q->peer_addr));
                                            
                                    // Se la richiesta non e' stata inviata sleep
                                    if(ret < 0)
                                        sleep(polling);
                                } while(ret < 0);

                                // incremento il puntatore che scorre la lista
                                q = q->next;
                            }
                        }

                        // registro nel file DS_stuff la prima data online di un peer !
                        // mi serve dopo quando devo effettuare delle elaborazioni
                        // prima data online di peer porta:b
                        char temp[20];
                        char data[20];
                        struct data t;
                        time(&rawtime);
                        timeinfo = localtime(&rawtime);
                        t.a=timeinfo->tm_year+1900;
                        t.m=timeinfo->tm_mon+01;
                        t.g=timeinfo->tm_mday;
                        itos(data, t);
                        sprintf(temp, "%d %s", b, data);
                        fd = fopen("DS_stuff", "a");
                        if(fd == NULL)
                            printf("error can't open DS_stuff");
                        fprintf(fd, "%s\n", temp);
                        fclose(fd);

                        // next free in array_porte
                        // mi sposto nel vettore delle porte sei peer
                        array_porte_next = next_free(array_porte);

                    }


                    ///////////////////////////////////
                    // gestione "STOP <porta_peer>" ///
                    ///////////////////////////////////
                    if(strncmp(buffer_in, "STOP", 4) == 0)
                    {
                        printf("STOP: richiesto da un peer..\n");
                        cmd = "STOP OK";
                        do
                        {
                            ret = sendto(udp_s, cmd, sizeof(cmd),0,
                                (struct sockaddr*)&connect, sizeof(connect));
                            if(ret<0)
                                sleep(polling);
                        }while (ret<0);

                        printf("stop request grant..\n");
                        sscanf(buffer_in, "%s %d", a, &b);
                        // rimuove il peer dalla lista
                        cancella_peer(&lista_peer, b);
                        // aggiorna i campi vicini di ogni peer
                        aggiorna_vicini_peer(lista_peer, b);
                        // aggiorna l'array dei peer togliendo la porta del peer che si disconnect
                        aggiorna_array_porte(array_porte, quanti_peer, b);
                        // next posizione free
                        array_porte_next = next_free(array_porte);

                        // aggiorno il num di peer
                        quanti_peer--;

                        // aggiorno i vicini dei peer
                        if(quanti_peer >= 4)
                        {
                            q = lista_peer;
                            while(q!=0)
                            {
                                for(i=0; i<quanti_peer; i++)
                                {
                                    if(q->porta == array_porte[i])
                                        break;
                                }
                                q->v1 = array_porte[(i+1)%quanti_peer];
                                q->v2 = array_porte[(i+2)%quanti_peer];
                                q->v3 = array_porte[(i+3)%quanti_peer];

                                sprintf(buffer_go, "%s %d %d %d", "VICINI", q->v1, q->v2, q->v3);
                                buffer_len = strlen(buffer_go) +1;
                                do
                                {
                                    // Tento di inviare la richiesta continuamente
                                    ret = sendto(udp_s, buffer_go, buffer_len, 0,
                                        (struct sockaddr*)&q->peer_addr, sizeof(q->peer_addr));
                                    // Se la richiesta non e' stata inviata vado
                                    if(ret < 0)
                                        sleep(polling);
                                }while(ret < 0);
                                q = q->next;
                            }
                        }
                    }


                    /////////////////////////////////
                    /// rechiesta date di avvio    //
                    /////////////////////////////////
                    else if(strncmp(buffer_in, "AVVIO", 5) == 0)
                    {
                        // legge nel file, fa la stinga con data avvio e data online e manda al richiedente
                        int intero;
                        char cmd[6];

                        sscanf(buffer_in, "%s %d", cmd, &intero);
                        char * line = NULL;
                        size_t len = 0;
                        ssize_t read;
                        char data_avvio[12];
                        char data_online[12];
                        int intr;

                        fd = fopen("DS_stuff", "r");
                        if (fd == NULL)
                            printf("error open file\n");

                        while ((read = getline(&line, &len, fd)) != -1)
                        {
                            if(strncmp(line, "avvio", 5) == 0)
                            {
                                sscanf(line, "%s %s", cmd, data_avvio);
                            }
                            else
                            {
                                sscanf(line, "%d %s", &intr, data_online);
                                if(intr == intero)
                                {
                                    break;
                                }
                            }
                        }
                        sprintf(buffer_go, "%s %s", data_avvio, data_online);
                        fclose(fd);

                        buffer_len = strlen(buffer_go)+1;
                        do
                        {
                            ret = sendto(udp_s, buffer_go, buffer_len, 0,
                                (struct sockaddr*)&connect, sizeof(connect));
                            if(ret<0)
                                sleep(polling);
                        }while(ret<0);
                    }
                }



                //#######################
                //# CASO SOCKET STDIN ###
                //#######################
                else if(j == STDIN)
                {
                    //leggo dalla linea di commando
                    fgets(buffer, 30, stdin);

                    //////////////////
                    // comand help ///
                    //////////////////
                    if(strncmp(buffer, "help", 4) == 0)
                    {
                        menu_ds();
                    }




                    ///////////////////
                    // comand status //
                    ///////////////////
                    else if(strncmp(buffer, "status", 5) == 0)
                    {
                        printf("...........exe status\n");
                        printf("connected peers:\n");
                        for(i=0; i<quanti_peer; i++)
                            printf(",%d",array_porte[i]);
                        printf("\n");

                        printf("...........end exe status\n");
                    }



                    //////////////////
                    // showneighbor //
                    //////////////////
                    else if(strncmp(buffer, "showneighbor", 12) == 0)
                    {
                        printf("........... exe showneighbor\n");
                        char stringa[20];
                        int intero = -1;
                        sscanf(buffer, "%s %d",stringa, &intero);
                        if (intero == -1)
                        {
                            q = lista_peer;
                            while(q!=0){
                                printf("%d->%d,%d,%d\n", q->porta, q->v1, q->v2, q->v3);
                                q = q->next;
                            }
                        }
                        else
                        {
                            q = lista_peer;
                            while(q!=0)
                            {
                                if(q->porta == intero){
                                    printf("%d->%d,%d,%d\n", q->porta, q->v1, q->v2, q->v3);
                                    break;
                                }
                                q = q->next;
                            }
                        }

                        printf("...........end exe showneighbor\n");
                    }



                    ////////////
                    // ESC /////
                    ////////////
                    else if(strncmp(buffer, "esc", 3) == 0)
                    {
                        printf(".........exe esc\n");
                        printf("send term to peer...\n");
                        // faccio un broadcast a tutti peer
                        cmd = "ESC";
                        q = lista_peer;
                        while (q !=0)
                        {
                            do
                            {
                                ret = sendto(udp_s, cmd, sizeof(cmd), 0,
                                    (struct sockaddr*)&q->peer_addr, sizeof(q->peer_addr));
                                if(ret <0)
                                    sleep(polling);
                            } while (ret <0);
                                
                            q = q->next;
                        }
                        printf("closing sockets..\n");
                        close(tcp_s);
                        close(udp_s);
                        printf("disconnecting ..\n");
                        exit(1);
                        // comand non valide
                    }else{
                        printf("LOG: comando non valido !!\n");
                    }
                }

                //##############################################
                //# CASO SOCKET TCP PRONTO PER COMMUNICAZIONE ##
                //##############################################
                else 
                {
                    // Chiudo il socket e lo rimuovo dal set
                    close(i);
                    FD_CLR(i, &master);
                }
            }

        }
    }
    close(tcp_s);
}
