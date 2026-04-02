#include <iostream>
#include <fstream> // stream di file 
#include <string> // stringhe
#include <cstring>
#include <vector> // vettori
#include <filesystem> //per estrarre facilmente il nome del file
#include <signal.h> //per mantenere il segnale durante il download del file

//intestazioni specifiche per il networking linux (POSIX)
#include <sys/socket.h> //libreria per i socket linux
#include <netinet/in.h> // Strutture per gli indirizzi Internet (IP/Porte)
#include <unistd.h>     // Per funzioni di sistema come close()
/* programma che fa funzionare il computer ubuntu come server per trasmettere file
    tra computer e telefono iphone
*/

int main(int argc, char* argv[]){

    signal(SIGPIPE, SIG_IGN); // Ignora l'errore se l'iPhone chiude la connessione bruscamente

    //CONTROLLO ARGOMENTI
    //argc è il numero di argomenti. agrv[0] è il nome del programma, arg[1] il nome del file
    if(argc<2){
        std::cerr<<"Errore: devi specificare un file.\n";
        std::cerr<<"Esempio: "<<argv[0]<<" immagine.jpg"<<std::endl;
        return 1;
    }

    //variabili per il nome e il percorso del file
    std::string percorso_completo = argv[1];
    //estraggo solo il nome (esmepio: "foto.jpg") dal persocorso completo 
    std::string nome_file_destinazione=std::filesystem::path(percorso_completo).filename().string();

    //DICHIARAZIONE VARIABILI PER IL SOCKET
    int server_fd;                  //file descriptor del socket server
    int nuovo_socket;               //file descriptor per la connessione accettata
    struct sockaddr_in address;     //struttura per l'indirizzo IP e la porta
    int opt=1;                      //opzione per il riutilizzo della porta
    int porta=80;                 //porta utilizzata

    //CREAZIONE DEL SOCKET
    //AF_INET: IPv4 | SOCK_STREAM: TCP
    server_fd=socket(AF_INET, SOCK_STREAM, 0);
    if(server_fd < 0){ 
        perror("Apertura socket fallita");
        return 1;
    }

    //CONFIGURAZIONE PORTA (evita l'errore "address already in use")
    if(setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR | SO_REUSEPORT, &opt, sizeof(opt))){
        perror("Errore setsockopt");
        return 1;
    }

    // Inizializzazione memoria struttura (Previene crash su Linux)
    memset(&address, 0, sizeof(address));

    address.sin_family=AF_INET;         // accetta connessioni da qualsiasi interfaccia (Wi-fi/Eth)
    address.sin_addr.s_addr=INADDR_ANY; // Accetta connessioni da ogni IP locale
    address.sin_port = htons(porta);    // impongo ascolto della porta

    //BIND E LISTEN
    if(bind(server_fd, (struct sockaddr*)&address, sizeof(address))<0){
        perror("Bind fallito");
        return 1;
    }

    if (listen(server_fd, 10) < 0){ // controllo di listen
        perror("Listen fallita"); 
        return 1; 
    }

    //INTERFACCIA GRAFICA
    std::cout<<"=========================================================================="<<std::endl;
    std::cout<<"        SERVER AVVIATO"<<std::endl;
    std::cout<<"        file in condivisione: "<<nome_file_destinazione<<std::endl;
    std::cout<<"        collegati con iphone a http://192.168.1.106:"<<porta<<std::endl;
    std::cout<<"        (ATTENZIONE: usa http, non https!)"<<std::endl;
    std::cout<<"=========================================================================="<<std::endl;

    //LOOP DI ACCETTAZIONE
    while(true){

        // VARIABILI PER IL CLIENT (L'IPHONE)
        struct sockaddr_in client_addr;
        socklen_t client_len = sizeof(client_addr);

        //CHIAMATA CORRECTA AD ACCEPT
        // client_addr e client_len, non la struttura del server!
        nuovo_socket = accept(server_fd, (struct sockaddr*)&client_addr, &client_len); //nuovo socket

        if(nuovo_socket<0){
            perror("Errore nell'accettare la connessione");
            sleep(1); // se c'è errore critico meglio fermarsi un'attimo
            continue;
        }

        //AGGIUNTA --> mettere il server in ascolto prima di mandare tutto il file
        //questa aggiunta è necessaria poichè se no iphone interromperà il download del file
        char richiesta_client[4096];
        int byte_ricevuti=recv(nuovo_socket, richiesta_client, sizeof(richiesta_client)-1, 0);
        if(byte_ricevuti>0){
            richiesta_client[byte_ricevuti]='\0';
            //per vedere cosa dice iphone decommentare la riga sotto
            // std::cout<<"richiesta iphone: \n"<<richiesta_client<<std::endl;
        }

        //apertura file in modalità binaria
        std::ifstream file(percorso_completo, std::ios::binary | std::ios::ate);
        if(!file.is_open()){
            std::cerr<<"impossibile aprire il file: "<<percorso_completo<<std::endl;
            std::string msg404="HTTP/1.1 404 Not Found\r\nContent-Length: 0\r\n\r\n";
            send(nuovo_socket, msg404.c_str(), msg404.size(), 0);
            close(nuovo_socket);
            continue;
        }

        //calcolo dimensione
        std::streamsize dimensione_file=file.tellg();
        file.seekg(0, std::ios::beg);

        //costruzione header HTTP (usando nome del file originale)
        // Corretti "Lenght" -> "Length" e "clore" -> "close"
        std::string header=
            "HTTP/1.1 200 OK\r\n"
            "Content-Type: application/octet-stream\r\n"
            "Content-Disposition: attachment; filename=\"" + nome_file_destinazione + "\"\r\n"
            "Content-Length: " + std::to_string(dimensione_file) + "\r\n"
            "Connection: close\r\n\r\n";

        //invio header
        send(nuovo_socket, header.c_str(), header.size(), 0);

        //invio contenuto a blocchi
        std::vector<char> buffer(16384); //buffer da 16KB 
        while(file.good()){
            file.read(buffer.data(), buffer.size());
            std::streamsize bytes_letti=file.gcount();
            if(bytes_letti>0){
                if(send(nuovo_socket, buffer.data(), bytes_letti, 0)<0){
                    std::cerr<<"connessione interrotta da IPHONE"<<std::endl;
                    break;
                }
            }
        }

        std::cout<<"File '" << nome_file_destinazione << "' inviato con successo!"<<std::endl;

        file.close();
        close(nuovo_socket);
    }

    close(server_fd);
    return 0;
}