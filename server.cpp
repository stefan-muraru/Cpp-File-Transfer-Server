#include <iostream>
#include <fstream> // stream of files 
#include <string> // for strings
#include <cstring>
#include <vector> // for vectors
#include <filesystem> //to estract easily the name of the file
#include <signal.h> //to mantain the signal during the download of the file
#ifdef _WIN32
    /*-------FOR WINDOWS-------*/
    #include <winsock2.h>  // library for windows
    #include <ws2tcpip.h>
    #include <windows.h> //for sleep()
    #pragma comment(lib, "ws2_32.lib") //instruction for the linker on windows

    #define CLOSE_SOCKET closesocket
    typedef int socklen_t; // Windows uses int for address length in accept()
#else
    /*-------FOR LINUX-------*/
    #include <sys/socket.h> //library for Linux socketset linux
    #include <netinet/in.h> // structures for Internet addresses (IP/Ports)
    #include <unistd.h>     // for system functions like close()

    #define CLOSE_SOCKET close
#endif

int main(int argc, char* argv[]){

#ifndef _WIN32// if NOT defined WINDOWS 
    signal(SIGPIPE, SIG_IGN); // ignore the error if the iPhone abruptly closes the connection
#endif
    //ARGUMENT CHECKING
    //argc is the number of arguments. agrv[0] is the name of the program, arg[1] is the name of the file
    if(argc<2){
        std::cerr<<"error: you must specify a file.\n";
        std::cerr<<"exemple: "<<argv[0]<<" image.jpg"<<std::endl;
        return 1;
    }

    //variables for the file name and path
    std::string complete_path = argv[1];
    //extract only the name(example: "image.jpg") from the full path
    std::string destination_file_name=std::filesystem::path(complete_path).filename().string();

#ifdef _WIN32
    // WINSOCK INITIALIZATION (Strictly required for Windows)
    WSADATA wsaData;
    if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) {
        std::cerr << "WSAStartup failed." << std::endl;
        return 1;
    }
#endif

    //VARIABLE DECLARATION FOR THE SOCKET
    int server_fd;                  //server socket file descriptor
    int new_socket;               //accepted connection file descriptor
    struct sockaddr_in address;     //structure for the IP address and port
    int opt=1;                      //option for reusing the port
    int port=80;                 //used port

    //SOCKET CREATION
    //AF_INET: IPv4 | SOCK_STREAM: TCP
    server_fd=socket(AF_INET, SOCK_STREAM, 0);
    if(server_fd < 0){ 
        perror("socket opening failed");
#ifdef _WIN32
        WSACleanup();
#endif
        return 1;
    }

    //PORT CONFIGURATION (avoids the "address already in use" error)
#ifdef _WIN32
    // Windows expects const char* and SO_REUSEPORT is not commonly used/standard here
    if(setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, (const char*)&opt, sizeof(opt))){
#else
    if(setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR | SO_REUSEPORT, &opt, sizeof(opt))){
#endif
        perror("setsockopt error");
        CLOSE_SOCKET(server_fd);
#ifdef _WIN32
        WSACleanup();
#endif
        return 1;
    }

    // initialize structure memory (prevents crashes on Linux)
    memset(&address, 0, sizeof(address));

    address.sin_family=AF_INET;         // accepts connection from any interface (Wi-fi/Eth)
    address.sin_addr.s_addr=INADDR_ANY; // accepts connection from any local IP
    address.sin_port = htons(port);    // set listening on the port

    //BIND E LISTEN
    if(bind(server_fd, (struct sockaddr*)&address, sizeof(address))<0){
        perror("Bind failed");
        CLOSE_SOCKET(server_fd);
#ifdef _WIN32
        WSACleanup();
#endif
        return 1;
    }

    if (listen(server_fd, 10) < 0){ // listen control
        perror("Listen failed"); 
        CLOSE_SOCKET(server_fd);
#ifdef _WIN32
        WSACleanup();
#endif
        return 1; 
    }

    //GRAFICAL INTERFACE (server side)
    std::cout<<"=========================================================================="<<std::endl;
    std::cout<<"        SERVER STARTED"<<std::endl;
    std::cout<<"        file being shared: "<<destination_file_name<<std::endl;
    std::cout<<"        connect with your phone at http://192.168.1.106:"<<port<<std::endl;
    std::cout<<"        (CAUTION: use HTTP, not HTTPS!)"<<std::endl;
    std::cout<<"=========================================================================="<<std::endl;

    //CONNECTION ACCEPT LOOP
    while(true){

        // VARIABLES FOR THE CLIENT (PHONE)
        struct sockaddr_in client_addr;
        socklen_t client_len = sizeof(client_addr);

        //CORRECT CALL TO ACCEPT
        // client_addr & client_len, not the structure of the server!
        new_socket = accept(server_fd, (struct sockaddr*)&client_addr, &client_len); //new socket

        if(new_socket<0){
            perror("error accepting connection");
#ifdef _WIN32 // if there is a critical error it's better to stop for a moment
            Sleep(1000);
#else
            sleep(1);
#endif
            continue;
        }

        //ADDITION --> put the server in listen mode before sending the whole file
        //this addition is necessary because otherwise iPhone will interrupt the file downloas
        char client_request[4096];
        int byte_received=recv(new_socket, client_request, sizeof(client_request)-1, 0);
        if(byte_received>0){
            client_request[byte_received]='\0';
            //to see what the phone says, uncomment the line below
            // std::cout<<"client request: \n"<<client_request<<std::endl;
        }

        //open file in binary mode
        std::ifstream file(complete_path, std::ios::binary | std::ios::ate);
        if(!file.is_open()){
            std::cerr<<"error: unable to open the file: "<<complete_path<<std::endl;
            std::string msg404="HTTP/1.1 404 Not Found\r\nContent-Length: 0\r\n\r\n";
            send(new_socket, msg404.c_str(), msg404.size(), 0);
            CLOSE_SOCKET(new_socket);
            continue;
        }

        //calculate size
        std::streamsize size_file=file.tellg();
        file.seekg(0, std::ios::beg);

        //HTTP header construction (using original file name)
        std::string header=
            "HTTP/1.1 200 OK\r\n"
            "Content-Type: application/octet-stream\r\n"
            "Content-Disposition: attachment; filename=\"" + destination_file_name + "\"\r\n"
            "Content-Length: " + std::to_string(size_file) + "\r\n"
            "Connection: close\r\n\r\n";

        //send header
        send(new_socket, header.c_str(), header.size(), 0);

        //send content in blocks
        std::vector<char> buffer(16384); //16KB buffer
        while(file.good()){
            file.read(buffer.data(), buffer.size());
            std::streamsize bytes_readed=file.gcount();
            if(bytes_readed>0){
                if(send(new_socket, buffer.data(), bytes_readed, 0)<0){
                    std::cerr<<"connection interrupted by PHONE"<<std::endl;
                    break;
                }
            }
        }

        std::cout<<"File '" << destination_file_name << "' sent successfully!"<<std::endl;

        file.close();
        CLOSE_SOCKET(new_socket);
    }

    CLOSE_SOCKET(server_fd);
#ifdef _WIN32
    WSACleanup();
#endif
    return 0;
}