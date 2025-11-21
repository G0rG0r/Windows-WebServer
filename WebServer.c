#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <WinSock2.h> 
#include <Windows.h> 
#include <ws2tcpip.h> 
#include <sys/stat.h> 


#define PORT 80
#define MAXCLIENTS 100
volatile int UserCount = 0; 


struct Client {
  SOCKET clientSocket;
  struct sockaddr_in clientAddr;
  int clientLen; 
};


DWORD WINAPI NewClient(LPVOID arg);

char* GetClientIP(const struct sockaddr_in* addr, char* buffer, size_t len) {

  strncpy(buffer, inet_ntoa(addr->sin_addr), len);
  buffer[len-1] = '\0';
  return buffer;
}


int main() {
  WSADATA wsaData;
  SOCKET serverSocket;
  struct sockaddr_in serverAddr;
  
 
  if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) {
    fprintf(stderr, "WSAStartup failed: %d\n", WSAGetLastError());
    return 1;
  }

  
  serverSocket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
  if (serverSocket == INVALID_SOCKET) {
    fprintf(stderr, "Socket creation failed: %d\n", WSAGetLastError());
    WSACleanup();
    return 1;
  }


  serverAddr.sin_family = AF_INET;
  serverAddr.sin_port = htons(PORT);

  serverAddr.sin_addr.s_addr = inet_addr("0.0.0.0"); 


  if (bind(serverSocket, (struct sockaddr*)&serverAddr, sizeof(serverAddr)) == SOCKET_ERROR) {
    fprintf(stderr, "Bind failed on port %d: %d\n", PORT, WSAGetLastError());
    closesocket(serverSocket);
    WSACleanup();
    return 1;
  }
  printf("Binded to port %d successfully\n", PORT);


  if (listen(serverSocket, SOMAXCONN) == SOCKET_ERROR) {
    fprintf(stderr, "Listen failed: %d\n", WSAGetLastError());
    closesocket(serverSocket);
    WSACleanup();
    return 1;
  }

  printf("Listening...\n");

  while (1) {
    if (UserCount < MAXCLIENTS) {
      struct sockaddr_in clientAddr;
      int clientLen = sizeof(clientAddr);
      SOCKET clientSocket = accept(serverSocket, (struct sockaddr*)&clientAddr, &clientLen);

      if (clientSocket == INVALID_SOCKET) {
        fprintf(stderr, "Accept failed: %d\n", WSAGetLastError());
        continue;
      }

      struct Client* client = (struct Client*)malloc(sizeof(struct Client));
      if (!client) {
        fprintf(stderr, "Memory allocation failed.\n");
        closesocket(clientSocket);
        continue;
      }

      client->clientSocket = clientSocket;
      client->clientAddr = clientAddr;
      client->clientLen = clientLen;

     
      HANDLE threadHandle = CreateThread(
        NULL,       
        0,        
        NewClient,   
        (LPVOID)client,  
        0,       
        NULL       
      );

      if (threadHandle != NULL) {
        CloseHandle(threadHandle); 
        UserCount++;
      } else {
        fprintf(stderr, "Thread creation failed: %d\n", GetLastError());
        closesocket(clientSocket);
        free(client);
      }
    } else {
      printf("\r server is full number of clients [%d] \n", UserCount);
      
    }
  }
  

  printf("Closing\n");
  closesocket(serverSocket);
  WSACleanup();
  return 0;
}

DWORD WINAPI NewClient(LPVOID arg) {
  struct Client* client = (struct Client*)arg;
  char clientIP[INET_ADDRSTRLEN];
  GetClientIP(&client->clientAddr, clientIP, sizeof(clientIP));
  printf("[+] %s Accepted Connection %d\n", clientIP, UserCount);
  

  FILE* blacklist = fopen("blacklist.txt", "r");
  if (blacklist == NULL) {
    fprintf(stderr, "couldnt open blacklist.txt\n");
  } else {
    char checkIP[INET_ADDRSTRLEN + 1]; 
    size_t len;
    while (fgets(checkIP, sizeof(checkIP), blacklist)) {
      len = strlen(checkIP);
     
      if (len > 0 && checkIP[len - 1] == '\n') {
        checkIP[len - 1] = '\0';
        len--;
      }

      if (strcmp(checkIP, clientIP) == 0) {
        printf("Client %s is Banned\n", clientIP);
        closesocket(client->clientSocket);
        
       
        UserCount--;
        fclose(blacklist);
        free(client);
        return 0;
      }
    }
    fclose(blacklist);
  }
  

  char recvBuffer[1024];
  int recLen;

  while ((recLen = recv(client->clientSocket, recvBuffer, sizeof(recvBuffer) - 1, 0)) > 0) {
    recvBuffer[recLen] = '\0'; 

       
        char method[8];
        char request_path[256];
        char *filepath_to_serve = NULL;

        
        if (sscanf(recvBuffer, "%7s %255s", method, request_path) == 2 && strcmp(method, "GET") == 0) {
            
            
            if (strcmp(request_path, "/") == 0) {
                
                filepath_to_serve = "index.html";
            } else {
              
                filepath_to_serve = request_path + 1;
            }
            printf("Serving requested file: %s\n", filepath_to_serve);
        } else {
            
            const char* notAllowed = "HTTP/1.1 405 Method Not Allowed\r\nContent-Length: 22\r\n\r\n405 Method Not Allowed";
            send(client->clientSocket, notAllowed, strlen(notAllowed), 0);
            break; 
        }

    FILE* htmlFile = fopen(filepath_to_serve, "rb");
    if (htmlFile == NULL) {
      const char* notFoundPage =
        "HTTP/1.1 404 Not Found\r\n"
        "Content-Type: text/plain\r\n"
        "Content-Length: 13\r\n"
        "\r\n"
        "404 Not Found";
      send(client->clientSocket, notFoundPage, strlen(notFoundPage), 0);
    } else {
     
      fseek(htmlFile, 0, SEEK_END);
      long filesize = ftell(htmlFile);
      fseek(htmlFile, 0, SEEK_SET);

      
      char* filebuffer = (char*)malloc(filesize);
      if (filebuffer == NULL) {
        perror("File buffer allocation failed");
        fclose(htmlFile);
        break; 
      }

     
      fread(filebuffer, 1, filesize, htmlFile);
      fclose(htmlFile);

      char header[256];
      int headerLen = snprintf(header, sizeof(header),
                  "HTTP/1.1 200 OK\r\n"
                  "Content-Type: text/html; charset=UTF-8\r\n"
                  "Content-Length: %ld\r\n"
                  "Connection: close\r\n" 
                  "\r\n", filesize);

      send(client->clientSocket, header, headerLen, 0);
      send(client->clientSocket, filebuffer, filesize, 0);
      free(filebuffer);
    }
    
   
    break; 
  }

  
  if (recLen <= 0) {
    printf("[-] %s client disconnected\n", clientIP);
  }
  
  
  if (recLen > 0) {
  
    UserCount--;
  }
  
  closesocket(client->clientSocket);
  free(client);
  return 0; 
}