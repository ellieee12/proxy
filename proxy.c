#include  <stdio.h>
#include  <stdlib.h>
#include  <sys/socket.h>
#include  <netdb.h>
#include  <string.h>
#include  <unistd.h>
#include  <stdbool.h>
#include "./simpleSocketAPI.h"


#define SERVADDR "127.0.0.1"        // Définition de l'adresse IP d'écoute
#define SERVPORT "0"                // Définition du port d'écoute, si 0 port choisi dynamiquement
#define LISTENLEN 1                 // Taille de la file des demandes de connexion
#define MAXBUFFERLEN 1024           // Taille du tampon pour les échanges de données
#define MAXHOSTLEN 64               // Taille d'un nom de machine
#define MAXPORTLEN 64               // Taille d'un numéro de port
#define MODEDATACLIENT "PORT %d,%d,%d,%d,%d,%d" //Format de commande PORT du client FTP 
#define MODEPASV "%*[^(](%d,%d,%d,%d,%d,%d)" //Format de réponse PASV du serveur FTP

void extraitAdressePort(char * mode, char * buffer, char *adresseIP, char * portData);
void calculationPort(int port1, int port2, char * portData);

int main(){
    int ecode;                       // Code retour des fonctions
    char serverAddr[MAXHOSTLEN];     // Adresse du serveur
    char serverPort[MAXPORTLEN];     // Port du server
    int descSockRDV;                 // Descripteur de socket de rendez-vous
    int descSockCOM;                 // Descripteur de socket de communication
    struct addrinfo hints;           // Contrôle la fonction getaddrinfo
    struct addrinfo *res;            // Contient le résultat de la fonction getaddrinfo
    struct sockaddr_storage myinfo;  // Informations sur la connexion de RDV
    struct sockaddr_storage from;    // Informations sur le client connecté
    socklen_t len;                   // Variable utilisée pour stocker les 
				                     // longueurs des structures de socket
    char buffer[MAXBUFFERLEN];       // Tampon de communication entre le client et le serveur
    
    // Initialisation de la socket de RDV IPv4/TCP
    descSockRDV = socket(AF_INET, SOCK_STREAM, 0);
    if (descSockRDV == -1) {
         perror("Erreur création socket RDV\n");
         exit(2);
    }
    // Publication de la socket au niveau du système
    // Assignation d'une adresse IP et un numéro de port
    // Mise à zéro de hints
    memset(&hints, 0, sizeof(hints));
    // Initialisation de hints
    hints.ai_flags = AI_PASSIVE;      // mode serveur, nous allons utiliser la fonction bind
    hints.ai_socktype = SOCK_STREAM;  // TCP
    hints.ai_family = AF_INET;        // seules les adresses IPv4 seront présentées par 
				                      // la fonction getaddrinfo

     // Récupération des informations du serveur
     ecode = getaddrinfo(SERVADDR, SERVPORT, &hints, &res);
     if (ecode) {
         fprintf(stderr,"getaddrinfo: %s\n", gai_strerror(ecode));
         exit(1);
     }
     // Publication de la socket
     ecode = bind(descSockRDV, res->ai_addr, res->ai_addrlen);
     if (ecode == -1) {
         perror("Erreur liaison de la socket de RDV");
         exit(3);
     }
     // Nous n'avons plus besoin de cette liste chainée addrinfo
     freeaddrinfo(res);

     // Récuppération du nom de la machine et du numéro de port pour affichage à l'écran
     len=sizeof(struct sockaddr_storage);
     ecode=getsockname(descSockRDV, (struct sockaddr *) &myinfo, &len);
     if (ecode == -1)
     {
         perror("SERVEUR: getsockname");
         exit(4);
     }
     ecode = getnameinfo((struct sockaddr*)&myinfo, sizeof(myinfo), serverAddr,MAXHOSTLEN, 
                         serverPort, MAXPORTLEN, NI_NUMERICHOST | NI_NUMERICSERV);
     if (ecode != 0) {
             fprintf(stderr, "error in getnameinfo: %s\n", gai_strerror(ecode));
             exit(4);
     }
     printf("L'adresse d'ecoute est: %s\n", serverAddr);
     printf("Le port d'ecoute est: %s\n", serverPort);

     // Definition de la taille du tampon contenant les demandes de connexion
     ecode = listen(descSockRDV, LISTENLEN);
     if (ecode == -1) {
         perror("Erreur initialisation buffer d'écoute");
         exit(5);
     }

     //Boucle principale pour accepter et gérer les connexions des clients
    while (true) {
        len = sizeof(struct sockaddr_storage);
        // Acceptation d'une connexion client
        descSockCOM = accept(descSockRDV, (struct sockaddr*)&from, &len);
        if (descSockCOM == -1) {
            perror("Error accepting connection");
            continue; // Continue à écouter les autres clients
        }

        // Création d'un processus enfant pour gérer la connexion
        pid_t pid = fork();
        if (pid < 0) {
            perror("Error forking process");
            close(descSockCOM); // Fermeture de la socket en cas d'échec
            continue;
        }

        //PROCESSUS ENFANT
        if (pid == 0) {
            // Child process
            close(descSockRDV); // L'enfant n'a pas besoin de la socket d'écoute

            //GESTION DES ECHANGES 
            char i[] = "21";
            char hostname[50], username[50];
            int descSockServer;
            strcpy(buffer, "220 Bienvenu au ftp\n");
            write(descSockCOM, buffer, strlen(buffer));

            //Recuperer le username et le hostname
            ecode = read(descSockCOM, buffer, MAXBUFFERLEN);
            if (ecode == -1) {perror("Problème de lecture de client\n"); exit(6);}
            buffer[ecode] = '\0';
            //Décomposition de buffer pour récuperer le username et le hostname
            sscanf(buffer, "%[^@]@%[^\n]", username, hostname);
            hostname[strlen(hostname)-1] = '\0';

            // Essayer de connecter au serveur
            ecode = connect2Server(hostname, "21", &descSockServer);
            if (ecode == -1) {perror("Problème de connexion\n"); exit(3);}
            
            //Récuperer la réponse auprès le serveur
            ecode = read(descSockServer, buffer, MAXBUFFERLEN-1);
            if (ecode == -1) {perror("Problème de lecture de serveur\n"); exit(7);}
            buffer[ecode]='\0';
            strcat(username, "\r\n");

            //Envoyer username au serveur
            write(descSockServer, username, strlen(username));
            if (ecode == -1) {perror("Problème d'écriture au serveur\n"); exit(8);}
            buffer[ecode]='\0';

            //Récuperer "331 Anonymous login ok, send..." du serveur
            ecode = read(descSockServer, buffer, MAXBUFFERLEN-1);
            if (ecode == -1) {perror("Problème de lecture de serveur\n"); exit(7);}
            buffer[ecode]='\0';

            //Afficher le message "331 Anonymous login ok, send ..." sur le terminal client
            write(descSockCOM, buffer, strlen(buffer));
            if (ecode == -1) {perror("Problème d'écriture au client\n"); exit(9);}
            buffer[ecode]='\0';

            //Lire le mot de pass saisi par le client
            ecode = read(descSockCOM, buffer, MAXBUFFERLEN-1);
            if (ecode == -1) {perror("Problème de lecture de client\n"); exit(6);}
            buffer[ecode]='\0';

            //Envoyer PASS mdp au serveur
            write(descSockServer,buffer,ecode);
            if (ecode == -1) {perror("Problème d'écriture au serveur\n"); exit(8);}
            buffer[ecode] = '\0';

            //Lire le message "230-The FTP service on this host ..."
            ecode = read(descSockServer, buffer, MAXBUFFERLEN-1);
            if (ecode == -1) {perror("Problème de lecture de serveur\n"); exit(7);}
            buffer[ecode] = '\0';
            //Afficher le message "230-The FTP service on this host ..." sur le terminal client
            write(descSockCOM,buffer,strlen(buffer));
            if (ecode == -1) {perror("Problème d'écriture au client\n"); exit(9);}

            //Lire le message "230 Anonymous access granted, restrrictions apply"
            ecode = read(descSockServer, buffer, MAXBUFFERLEN-1);
            if (ecode == -1) {perror("Problème de lecture de serveur\n"); exit(7);}
            buffer[ecode] = '\0';
            //Afficher le message "230 Anonymous access granted, restrrictions apply"
            write(descSockCOM,buffer,strlen(buffer));
            if (ecode == -1) {perror("Problème d'écriture au client\n"); exit(9);}

            //Lire la commande SYST sur le terminal de client
            ecode = read(descSockCOM, buffer, MAXBUFFERLEN-1);
            if (ecode == -1) {perror("Problème de lecture de client\n"); exit(6);}
            buffer[ecode] = '\0';
            //Envoyer la commande SYST au serveur
            write(descSockServer,buffer,ecode);
            if (ecode == -1) {perror("Problème d'écriture au serveur\n"); exit(8);}

            // Lire "215 Unix Type: L8..." auprès le serveur 
            ecode = read(descSockServer, buffer, MAXBUFFERLEN-1);
            if (ecode == -1) {perror("Problème de lecture de serveur\n"); exit(7);}
            buffer[ecode] = '\0';
            //Afficher "215 Unix Type: L8..." sur terminal client
            write(descSockCOM, buffer, strlen(buffer));
            if (ecode == -1) {perror("Problème d'écriture au client\n"); exit(9);}


            int descSockServerDATA;
            int descSockCOMDATA;
            char adresseIP[50], portData[50],cmd[50];
            //Initialisation des tableaux pour stocker l'adress IP, le port et les commandes
            memset(adresseIP, 0, sizeof(adresseIP));
            memset(portData, 0, sizeof(portData));
            memset(cmd, 0, sizeof(cmd));
            //Lire la commande envoyée par le client
            ecode = read(descSockCOM, cmd, MAXBUFFERLEN-1);
            cmd[ecode] = '\0';

            //Boucle principale pour traiter les commandes tant que "QUIT" n'est pas reçu
            while (strncmp(cmd,"QUIT",4)!=0){
                if (strncmp(cmd,"PORT",4)==0){
                    //Extraction de l'adresse et du port à partir de la commande "PORT"
                    extraitAdressePort(MODEDATACLIENT, cmd, adresseIP, portData);
                    printf("%s\n", adresseIP);
                    printf("%s\n", portData);
                    //create connection on client side
                    ecode=connect2Server(adresseIP, portData, &descSockCOMDATA);

                    //Confirmation de la commande "PORT" réussie
                    char success[] = "200 PORT command successful\n";
                    strcpy(buffer, success);
                    ecode = write(descSockCOM, buffer, strlen(success));
                    if (ecode == -1) {perror("Problème d'écriture au client\n"); exit(9);}

                    //Lecture de la réponse "PASV" du serveur
                    strcpy(buffer, "PASV\r\n");
                    printf("%s\n", buffer);
                    ecode = write(descSockServer, buffer, strlen(buffer));
                    if (ecode == -1) {perror("Problème d'écriture au serveur\n"); exit(8);}

                    // Lecture de la réponse "PASV" du serveur
                    ecode = read(descSockServer, buffer, MAXBUFFERLEN-1);
                    if (ecode == -1) {perror("Problème de lecture de serveur\n"); exit(7);}
                    buffer[ecode] = '\0';
                    printf("%s\n", buffer);

                    // Extraction de l'adresse et du port côté serveur
                    extraitAdressePort(MODEPASV, buffer, adresseIP, portData);
                    printf("%s\n", adresseIP);
                    printf("%s\n", portData);

                     // Connexion au serveur côté données
                    ecode=connect2Server(hostname, portData, &descSockServerDATA);
                    printf("Connexion avec le serveur Data FTP OK\n");
                    printf("Proxy passé en mode passive\n");

                    // Lecture de la commande suivante du client
                    ecode = read(descSockCOM,buffer,MAXBUFFERLEN-1);
                    buffer[ecode]='\0';
                    printf("7 - %s\n,%d\n",buffer,strlen(buffer));

                     // Transmission de la commande au serveur
                    write(descSockServer,buffer,ecode);


                    // Lecture du message 150 envoyé par le serveur
                    ecode = read(descSockServer,buffer,MAXBUFFERLEN-1);
                    buffer[ecode]='\0';
                    write(descSockCOM,buffer,ecode);

                    // Lecture des données côté serveur
                    ecode = read(descSockServerDATA,buffer,MAXBUFFERLEN-1);
                    buffer[ecode]='\0';

                    // Transmission des données au client
                    while (ecode>0){
                        write(descSockCOMDATA,buffer,strlen(buffer));
                        ecode = read(descSockServerDATA, buffer, MAXBUFFERLEN -1);
                        buffer[ecode] = '\0';
                    }

                    // Fermeture des sockets de données
                    close(descSockCOMDATA);
                    close(descSockServerDATA);
                    printf("SOCKET SERVEUR DATA et SOCKET CLIENT DATA fermées\n");

                     // Lecture du message 226 de fin de transfert
                    ecode = read(descSockServer,buffer,MAXBUFFERLEN-1);
                    buffer[ecode]='\0';
                    write(descSockCOM,buffer,ecode);
                }else{
                    // Traitement des commandes autres que "PORT"
                    ecode = write(descSockServer,cmd,strlen(cmd));
                    ecode = read(descSockServer,buffer,MAXBUFFERLEN);
                    while(ecode>0){
                        write(descSockCOM,buffer,strlen(buffer));
                        ecode=read(descSockServer,buffer,MAXBUFFERLEN-1);
                        buffer[ecode]='\0';
                    }
                }
                // Lecture de la commande suivante
                ecode = read(descSockCOM, cmd, MAXBUFFERLEN -1);
                cmd[ecode] = '\0';
            }
            // Traitement de la commande "QUIT"
            ecode = write(descSockServer,cmd,strlen(cmd));
            ecode = read(descSockServer,buffer,MAXBUFFERLEN);
            buffer[ecode]='\0';
            write(descSockCOM,buffer,ecode);
            
            // Fermeture des sockets après la fin de la communication
            close(descSockRDV);
            close(descSockCOM); // Fermeture du socket de communication
            close(descSockServer); // Fermeture du socket du serveur si connecté
            exit(0); // Terminaison du processus enfant
        } else {
            // Processus parent
            close(descSockCOM); // Le parent n'a pas besoin du socket de communication
        }
    }

}

//Extraire l'adresse IP et le port d'une commande FTP (PORT ou PASV)
void extraitAdressePort(char * mode, char * buffer, char *adresseIP, char * portData){
    int ipPort[6];  
    sscanf(buffer, mode, &ipPort[0], &ipPort[1], &ipPort[2], &ipPort[3], &ipPort[4], &ipPort[5]);
    sprintf(adresseIP, "%d.%d.%d.%d", ipPort[0], ipPort[1], ipPort[2], ipPort[3]);
    calculationPort(ipPort[4], ipPort[5], portData);
}


// Calculer le numéro de port à partir de deux octets
void calculationPort(int port1, int port2, char * portData){
    int numPort = port1*256 + port2;
    sprintf(portData, "%d", numPort);
}
