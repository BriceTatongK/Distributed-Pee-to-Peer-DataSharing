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

    int ret, newfd, tcp_s, addrlen, i, len, udp_s;
    int array_vicini[3];    // array per porte dei vicini
    azzera_array(array_vicini); // mette a -1 i campi del vettore
    int connected = 0;  //per sapere se il peer è connesso o meno
    char a[20];
    int  v1, v2, v3, buffer_len;
    // buffer per invio e recezione dati, e per tenere le date di primo avvio dell'app, e di connessione del peer
    char buffer_in[BUF_LEN], buffer_go[BUF_LEN], data_avvio[12], data_online[12], buffer[BUF_LEN];

    // per la gestione dei sockets
    fd_set master;
    fd_set read_fds;
    int fdmax;

    // puntatori e nomi file
    FILE *file_giorno;
    FILE *file_next;

    // variabili per il tempo
    time_t rawtime;
    struct tm* timeinfo;
    
    // per nomi de file del giorno e next
    char nome_file_next[20];
    char nome_file_giorno[20];

    // crea i nomi dei due file con la porta dietro la quale ascolta
    make_file_name(nome_file_giorno, nome_file_next, atoi(argv[1]));

    // per tener indirizzi
    struct sockaddr_in my_addr, cl_addr, connect_addr, srv_addr, array_addr[3];

    //#Creazione socket
    tcp_s = socket(AF_INET, SOCK_STREAM, 0);

    //Creazione indirizzo di bind
    memset(&my_addr, 0, sizeof(my_addr)); 
    my_addr.sin_family = AF_INET;
    my_addr.sin_port = htons((uint16_t)atoi(argv[1]));
    my_addr.sin_addr.s_addr = INADDR_ANY;
    ret = bind(tcp_s, (struct sockaddr*)&my_addr, sizeof(my_addr) );
    if( ret < 0 )
    {
        perror("Bind non riuscita tcp_s\n");
        exit(0);
    }
    listen(tcp_s, 10);

    //create udp_s socket
    udp_s = socket(AF_INET,SOCK_DGRAM|SOCK_NONBLOCK,0);
    ret = bind(udp_s, (struct sockaddr*)&my_addr, sizeof(my_addr));
    if( ret < 0 )
    {
        perror("Bind non riuscita udp_s\n");
        exit(0);
    }

    //Creazione indirizzo del server
    memset(&srv_addr, 0, sizeof(srv_addr)); //Pulizia
    srv_addr.sin_family = AF_INET;
    inet_pton(AF_INET, "127.0.0.1", &srv_addr.sin_addr);
    srv_addr.sin_port = htons(4242);

    // Reset FDs
    FD_ZERO(&master);
    FD_ZERO(&read_fds);

    // Add tcp_s socket to the set
    FD_SET(tcp_s, &master);
    FD_SET(STDIN, &master);
    FD_SET(udp_s, &master);

    //fdmax prende il max tra tcp_s e udp_s, ormai STDIN vale 0
    fdmax = _max(tcp_s, udp_s);
    
    //# MENU
    printf("\n**|PEER_%s_STARTED|**\n", argv[1]);
    menu();

    while(1)
    {
        // Reset the set
        read_fds = master;
        
        // Mi blocco in attesa di descrottori pronti
        select(fdmax + 1, &read_fds, NULL, NULL, NULL);

        for(i = 0; i <= fdmax; i++)
        {
            if(FD_ISSET(i, &read_fds))
            {
                //################################
                //# CASO  SOCKET TCP PER ASCOLTO #
                //################################
                if(i == tcp_s)
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
                else if(i == udp_s)
                {
                    printf("_____SOCKET_UDP_ON_____\n");

                    do
                    {
                        // Tento di ricevere i dati dal server
                        len = sizeof(connect_addr);
                        ret = recvfrom(udp_s, buffer_in, 1024, 0,
                            (struct sockaddr*)&connect_addr, (socklen_t*)&len);
                        //Se non ricevo niente vado a dormire per un poco
                        if(ret < 0)
                            sleep(polling);
                    } while(ret < 0);


                    /////////////////////////
                    // gestione ESC dal ds //
                    /////////////////////////
                    if(strcmp(buffer_in, "ESC") == 0)
                    {
                        printf("ricevuto ESC dal DS..\n");
                        printf("salvo stato nel file");
                        out();

                        //salvataggio stato in un file
                        printf("closing sockets");
                        out();
                        close(tcp_s);
                        close(udp_s);
                        printf("disconnecting");
                        out();
                        exit(1);
                    }

                    /////////////////////////
                    // se ricevo i VICINI ///
                    /////////////////////////
                    else if(strncmp(buffer_in, "VICINI", 6) == 0)
                    {
                        
                        // recezione vicini 
                        printf("ricezione vicini:");out();
                        sscanf(buffer_in, "%s %d %d %d", a, &v1, &v2, &v3);
                        printf("v1:%d, v2:%d, v3:%d\n", v1, v2, v3);
                        array_vicini[0] = v1; array_vicini[1] = v2; array_vicini[2] = v3;
                        printf("connessione UDP con vicini");out();

                        // preparo gli addr di ogni vicino in array_addr[]
                        make_addr(array_addr, array_vicini);

                    }

                    // se ricevo una entry, se diverso da quello che ho mandato
                    //io la prendo guardo l'ora la salvo in file(next/giorno)
                    // eppoi la inoltro al mio primo vicino che fara uguale
                    //////////////////////////////////////////////////////////
                    /// ricevo una Entry che fa il giro dell'annello /////////
                    //////////////////////////////////////////////////////////
                    else if(strncmp(buffer_in, "Entry", 5) == 0)
                    {
                        printf("ricevuto una entry elaborazione ...\n");
                        char entry[10];
                        int porta;
                        char data[15];
                        int valore;
                        int tipo[10];
                        sscanf(buffer_in, "%s %d %s %s %d", entry, &porta, data, tipo, &valore);
                        //if(porta != atoi(argv[1]))
                        //{
                            make_file_name(nome_file_giorno, nome_file_next, atoi(argv[1]));
                            printf("la salvo in file..\n");
                            if(registro_chiuso())
                            {
                                // inserisco nel file next
                                file_next = fopen((const char*)nome_file_next, "a");
                                if(file_next != NULL)
                                {
                                    fprintf(file_next, "%s\n", buffer_in);
                                    fclose(file_next);
                                    printf("inserito nel file di next !");out();
                                }
                                else
                                    printf("error apertura file next !\n");
                            }
                            else
                            {
                                // inserisco nel file di oggi
                                file_giorno = fopen((const char*)nome_file_giorno, "a");
                                if(file_giorno != NULL)
                                {
                                    fprintf(file_giorno, "%s\n", buffer_in);
                                    fclose(file_giorno);
                                    printf("inserito nel file di oggi !");out();
                                }
                                else
                                    printf("error apertura file oggi !\n");
                            }

                            // evito di inoltrare la Entry al peer proprietario
                            if(array_vicini[0] != porta)
                            {
                                // adesso inoltro la entry al mio primo vicino
                                buffer_len = strlen(buffer_in);
                                do
                                {
                                    ret = sendto(udp_s, buffer_in, buffer_len, 0,
                                        (struct sockaddr*)&array_addr[0], sizeof(array_addr[0]));
                                    if(ret<0)
                                        sleep(polling);
                                }while(ret<0);
                                sleep(1);
                                printf("inoltrata ...\n");
                            }
                        //}

                        printf("...... end gestione Entry\n");
                    }


                    //////////////////////////////////////////////////////
                    // Aggr se ricevo un'aggr da peer che si disconnect //
                    //////////////////////////////////////////////////////
                    else if(strncmp(buffer_in, "Aggr", 4) == 0)
                    {
                        printf("ho ricevuto una Aggr da peer che termina\n");
                        //la inserisco nel file del girono corrente
                        //TOCCO1: devo prima verificare se non c'hela gia ! altrimento creo una ridondanza !!
                        FILE *fp;
                        char * line = NULL;
                        size_t len = 0;
                        ssize_t read;
                        fp = fopen((const char*)nome_file_giorno, "a+");
                        if(fp != NULL)
                        {
                            int flag = 0;
                            while ((read = getline(&line, &len, fp)) != -1)
                            {
                                printf("entry file: %s\n", line);
                                // se la Aggr esiste gia nel file non inserire
                                if(strcmp(line, buffer_in) ==0)
                                {
                                    printf("aggr già presente !\n");
                                    fclose(fp);
                                    flag = 1;
                                    break;
                                }
                            }
                            if(flag == 0)
                            {
                                fprintf(fp, "%s", buffer_in);
                                fclose(fp);
                                printf("Aggr inserita nel file del giorno\n");
                            }
                        }
                        else
                            printf("error can't  open file !\n");
                        printf("..... data from terminating peer riceived !\n");
                    }
                    

                    ///////////////////////////////////
                    // se ricevo un msg con REQ_DATA//|
                    /////////////////////////////////||
                    else if(strncmp(buffer_in, "REQ_DATA", 8) == 0)
                    {
                        //TOCCO2 : se la aggr richiesta è una full, controllo soltanto nel file di oggi !! xche ogni giorno ha un aggr full diverso !!!
                        //devo verificare la porta per saper se non l'avevo fatta io perche potrebbe ritornare a me
                        char cmd[10], agg[100], aggr[11], type[11],  period[30], comand[4];
                        char  _aggr[11], _type[11],  _period[30], _comand[4];
                        int valore, port;

                        sscanf(buffer_in, "%s %d %s %s %s %s", cmd, &port, comand, aggr, type, period);
                        printf("check buffer_in: %s\n", buffer_in);
                        printf("ricevuto REQ_DATA da %d ..........\n", port);

                        if(port != atoi(argv[1]))
                        {
                            // qui devo verificare se ho l'aggr e nel caso inviare con msg: REPLAY_DATA
                            /*
                            char d1[11];
                            char d2[11];
                            */
                            // prendo le info dal commando che mi serviranno per identificare la aggr nei file
                            //sscanf(agg, "%s %s %s %s",comand, aggr, type, period);
                            //printf("a chercher: %s\n", agg);
                            printf("preso:?? %s %s %s\n", aggr, type, period);

                            ////QUI VERIFICO SE HO IL DATO GIA CALCOLATO////
                            printf("verifico se ho gia calcolato..\n"); //*******
                            sleep(1);

                            struct data d2, d1;
                            char nome_file[20];
                            FILE *fp;
                            int res;
                            time(&rawtime);
                            timeinfo = localtime(&rawtime);
                            d2.a = timeinfo->tm_year+1900;
                            d2.m = timeinfo->tm_mon+01;
                            d2.g = timeinfo->tm_mday;
                            sscanf(data_online, "%d:%d:%d", &d1.a, &d1.m, &d1.g);
                            printf("d1: %d:%d:%d  d2: %d:%d:%d\n", d1.a, d1.m, d1.g, d2.a, d2.m, d2.g); //*******

                            // controllo i file da quando sono entrato per la prima volta fino ad oggi
                            int day = 1;
                            while(date_cmp(&d1, &d2) == 2 || date_cmp(&d1, &d2) == 0)
                            {
                                // creo il nome del file di quel giorno
                                sprintf(nome_file, "%d:%02d:%d_%s", d2.a, d2.m, d2.g, argv[1]); 
                                printf("file name? %s\n", nome_file); //**********
                                char * line = NULL;
                                size_t len = 0;
                                ssize_t read;

                                // apro in lettura
                                fp = fopen(nome_file, "r");

                                // se il file esiste 
                                if (fp != NULL)
                                {
                                    printf("il file: %s esiste\n", nome_file);  //************
                                    printf("day:%d  %d:%d:%d\n", day, d2.a, d2.m, d2.g);   //*************
                                    day ++;
                                    // leggo finche non raggiungo EOF
                                    while ((read = getline(&line, &len, fp)) != -1)
                                    {
                                        // se leggo un Aggregazione
                                        if(strncmp(line, "Aggr", 4) == 0)
                                        {
                                            printf("%s\n",line); ////*****************
                                            // verifico se corrisponde a quello che devo calcolare
                                            sscanf(line, "%s %s %s %s %d", _comand, _aggr, _type, _period, &valore);

                                            if(!strcmp(aggr, _aggr)
                                                && !strcmp(type, _type)
                                                    && !strcmp(period, _period))
                                            {
                                                printf("trovato già calcolato !\n");
                                                printf(">>>>>>>>\n");
                                                printf("%s\n", line);

                                                // esco dal while di lettura del file
                                                //mando REPLY_DATA a chi mi aveva inviato REQ_DATA e salto a done !
                                                sprintf(buffer_go, "%s %d %s", "REPLY_DATA", atoi(argv[1]), line);
                                                buffer_len = strlen(buffer_go) +1;
                                                do
                                                {
                                                    ret = sendto(udp_s, buffer_go, buffer_len, 0,
                                                        (struct sockaddr*)&connect_addr, sizeof(connect_addr));
                                                    if(ret<0)
                                                        sleep(polling);
                                                }while(ret<0);

                                                goto done;
                                            }
                                        }
                                    }
                                    // chiudo il file
                                    fclose(fp);
                                }

                                // incremento la data d1 di 1giorno, passo al giorno successivo e quindi controllo il file se esiste
                                // converto: (struct date d1 ----> struct tm* t ---> DatePlusDays(t, +1) ----> d1) 
                                struct tm* t;
                                time(&rawtime);
                                t = localtime(&rawtime);
                                t->tm_year = d2.a-1900;
                                t->tm_mon = d2.m-1;
                                t->tm_mday = d2.g;
                                DatePlusDays(t, -1);
                                d2.a = t->tm_year+1900;
                                d2.m = t->tm_mon+1;
                                d2.g = t->tm_mday;
                                /*
                                printf("day:%d  %d:%d:%d\n", day, d2.a, d2.m, d2.g);   //*************
                                day ++;
                                */
                            }

                            // non ho trovato nulla ! rispondo con REPLY_DATA vuota !
                            sprintf(buffer_go, "%s %d", "REPLY_DATA", atoi(argv[1]));
                            buffer_len = strlen(buffer_go) +1;
                            do
                            {
                                ret = sendto(udp_s, buffer_go, buffer_len, 0,
                                    (struct sockaddr*)&connect_addr, sizeof(connect_addr));
                                if(ret<0)
                                    sleep(polling);
                            }while(ret<0);
done:
                            printf("mando REPLY_DATA a %d\n", port);
                            printf("........end gestione REQ_DATA\n");
                        }
                    }



                    ///////////////////////////////////////////
                    // richiesta di Som q(ei) del giorno "i" //
                    ///////////////////////////////////////////
                    else if(strncmp(buffer_in, "Som", 3) == 0)
                    {
                        char cmd[4], t[15], d[20];
                        int p, val;
                        FILE *ptr;
                        char * line = NULL;
                        size_t len = 0;
                        ssize_t read;

                        sscanf(buffer_in, "%s %d %s %s %d", cmd, &p, t, d, &val);
                        printf("ricevo una richiesta di Som: q(ei) da %d ....\n", p);
                        printf("%s ??\n", buffer_in);
                        // riformo il nome del file verifico se c'è oppure se devo calcolare !
                        // ricevo : sprintf(buffer_go, "%s %d %s %d:%02d:%d", "Som", atoi(argv[1]), type, d1.a, d1.m, d1.g);
                        // e rispondere: //sscanf(buffer_in, "%s %d %d", cmd, &port, &valor);
                        // formato nel file: Som tampone 2021:02:4 30
                        
                        // nome file da guardare
                        char nome_file[20];
                        sprintf(nome_file, "%s_%s", d, argv[1]);
                        //printf("nome_file correct ? %s\n", nome_file);

                        // vediamo se esiste
                        ptr = fopen(nome_file, "r");
                        if(ptr !=NULL)
                        {
                            printf("file esistente !\n");
                            //percorro il file, per ogni record se:
                            //se Entry, verifico il tipo(t) richiesto e sommo
                            //se Som, prendo e verifico il tipo(t), compongo il msg e mando indietro

                            // flag done per uscire dal ciclo di lettura del file appena trovo "Som" nel file
                            // label mi serve per memorizzare il valore del record letto
                            int done = 0, label = 0;

                            while ((read = getline(&line, &len, ptr)) != -1)
                            {

                                // se leggo una q(ei) Som
                                if(strncmp(line, "Som", 3) == 0)
                                {
                                    // variabili  Som tampone 2021:02:4 30
                                    char c[4], _t[13], _d[15];
                                    int _v;
                                    // verifico se corrisponde a quello che devo che mi serve (type ? data non serve !!)
                                    sscanf(line, "%s %s %s %d", c, _t, _d, &_v);
                                    //Som nuovocaso 2021:02:5 4
                                    if(!strcmp(_t, t) /*&& !date_cmp(&stoi(_d), &stoi(d))*/)
                                    {
                                        printf("Som trovato già calcolato!\n");
                                        printf("%s", line);

                                        // messaggio di risposta
                                        sprintf(buffer_go, "%s %d %d", "Som", atoi(argv[1]), _v);

                                        // set done serve subito fuori dal while di lettura
                                        done = 1;

                                        // esco dal while
                                        break;
                                    }
                                }

                                // se leggo invece una Entry
                                else if(strncmp(line, "Entry", 5) == 0)
                                {
                                    // dichiaro variabili per sscanf
                                    char e[6], d_[10], t_[15];
                                    int p_, v_;
                                    sscanf(line, "%s %d %s %s %d", e, &p_, d_, t_, &v_);

                                    // verifico se type richiesto è uguale a quello della Entry letta
                                    if(strcmp(t_, t) == 0)
                                    {
                                        // aggiorno il valore di Som q(ei)
                                        printf("Entry label: %d\n", v_);
                                        label = label + v_;
                                        printf("q(ei): %d\n", label);
                                    }
                                }
                            }

                            // chiudo il file
                            fclose(ptr);

                            // se non avevo trovato Som
                            if(done == 0)
                            {
                                // qui ho il valore q(ei) dentro q[I] però devo salvare nel file: "nome_file"
                                // quindi faccio la entry di Som e salvo
                                sprintf(buffer_go, "%s %s %s %d", "Som", t, d, label);    //Som tampone 2021:02:7 0

                                // salvo nel file di quel giorno
                                ptr = fopen((const char*)nome_file, "a");
                                if(ptr != NULL)
                                {
                                    fprintf(ptr, "%s\n", buffer_go);
                                    printf("salvato nel file !\n");
                                    fclose(ptr);
                                }
                                else
                                    printf("error open file!\n");
                                printf("%s\n", buffer_go);

                            }

                            //mando la risposta al peer requester direttamente ! per evitare di percorrere tutto l'anello
                            //conosco la sua porta, quindi imposto l'addr e lo contatto
                            struct sockaddr_in requester;
                            memset(&requester, 0, sizeof(requester)); //Pulizia
                            requester.sin_family = AF_INET;
                            inet_pton(AF_INET, "127.0.0.1", &requester.sin_addr);
                            requester.sin_port = htons(p);

                            buffer_len = strlen(buffer_go)+1;
                            do
                            {
                                ret = sendto(udp_s, buffer_go, buffer_len, 0,
                                    (struct sockaddr*)&requester, sizeof(requester));
                                if(ret<0)
                                    sleep(polling);
                            }while(ret<0);
                            printf("sent: %s to: %d\n", buffer_go, p);
                            printf(".... end gestione richiesta Som q(ei)\n");
                        }

                        // se il file non esiste, inoltro la richiesta al mio vicino i+1
                        else
                        {
                            strcpy(buffer_go, buffer_in);
                            if(p == array_vicini[0])
                            {
                                sprintf(buffer_go, "%s %s %s %d", "Som", t, d, 0);
                            }
                            printf("file non esistente !\n");
                            buffer_len = strlen(buffer_go)+1;
                            do
                            {
                                ret = sendto(udp_s, buffer_go, buffer_len, 0,
                                    (struct sockaddr*)&array_addr[0], sizeof(array_addr[0]));
                                if(ret<0)
                                    sleep(polling);
                            }while(ret<0);
                            printf("inoltrato: %s a: %d\n", buffer_go, array_vicini[0]);
                            printf(".... end gestione richiesta q(ei)\n");
                        }

                        printf("........end exe Som\n");
                    }

                }




                //#######################
                //##### SOCKET STDIN ####
                //#######################
                else if(i == STDIN)
                {
                    printf("_____SOCKET_STDIN_ON_____\n");

                    fgets(buffer, 50, stdin);
                    //fprintf(stdout, "CHECK: input.. %s", buffer);


                    /////////////////
                    // comand help //
                    /////////////////
                    if(strncmp(buffer, "help", 4) == 0){
                        // stampa il menu !!
                        menu();
                    }


                    ///////////////////
                    // comand start //
                    ///////////////////
                    else if(strncmp(buffer, "start", 5) == 0)
                    {
                        if(connected)
                        {
                            printf("WARN: peer gia connesso !\n");
                        }
                        else
                        {
                            printf("exe start");out();
                            sprintf(buffer_go, "%s %d", "start", atoi(argv[1]));
                            printf("richiesta connessione...\n");
                            buffer_len = strlen(buffer_go) +1;
                            do
                            {
                                // Tento di inviare la richiesta continuamente
                                ret = sendto(udp_s, buffer_go, buffer_len, 0,
                                    (struct sockaddr*)&srv_addr, sizeof(srv_addr));
                                // Se la richiesta non e' stata inviata vado
                                if(ret < 0)
                                    sleep(polling);
                            }while(ret < 0);

                            do
                            {
                                // Tento di ricevere i dati dal server
                                len = sizeof(srv_addr);
                                ret = recvfrom(udp_s, buffer_in, RES_LEN, 0,
                                    (struct sockaddr*)&srv_addr, (socklen_t*)&len);
                                //Se non ricevo niente vado a dormire per un poco
                                if(ret < 0)
                                    sleep(polling);
                            }while(ret < 0);
                            
                            if(strncmp(buffer_in, "no", 2) == 0)
                            {
                                //printf("ricevuto ACK dal DS\n");
                                printf("MSG VUOTO: primo peer\n");
                            }
                            else
                            {
                                // ricezione vicini 
                                printf("recevuto vicini");out();
                                sscanf(buffer_in, "%s %d %d %d", a, &v1, &v2, &v3);
                                printf("v1:%d,v2:%d,v3:%d\n", v1, v2, v3);
                                array_vicini[0] = v1; array_vicini[1] = v2; array_vicini[2] = v3;
                                make_addr(array_addr, array_vicini);
                                printf("connessione UDP con vicini");out();

                                // contattare vicini
                                // contattare vicini udp
                                make_addr(array_addr, array_vicini);

                            }

                            // CHIEDO AL DS LE INFO D'INIZIO: da quando esiste il dataB distribuito
                            // e da quand'è la prima volta che il peer si era connesso!
                            sprintf(buffer_go, "%s %d", "AVVIO", atoi(argv[1]));
                            buffer_len = strlen(buffer_go) +1;
                            do
                            {
                                ret = sendto(udp_s, buffer_go, buffer_len, 0,
                                    (struct sockaddr*)&srv_addr, sizeof(srv_addr));
                                if(ret<0)
                                    sleep(polling);
                            }while(ret<0);

                            // receive 
                            len = sizeof(srv_addr);
                            do
                            {
                                ret = recvfrom(udp_s, buffer_in, 30, 0, (struct sockaddr*)&srv_addr, (socklen_t*)&len);
                                if(ret<0)
                                    sleep(polling);
                            }while(ret<0);
                            sscanf(buffer_in, "%s %s", data_avvio, data_online);

                            //QUESTA VARIABILE MI PERMETTE DI VERIFICARE SE IL PEER È CONNESSO O MENO
                            connected = 1;
                        }
                    }

                    //////////////////////////
                    // add <tipo> <quantity>//
                    //////////////////////////
                    else if(strncmp(buffer, "add", 3) == 0)
                    {
                        printf("........... exe add \n");

                        // eleboro il commando solo se il peer è connected !
                        if(connected)
                        {
                            // do
                            printf("exe add");out();
                            // preparo nomi dei file
                            make_file_name(nome_file_giorno, nome_file_next, atoi(argv[1]));

                            // creazione della entry
                            printf("crea la entry");out();
                            time(&rawtime);
                            timeinfo = localtime(&rawtime);
                            struct data d;
                            d.a = timeinfo->tm_year+1900;
                            d.m = timeinfo->tm_mon+01;
                            d.g = timeinfo->tm_mday;
                            char q[11];
                            itos(q, d);
                            char tipo[15];
                            char commando[10];
                            int quantita;
                            char entry[1024];
                            sscanf(buffer, "%s %s %d", commando, tipo, &quantita);
                            
                            // controllo l'ora prima di inserire
                            if(timeinfo->tm_hour < 18 /*!registro_chiuso()*/)
                            {
                                // entry oggi
                                if(strncmp(tipo, "tampone", 7) == 0){
                                    sprintf(entry, "%s %d %s %s %d", "Entry", atoi(argv[1]), q, "tampone", quantita);
                                }else if(strncmp(tipo, "nuovocaso", 10) == 0){
                                    sprintf(entry, "%s %d %s %s %d", "Entry", atoi(argv[1]), q, "nuovocaso", quantita);
                                }

                                // apro i file
                                file_giorno = fopen((const char*)nome_file_giorno, "a");

                                // inserisco la entry nel file_giorno
                                if(file_giorno != NULL){
                                    fprintf(file_giorno, "%s\n", entry);
                                    fclose(file_giorno);
                                }
                                else
                                    printf("error apertura file !\n");
                                printf("inserito nel file di oggi !");out();
                            }

                            // se è passato le 18h !!
                            else
                            {
                                DatePlusDays(timeinfo, +1);
                                char q[11];
                                struct data d;
                                d.a = timeinfo->tm_year+1900;
                                d.m = timeinfo->tm_mon+01;
                                d.g = timeinfo->tm_mday;
                                itos(q, d);
                                file_next = fopen((const char*)nome_file_next, "a");
                                
                                // entry per giorno dopo 
                                if(strncmp(tipo, "tampone", 7) == 0){
                                    sprintf(entry, "%s %d %s %s %d", "Entry", atoi(argv[1]), q, "tampone", quantita);
                                }else if(strncmp(tipo, "nuovocaso", 10) == 0){
                                    sprintf(entry, "%s %d %s %s %d", "Entry", atoi(argv[1]), q, "nuovocaso", quantita);
                                }

                                //inserisco nel file giorno dopo
                                file_next = fopen((const char*)nome_file_next, "a");
                                if(file_next != NULL){
                                    fprintf(file_next, "%s\n", entry);
                                    fclose(file_next);
                                }
                                else
                                    printf("error apertura file !\n");
                                printf("inserito nel file del giorno sucessivo !");out();
                            }

                            // # MANDO LA ENTRY AL PRIMO VICINO
                            // la entry deve fare il giro dell'anello affinchè i peer siano
                            // update con le entry
                            buffer_len = strlen(entry) +1;
                            do
                            {
                                ret = sendto(udp_s, entry, buffer_len, 0,
                                    (struct sockaddr*)&array_addr[0], sizeof(array_addr[0]));
                                if(ret<0)
                                    sleep(polling);
                            }while(ret<0);
                            sleep(1);
                            printf("ring tour: %s\n", entry);
                        }
                        else
                        {
                            printf("WARN: not connected yet !\n");
                        }

                        printf("............. end exe add\n");
                    }


                    ////////////////////////////////
                    // get <aggr> <type> <period> //
                    ////////////////////////////////
                    else if(strncmp(buffer, "get", 3) == 0)
                    {
                        if(connected)
                        {
                            printf("...........exe get");out();
                            
                            // variabili per vari dati dell'aggregazione
                            char aggr[11], _aggr[11], type[11], _type[11],  period[30], _period[22], comand[4], _comand[4], request[100];
                            int valore, day;
                            char sx[12], dx[12], _periodo_[25];

                            // inizializzo la variabile period a "empty" per poter effettuare il controllo dopo aver letto
                            // il periodo dalla riga di commando
                            strcpy(period, "empty");

                            // prendo le info dal commando che mi serviranno per identificare la aggr nei file
                            sscanf(buffer, "%s %s %s %s",comand, aggr, type, period);
                            printf("get: %s %s %s\n", aggr, type, period);

                            // evito di sporcare il contenuto di period perchè mi serve laggù !
                            strcpy(_periodo_, period);

                            //VERIFICO LA CORRETTEZZA DEL PERIODO
                            //int period_check(sx, dx) //1:correct -- 0:non correct // stampa se non è corretto !
                            printf("|||verifico la correttezza del period|||\n");
                            riga();
                            sleep(1);

                            // se la GET non è "no period"
                            // 
                            if(strcmp(_periodo_, "empty") !=0)
                            {

                                if(strlen(_periodo_) >= 1 && strlen(_periodo_) <11){
                                    printf("WARN:1 periodo non corretto !\n");
                                    goto endget;
                                }

                                char *p;
                                if((p = strrchr(_periodo_, '-')) == NULL)
                                {
                                    printf("WARN:2 periodo non corretto !\n");
                                    goto endget;
                                }

                                // uso strtok per prelevare le info del periodo
                                char *pch = strtok(_periodo_, "-");
                                strcpy(sx, pch);
                                pch = strtok(NULL, "-");
                                strcpy(dx, pch);
                                pch = strtok(NULL, "-");
                                printf("periodo [[ %s ; %s ]]\n", sx, dx);

                                if(!period_check(sx, dx, data_avvio))
                                {
                                    printf("WARN:3 periodo non corretto !\n");
                                    goto endget;
                                }
                            }
                            printf("OK !\n");

                            //VERIFICO LA VALIDITA DELLA RICHIESTA DI ELABORATION
                            printf("\n|||verifico la validità della req di elab|||\n");
                            riga();
                            struct data t1, t2, t3;
                            t2 = stoi(dx);
                            time(&rawtime);
                            timeinfo = localtime(&rawtime);
                            t3.a = timeinfo->tm_year+1900;
                            t3.m = timeinfo->tm_mon+1;
                            t3.g = timeinfo->tm_mday;

                            if((!date_cmp(&t2, &t3)) && (!registro_chiuso()))
                            {
                                printf("WARN: registro del (%s) aperto!\n", dx);
                                goto endget;
                            }
                            
                            if(!strcmp(_periodo_, "empty") && !registro_chiuso())
                            {
                                printf("WARN: registro di oggi aperto !\n");
                                goto endget;
                            }

                            // se no period, imposto le date da considerare
                            // dal primo avvio dell'app fino alla data del giorno
                            if(!strcmp(_periodo_, "empty"))
                            {
                                time(&rawtime);
                                timeinfo = localtime(&rawtime);
                                sprintf(period, "%s-%d:%02d:%d", data_avvio, timeinfo->tm_year+1900, timeinfo->tm_mon, timeinfo->tm_mday);
                                //printf("_periodo_: %s\n", period);
                            }
                            printf("OK !\n");

                            //////////////////////////////////////////////
                            // QUI VERIFICO SE HO IL DATO GIA CALCOLATO //
                            printf("\n||| GUARDO NEI MIEI FILE SE HO L'AGGR GIA CALCOLATA |||\n");
                            riga();


                            struct data d2, d1;
                            char nome_file[20];
                            FILE *fp;
                            int res;
                            time(&rawtime);
                            timeinfo = localtime(&rawtime);
                            d2.a = timeinfo->tm_year+1900;
                            d2.m = timeinfo->tm_mon+01;
                            d2.g = timeinfo->tm_mday;
                            sscanf(data_online, "%d:%d:%d", &d1.a, &d1.m, &d1.g);
                            printf("[ %d:%d:%d  ;  %d:%d:%d ]\n", d1.a, d1.m, d1.g, d2.a, d2.m, d2.g);

                            // controllo i file da quando sono entrato per la prima volta fino ad oggi
                            while(date_cmp(&d1, &d2) ==2 || date_cmp(&d1, &d2) == 0)
                            {
                                printf("\n// DAY: %d:%02d:%d //\n",d2.a, d2.m, d2.g);
                                // creo il nome del file di quel giorno
                                sprintf(nome_file, "%d:%02d:%d_%s", d2.a, d2.m, d2.g, argv[1]); 
                                //printf("file name? %s\n", nome_file);
                                char * line = NULL;
                                size_t len = 0;
                                ssize_t read;

                                // apro in lettura
                                fp = fopen(nome_file, "r");

                                // se il file esiste 
                                if (fp != NULL)
                                {
                                    printf("il file: (%s) esiste !!\n", nome_file); 
                                    
                                    // leggo finche non raggiungo EOF
                                    while ((read = getline(&line, &len, fp)) != -1)
                                    {
                                        printf("  - %s", line);
                                        // se leggo un Aggregazione
                                        if(strncmp(line, "Aggr", 4) == 0)
                                        {
                                            // verifico se corrisponde a quello che devo calcolare
                                            sscanf(line, "%s %s %s %s %d", _comand, _aggr, _type, _period, &valore);
                                            if(!strcmp(aggr, _aggr)
                                                && !strcmp(type, _type)
                                                    && !strcmp(period, _period))
                                            {
                                                printf("trovato già calcolato!\n");
                                                printf("\nRESULT\n");
                                                printf("%s\n", line);

                                                // esco dal while di lettura del file
                                                //salto direttamente a endget
                                                goto endget;
                                            }
                                        }
                                    }
                                    // chiudo il file
                                    fclose(fp);

                                }
                                else
                                    printf("file: (%s) non esiste !\n", nome_file);

                                // incremento la data d1 di 1giorno, passo al giorno successivo e quindi controllo il file se esiste
                                // converto: (struct date d1 ----> struct tm* t ---> DatePlusDays(t, +1) ----> d1) 
                                struct tm* t;
                                time(&rawtime);
                                t = localtime(&rawtime);
                                t->tm_year = d2.a-1900;
                                t->tm_mon = d2.m-1;
                                t->tm_mday = d2.g;
                                DatePlusDays(t, -1);
                                d2.a = t->tm_year+1900;
                                d2.m = t->tm_mon+1;
                                d2.g = t->tm_mday;
                            }
                            printf("\n!! Aggr mai calcolata !!\n");

                            printf("\n|||REQ_DATA AI VICINI|||\n");
                            riga();
                            //////////////////////////////////////////////
                            // QUI FACCIO REQ_DATA AI VICINI PER L'AGGR //
                            // printf("REQ_DATA ai vicini\n");
                            //printf("period_check 3: %s\n", period);

                            // fabbrico l'aggr da richiedere
                            //sprintf(request, "%s %s %s %s", "Aggr", aggr, type, period);

                            //fabbrico il msg da mandare REQ_DATA
                            sprintf(buffer_go, "%s %d %s %s %s", "REQ_DATA", atoi(argv[1]), aggr, type, period);
                            printf("request: %s\n", buffer_go);
                            buffer_len = strlen(buffer_go) +1;
                            for(i=0; i<3; i++)
                            {
                                // mando solo se c'è un vicino alla posizione "i" altrimenti potrei rimanere bloccato nel do{..}while
                                if(array_vicini[i] != -1)
                                {
                                    printf(">>> REQ_DATA a: %d\n", array_vicini[i]);
                                    do
                                    {
                                        ret = sendto(udp_s, buffer_go, buffer_len, 0,
                                            (struct sockaddr*)&array_addr[i], sizeof(array_addr[i]));
                                        if(ret<0)
                                        sleep(polling);
                                    }while(ret<0);
                                }
                            }

                            //////////////////////////////////////////
                            // GESTISCO I REPLY_DATA DI MIEI VICINI //
                            // def di un flag per sapere se un vicino ha gia dato una risposta con aggr richiesta
                            int flag = 0;

                            // recevo da ciascuno e verifico se REPLY_DATA contiene qualcosa
                            for(i=0; i<3; i++)
                            {
                                // ricevo solo se c'è un vicino alla posizione "i" altrimenti potrei rimanere bloccato nel do{..}while
                                if(array_vicini[i] != -1)
                                {
                                    printf("REPLY_DATA da: %d <<<<\n", array_vicini[i]);
                                    len = sizeof(array_addr[i]);
                                    do
                                    {
                                        ret = recvfrom(udp_s, buffer_in, 60, 0,
                                            (struct sockaddr*)&array_addr[i], (socklen_t*)&len);
                                        if(ret<0)
                                        sleep(polling);
                                    }while(ret<0);

                                    // verifico se REPLY_DATA è vuoto o meno
                                    int port;
                                    char cmd[10], agg[60], temp[60];
                                   
                                    // verifico se buffer_in è solo (REPLY_DATA <porta>)
                                    if(strlen(buffer_in) >= 20)
                                    {
                                        //if(flag == 0)
                                        //{
                                            sscanf(buffer_in, "%s %d %s %s %s %s %d", cmd, &port, _comand, _aggr, _type, _period, &valore);
                                    
                                            printf("buffer_in: %s\n", buffer_in);
                                            sprintf(agg, "%s %s %s %s %d", _comand, _aggr, _type, _period, valore);
                                            //printf("va nel file: %s\n", agg);

                                            // devo salvare aggr nel file del giorno
                                            fp = fopen((const char*)nome_file_giorno, "a");
                                            if(fp != NULL)
                                            {
                                                fprintf(fp, "%s\n", agg);
                                                printf("salvato nel file !\n");
                                                fclose(fp);
                                            }
                                            else
                                                printf("error open file!\n");
                                            printf("%s\n", agg);
                                            //strcpy(temp, agg);

                                            // set il flag !
                                            flag = 1;
                                        //}
                                    }
                                }
                            }
                            // se esco dal for con flag = 1 -> che ho ricevuto l'aggr alemno da un peer vicino
                            if(flag == 1)
                            {
                                //salto direttamente all'endget
                                goto endget;
                            }

                            printf("\n|||CALCOLO DELL'AGGREGAZIONE|||\n");
                            riga();
                            //dato che ad ogni ricezione di entry favcevo fare tutto il giro\n
                            //del ring, sono sicuro che ogni peer abbia tutte le entry\n per effettuare il calcolo !\n");
                            /////////////////////////////////////////////////////////////////////
                            // QUI DEVO CALCOLARE L'AGGR PERCHE NEMMENO I MIEI VICINI NE HANNO //
                            // scompongo period con strtok
                            // xche con sscanf non ci riesco ?!

                            //Assegnamento delle date from e to in base al periodo di elab richiesto !
                            ///////////////////////////////////////
                            
                            //struct data d2, d1;
                            //char nome_file[20];
                            //FILE *fp;
                            int q[100], I=0;

                            // q[I] = q(ei) somma giornaliera
                            for(i =0; i<100; i++){
                                q[i] = 0;
                            }

                            // reimposto la data di d2
                            d1 = stoi(sx);
                            d2 = stoi(dx);
                            
                            //no lower bound : imposto la data da considerare con il valore di "primo_avvio"   
                            if(!strcmp(sx, "*"))
                            {
                                printf("data_avvio: %s\n", data_avvio);
                                sscanf(data_avvio, "%d:%d:%d", &d1.a, &d1.m, &d1.g);
                            }

                            //no upper bound : imposto la data del giorno in cui si effettua la GET
                            if(!strcmp(dx, "*"))
                            {
                                time(&rawtime);
                                timeinfo = localtime(&rawtime);
                                d2.a = timeinfo->tm_year+1900;
                                d2.m = timeinfo->tm_mon+01;
                                d2.g = timeinfo->tm_mday;
                            }

                            if (strcmp(_periodo_, "empty") == 0)
                            {
                                d1 = stoi(data_avvio);
                                time(&rawtime);
                                timeinfo = localtime(&rawtime);
                                d2.a = timeinfo->tm_year+1900;
                                d2.m = timeinfo->tm_mon;
                                d2.g = timeinfo->tm_mday;
                            }
                            

                            char from[15], to[15];
                            //converto le date da struct date ------> char *
                            itos(to, d1);
                            itos(from, d2);
                            printf("from: %s ---> to: %s\n", from, to);
                            //se ho il file di quel giorno:
                            //apro, e cerco il valore [q(ei)] del giorno 'i' nel file.
                            //esamino ogni record, finche non trovo q(ei). sommo tutte le entry cosi alla fine avro anche la somma ricercata.
                            //quindi la memorizzo nel file poi l'uso per ottenere il risultato dell'aggr.
                            //se non ho il file di quel giorno (probabilmente perchè il peer non era connesso quel giorno),
                            //chiedo direttamente la q(ei) ai miei vicini:
                            //mandarmi se ce l'hanno, oppure calcolano, memorizzano una copia e me la mandano.

                            //finche (d2 >= d1) questo per percorrere tutti i giorni, compresi [d1, d2]
                            while(date_cmp(&d1, &d2) == 2 || date_cmp(&d1, &d2) == 0)
                            {
                                // creo il nome del file di quel giorno
                                sprintf(nome_file, "%d:%02d:%d_%s", d2.a, d2.m, d2.g, argv[1]);

                                printf("\n// DAY: %d:%02d:%d //\n",d2.a, d2.m, d2.g);
                                // variabili per la lettura nel file
                                char * line = NULL;
                                size_t len = 0;
                                ssize_t read;

                                // apro in lettura
                                fp = fopen(nome_file, "r");

                                // se il file esiste 
                                if (fp != NULL)
                                {
                                    
                                    printf("il file (%s) esiste !\n", nome_file);   //log

                                    // flag done per uscire quando trovo Som: q(ei) nel file
                                    int done = 0;

                                    // leggo ogni record del file
                                    while ((read = getline(&line, &len, fp)) != -1)     // leggo finche read è siverso dal EOF
                                    {
                                        printf("  - %s", line); //log

                                        // se leggo una q(ei) Som ??
                                        if(strncmp(line, "Som", 3) == 0)
                                        {
                                            //                                >> |Aggregazione |tipo    |periodo    |valore <<
                                            // variabili per questo formato:  >> |Som          |tampone |2021:02:4  |30     <<
                                            char c[4], t[13], d[15];
                                            int v;

                                            // verifico se corrisponde a quello che mi serve (type ?) la data non serve tanto
                                            // perche quando un peer si disconnect, manda solo Aggr mai Som, quindi
                                            // è impossibile che ce ne siano Som con date diverse
                                            sscanf(line, "%s %s %s %d", c, t, d, &v);
                                            if(!strcmp(t, type) /*&& !date_cmp(&stoi(d), &d2)*/)
                                            {
                                                printf("Som trovato già calcolato!\n"); //log
                                                printf("%s", line);

                                                // prendo valor che metto nel vettore delle q(ei) e incremento l'indice del vettore
                                                // delle somme giornaliere "I"
                                                q[I] = v;
                                                I ++;

                                                // set done serve subito fuori dal while di lettura
                                                done = 1;

                                                // esco dal while di lettura del file
                                                break;
                                            }
                                        }

                                        // se leggo invece una Entry nel file, 
                                        else if(strncmp(line, "Entry", 5) == 0)
                                        {
                                            // dichiaro variabili per sscanf
                                            char e[6], d[10], t[15];
                                            int p, val;
                                            sscanf(line, "%s %d %s %s %d", e, &p, d, t, &val);

                                            // verifico se type richiesto è uguale a quello della Entry letta
                                            if(strcmp(t, type) == 0)
                                            {
                                                // aggiungo il valore della entry a quello della somma del giorno
                                                q[I] = val + q[I];
                                            }
                                        }
                                    }

                                    // chiudo il file
                                    fclose(fp);

                                    // dopo la lettura del file, devo sapere perche sono uscito ?
                                    // se done vale 0: ---> non ho trovato Som nel file, e quindi devo conciderare la somma delle entry
                                    if(done == 0)
                                    {
                                        // qui ho il valore q(ei) dentro q[I] però devo salvare nel file: "nome_file"
                                        // quindi faccio la entry di Som e salvo
                                        sprintf(buffer_go, "%s %s %d:%02d:%d %d", "Som", type, d2.a, d2.m, d2.g, q[I]);

                                        // salvo nel file di quel giorno
                                        fp = fopen((const char*)nome_file, "a");
                                        if(fp != NULL)
                                        {
                                            fprintf(fp, "%s\n", buffer_go);
                                            printf("\nsalvato nel file !\n");
                                            fclose(fp);
                                        }
                                        else
                                            printf("error open file!\n");
                                        printf("%s\n", buffer_go);

                                        // incremento "I" perche in questo caso lo faccio solo ora
                                        I ++;
                                    }
                                }

                                // se non ho il file, devo chiedere ai miei vicini forse c'erano quel giorno !
                                // FLOOD_FOR_ENTRIES & REQ_ENTRIES
                                else
                                {
                                    printf("\n-- file assente: richiedo q(ei) ai vicini --\n");
                                    riga(); //log

                                    // **Som porta tipo data valore** formato delle richieste di q(ei)
                                    sprintf(buffer_go, "%s %d %s %d:%02d:%d %d", "Som", atoi(argv[1]), type, d2.a, d2.m, d2.g, 0);

                                    printf("request: %s\n", buffer_go);
                                    buffer_len = strlen(buffer_go) +1;

                                    printf(">>> to: %d\n", array_vicini[0]); //log
                                    do
                                    {
                                        ret = sendto(udp_s, buffer_go, buffer_len, 0,
                                            (struct sockaddr*)&array_addr[0], sizeof(array_addr[0]));
                                        if(ret<0)
                                        sleep(polling);
                                    }while(ret<0);


                                    // se ricevo altro che Som: q(ei) salto al label "loop"
                                    // volendo si potrebbe gestire queste richieste inserendole in una coda di attesa, eppoi saltare
                                    // al blocco di gestione corripondente, solo dopo aver finito con la GET.
loop:
                                    printf("attesa risposta di q(ei) ...\n"); //log

                                    // ricevo risposta
                                    len = sizeof(connect_addr);
                                    do
                                    {
                                        ret = recvfrom(udp_s, buffer_in, RES_LEN, 0,
                                            (struct sockaddr*)&connect_addr, (socklen_t*)&len);
                                        if(ret<0)
                                            sleep(polling);
                                    }while(ret<0);

                                    
                                    if(strncmp(buffer_in, "Som", 3) !=0)
                                    {
                                        printf("busy ! can't exe: %s\n", buffer_in);
                                        goto loop;
                                    }

                                    printf("<<< from: ring %s\n", buffer_in);    //log

                                    // verifico se Som è vuoto o meno
                                    int port, valor;
                                    char cmd[5], dat[15], tipo[10];
                                    sscanf(buffer_in, "%s %s %s %d", cmd, tipo, dat, &valor);

                                    // prendo valor che metto nel vettore delle q(ei) e inc I
                                    q[I] = valor;
                                    //printf("q[%d]:%d\n", I, q[I]);
                                    I ++;

                                    // preparo la entry Som che va nel file ex: Som tampone 2021:02:4 30
                                    sprintf(buffer_go, "%s %s %d:%02d:%d %d", "Som", type, d2.a, d2.m, d2.g, valor);

                                    // salvo nel file di quel giorno
                                    fp = fopen((const char*)nome_file, "a");
                                    if(fp != NULL)
                                    {
                                        fprintf(fp, "%s\n", buffer_go);
                                        printf("\nsalvato nel file !\n");
                                        fclose(fp);
                                    }
                                    else
                                        printf("error open file!\n");
                                    printf("%s\n", buffer_go);

                                }

                                // incremento la data d1 di 1giorno, passo al giorno successivo e quindi controllo il file se esiste
                                // converto: (struct date d1 ----> struct tm* t ---> DatePlusDays(t, +1) ----> d1) 
                                struct tm* t;
                                time(&rawtime);
                                t = localtime(&rawtime);
                                t->tm_year = d2.a-1900;
                                t->tm_mon = d2.m-1;
                                t->tm_mday = d2.g;
                                //decremento la data
                                DatePlusDays(t, -1);
                                d2.a = t->tm_year+1900;
                                d2.m = t->tm_mon+1;
                                d2.g = t->tm_mday;
                                
                            }
                            // adesso ho tutte le q(ei) dentro il vettore q[I].
                            // faccio il record dell'aggr calcolato stampo e salvo nel file di oggi.
                            int val_totale = 0;
                            if(!strcmp(aggr, "variazione"))
                            {
                                FILE *fp;
                                sprintf(buffer_go, "%s %s %s %s", "Aggr", "variazione", type, period);
                                fp = fopen((const char*)nome_file_giorno, "a");
                                fprintf(fp, "%s", buffer_go);
                                int i;
                                printf("\nRISULT:\n");
                                for(i=0; i<I-1; i++)
                                {
                                    fprintf(fp, " (q[%d]:%d - q[%d]:%d) = |%d| ", i, q[i], i+1, q[i+1], (q[i]-q[i+1]));
                                    printf("(q[%d]:%d - q[%d]:%d) = |%d|\n", i, q[i], i+1, q[i+1], (q[i]-q[i+1]));
                                }
                                fprintf(fp, " \n");
                                fclose(fp);
                                printf("\n");
                                
                            }
                            else if(!strcmp(aggr, "totale"))
                            {
                                int i;
                                for(i=0; i<I; i++)
                                {
                                    val_totale = val_totale + q[i];
                                }
                                printf("\nRISULT\n");
                                printf("%d\n", val_totale);
                                sprintf(buffer_go, "%s %s %s %s %d", "Aggr", "totale", type, period, val_totale);      //Aggr totale tampone full 100

                                // salvo l'aggr nel file di oggi
                                FILE *fp;
                                fp = fopen((const char*)nome_file_giorno, "a");
                                if(fp != NULL)
                                {
                                    fprintf(fp, "%s\n", buffer_go);
                                    fclose(fp);
                                }
                                else
                                    printf("error open file!\n");
                            }

endget:
                             printf("..........end exe GET\n");
                        }
                        else
                        {
                            printf("WARN: not connected yet !\n");
                        }
                    }



                    //////////
                    // stop //
                    //////////
                    else if(strncmp(buffer, "stop", 3) == 0)
                    {
                        printf("......... exe stop\n");

                        if(connected == 1)
                        {
                            printf("exe stop");out();
                            // manda le aggr che detiene ai vicini peer
                            // per ogni file devo prelevare aggr e mandarle
                            // dalla sua prima connessione fino ad oggi : [data_online ...... data_oggi]
                            // data & nomefile partenza
                            // while(data_cmp(data_online, data_oggi) !=0)
                            // crea il file<peer_port>.bak
                            //time_t rawtime;
                            //struct tm* timeinfo;
                            struct data d2;
                            struct data d1;
                            char nome_file[20];
                            FILE *fp;
                            int res;
                            time(&rawtime);
                            timeinfo = localtime(&rawtime);
                            d2.a = timeinfo->tm_year+1900;
                            d2.m = timeinfo->tm_mon+01;
                            d2.g = timeinfo->tm_mday;
                            sscanf(data_online, "%d:%d:%d", &d1.a, &d1.m, &d1.g);
                            printf("d1: %d:%d:%d  d2: %d:%d:%d\n", d1.a, d1.m, d1.g, d2.a, d2.m, d2.g); //**************

                            // controllo i file da quando sono entrato per la prima volta fino ad oggi
                            while(date_cmp(&d1, &d2) != 1)
                            {
                                // creo il nome del file di quel giorno
                                sprintf(nome_file, "%d:%02d:%d_%s", d1.a, d1.m, d1.g, argv[1]); 
                                printf("file name? %s\n", nome_file); //**********
                                char * line = NULL;
                                size_t len = 0;
                                ssize_t read;

                                // apro in lettura
                                fp = fopen(nome_file, "r");

                                // se il file esiste 
                                if (fp != NULL)
                                {
                                    printf("il file: %s esiste\n", nome_file);  //************
                                    // leggo finche non raggiungo EOF
                                    while ((read = getline(&line, &len, fp)) != -1)
                                    {
                                        // se leggo una info di Aggregazione
                                        if(strncmp(line, "Aggr", 4) == 0)
                                        {
                                            printf("trovato una aggr: %s\n", line); //*****************
                                            // devo mandare ai vicini quindi calcolo la lunghezza de msg
                                            buffer_len = strlen(line) +1;
                                            for(i=0; i<3; i++)
                                            {
                                                // mando solo se c'è un vicino alla posizione "i" altrimenti potrei rimanere bloccato nel do{..}while
                                                if(array_vicini[i] != -1)
                                                {
                                                    printf("mando a: %d\n", array_vicini[i]);
                                                    do
                                                    {
                                                        ret = sendto(udp_s, line, buffer_len, 0,
                                                            (struct sockaddr*)&array_addr[i], sizeof(array_addr[i]));
                                                        if(ret<0)
                                                            sleep(polling);
                                                    }while(ret<0);
                                                }
                                            }
                                        }
                                    }
                                    // chiudo il file
                                    fclose(fp);
                                }

                                // incremento la data d1 di 1giorno, passo al giorno successivo e quindi controllo il file se esiste
                                // converto: (struct date d1 ----> struct tm* t ---> DatePlusDays(t, +1) ----> d1) 
                                struct tm* t;
                                time(&rawtime);
                                t = localtime(&rawtime);
                                t->tm_year = d1.a-1900;
                                t->tm_mon = d1.m-1;
                                t->tm_mday = d1.g;
                                DatePlusDays(t, +1);
                                d1.a = t->tm_year+1900;
                                d1.m = t->tm_mon+1;
                                d1.g = t->tm_mday;
                                printf("d1+1: %d:%d:%d\n", d1.a, d1.m, d1.g);

                            }

                            printf("richiesta di disconnect al DS..\n");
                            sprintf(buffer_go, "%s %d", "STOP", atoi(argv[1]));
                            buffer_len = strlen(buffer_go)+1;

                            do
                            {
                                ret = sendto(udp_s, buffer_go, buffer_len, 0,
                                    (struct sockaddr*)&srv_addr, sizeof(srv_addr));
                                if(ret<0)
                                    sleep(polling);
                            } while (ret<0);

                            len = sizeof(connect_addr);
                            do
                            {
                                ret = recvfrom(udp_s, buffer_in, RES_LEN, 0,
                                    (struct sockaddr*)&connect_addr, (socklen_t*)&len);
                                if(ret<0)
                                    sleep(polling);
                            } while (ret<0);

                            if(strcmp(buffer_in, "STOP OK") == 0)
                            {
                                
                                printf("il DS ha dato OK");out();
                                printf("disconnecting");out();
                                close(udp_s);
                                close(tcp_s);
                                exit(1);
                            }
                        }
                        else
                        {
                            printf("WARN: not connected yet !\n");
                        }

                        printf("______end exe STOP_____ \n");
                    }



                    ////////////////////////
                    // comand non valido ///
                    ////////////////////////
                    else
                    {
                        printf("WARN: comando non valido !\n");
                    }

                }
                

                //##############################################
                //# CASO SOCKET TCP PRONTO PER SCAMBIO_MSG #####
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