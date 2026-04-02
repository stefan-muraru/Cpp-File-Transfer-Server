### Made by Muraru Stefan
This project is licensed under the MIT License - feel free to use, modify, and share it!

# C++ Local File Sharing Server

A simple, high-performance command-line utility written in **C++** to share files from a **Linux** machine to any device with a web browser (optimized for **iPhone/iOS**) over a local network.

## Features
- **Zero Dependencies**: Uses standard C++17 and POSIX sockets.
- **High Speed**: Efficient memory buffering for fast transfers.
- **Mobile Optimized**: Specifically tuned to handle Safari (iOS) connection behaviors and HTTP headers.
- **Reliable**: Handles SIGPIPE signals and multiple browser handshake requests.

## How it Works
The program creates a basic HTTP server using **TCP Sockets**. It listens for incoming connections, handles the HTTP GET request, and streams the file as a binary octet-stream, forcing the browser to trigger a download.


## Installation & Usage
**download the file "server.cpp"**
### How to compile the project:
   ```bash
   g++ -std=c++17 server.cpp -o file_server
 **run the program**
```
### How to run the command (server side)
   To share a file, pass its path as an argument when launching the program. If the filename has spaces, wrap it in quotes: 
   ```bash
   sudo ./file_server "your_file.pdf/.jpeg/ecc..."
```
The server will start, display your local IP address, and stay active (listening on port 8080) until you manually stop it with ```Ctrl+c```
### Hot to download (client side)
1. ***Find the IP***: The program will print your local IP in the terminal (e.g., 192.168.1.106).
2. ***Open browser***: On your iPhone, Android, or another PC, open Safari/Chrome and type:
   ```Plaintext
   http://192.168.1.xxx:80
   ```
   (Replace 192.168.1.xxx with the IP shown in your terminal).
3. ***HTTP ONLY***: You must use ```http://``` and NOT ```https://```. Most mobile browsers try to force HTTPS, which will cause a connection error because this server does not use SSL.
4. ***Save***: A download prompt will appear on your screen. Click *"Download"* to save the file to your *"Files"* or *"Downloads"* folder.

## Critical Troubleshooting & Security
1. ***The Firewall (UFW) - "Connection Refused"***
   If your phone says "Impossible to connect", the Linux firewall is likely blocking port 80. You can open the specific port with:
   * **To allow connection**: ```sudo ufw allow 80/tcp```
   * ***To check status***: ```sudo ufw status```
   * ***To block again later***: ```sudo ufw delete allow 80/tcp```
  
2. ***The "3 Success Messagges" Mystery***
   When downloading on iOS/Safari, the terminal might print "Transfer Successful" 3 times. This is normal behavior:
   * **Request 1**: Safari checks the file headers (metadata) to show the download prompt.
   * **Request 2**: Safari performs a quick stability test of the connection.
   * **Request 3**: The actual data download takes place.
   The server handles each "ping" as a successful interaction.

## Technical Deep Dive (What I Learned)
* **Socket Management**: Implementation of the `socket()`, `bind()`, `listen()`, and `accept()` lifecycle.
* **HTTP Protocol**: manual crafting of HTTP/1.1 headers. Specifically:
  * `Content-Type: application/octet-stream`: Forces the browser to treat data as a binary file.
  * `Content-Disposition: attachment; filename="..."`: Forces the "Save as" prompt with the original name.
* **Buffer & Memory**: Files are streamed in 16KB chunks using `std::vector<char>`, allowing for high-speed transfers without filling up the RAM, even for large files.
* **Request Clearing**: The server reads the client's request buffer (`recv`) before responding to ensure the TCP window is clear, preventing protocol synchronization errors.

A Project Done By Stefan Muraru.
