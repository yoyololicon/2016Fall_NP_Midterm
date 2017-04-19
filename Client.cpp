#include <iostream>
#include <unistd.h>
#include <netinet/in.h>
#include <string.h>
#include <arpa/inet.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <stdlib.h>
#include <string>
using namespace std;
#define MAXLINE 1024

void str_cli(FILE *, int, char*);
int translate_file(char*, char*);

int main(int argc, char **argv)
{
    int                 sockfd;
    struct sockaddr_in  servaddr;

    if(argc != 2)
    {
        cout << "Usage: .. <IPaddr>" << endl;
        return 1;
    }
    sockfd = socket(AF_INET, SOCK_STREAM, 0);

    bzero(&servaddr, sizeof(servaddr));
    servaddr.sin_family = AF_INET;
    servaddr.sin_port = htons(20000);

    if(inet_pton(AF_INET, argv[1], &servaddr.sin_addr) <= 0)
    {
        cerr << "IP addr connect error"  << endl;
        return 1;
    }
    if(connect(sockfd, (struct sockaddr *) &servaddr, sizeof(servaddr)) < 0)
    {
        cerr << "connection fail" << endl;
        return 1;
    }
    str_cli(stdin, sockfd, argv[1]);

    return 0;
}

void str_cli(FILE *fp, int sockfd, char* serv_addr)
{
    int     maxfdp1, stdineof, sfd, rfd, n, upload, download;
    fd_set  rset;
    char    sendline[MAXLINE], recvline[MAXLINE];

    stdineof = 0;
    FD_ZERO(&rset);
    upload = 0;
    download = 0;
    for(;;)
    {
        if(stdineof == 0)
            FD_SET(fileno(fp), &rset);
        FD_SET(sockfd, &rset);

        maxfdp1 = max(fileno(fp), sockfd)+1;

        select(maxfdp1, &rset, NULL, NULL, NULL);

        if(FD_ISSET(sockfd, &rset))
        {
            if((n = read(sockfd, recvline, MAXLINE)) == 0)
            {
                if(stdineof != 1)
                    cout << "serv terminated prematurely" << endl;
                return;
            }
            recvline[n] = 0;
            if(upload)
            {
                if(strcmp(recvline, "destination file open failed\n") == 0)
                {
                    upload = 0;
                    continue;
                }
                rfd = translate_file(serv_addr, recvline);
                while((n = read(sfd, sendline, MAXLINE)))
                {
                    if(n < 0)
                    {
                        printf("Read error\n");
                        break;
                    }
                    n = write(rfd,sendline,(size_t )n);
                    if(n < 0)
                    {
                        printf("Write error\n");
                        break;
                    }
                }
                close(sfd);
                close(rfd);
                upload = 0;
            }
            else if(download)
            {
                if(strcmp(recvline, "source file open failed\n") == 0)
                {
                    download = 0;
                    continue;
                }
                sfd = translate_file(serv_addr, recvline);
                while((n = read(sfd, recvline, MAXLINE)))
                {
                    if(n < 0)
                    {
                        printf("Read error\n");
                        break;
                    }
                    n = write(rfd, recvline,(size_t )n);
                    if(n < 0)
                    {
                        printf("Write error\n");
                        break;
                    }
                }
                close(sfd);
                close(rfd);
                download = 0;
            }
            else
                cout << string(recvline);
        }
        if(FD_ISSET(fileno(fp), &rset))
        {
            if(fgets(sendline, MAXLINE, fp) == NULL)
                cerr << "reading erro occur" << endl;

            if(strcmp(sendline, "EXIT\n") == 0)
            {
                stdineof = 1;
                shutdown(sockfd, SHUT_WR);
                FD_CLR(fileno(fp), &rset);
                continue;
            }
            else if(strncmp(sendline, "PUT ", 4) == 0)
            {
                strcpy(recvline, sendline+4);
                char* tok = strtok(recvline, " \n\r");
                if(tok == NULL)
                    continue;
                sfd = open(tok, O_RDONLY);
                if(sfd < 0)
                {
                    cout << "fail to open file " << string(tok) << endl;
                    continue;
                }
                upload = 1;
            }
            else if(strncmp(sendline, "GET ", 4) == 0)
            {
                strcpy(recvline, sendline+4);
                char* tok = strtok(recvline, " \n\r");
                tok = strtok(NULL, " \n\r");
                if(tok == NULL)
                    continue;
                rfd = open(tok, O_RDWR | O_CREAT, 0644);
                if(rfd < 0)
                {
                    cout << "fail to open file " << string(tok) << endl;
                    continue;
                }
                download = 1;
            }
            if (write(sockfd, sendline, strlen(sendline)) <= 0)
                    cout << "send fail" << endl;
        }
    }
}
int translate_file(char* serv_addr, char* serv_port)
{
    int                 sockfd;
    struct sockaddr_in  servaddr;

    sockfd = socket(AF_INET, SOCK_STREAM, 0);
    bzero(&servaddr, sizeof(servaddr));
    servaddr.sin_family = AF_INET;
    servaddr.sin_port = htons((uint16_t )atoi(serv_port));

    if(inet_pton(AF_INET, serv_addr, &servaddr.sin_addr) <= 0)
    {
        perror("Addr");
        return 0;
    }

    if(connect(sockfd, (struct sockaddr *) &servaddr, sizeof(servaddr)) < 0) {
        perror("translate_file");
        return 0;
    }
    return sockfd;
}