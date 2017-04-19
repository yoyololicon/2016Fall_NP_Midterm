#include <iostream>
#include <unistd.h>
#include <netinet/in.h>
#include <string.h>
#include <arpa/inet.h>
#include <sys/wait.h>
#include <fcntl.h>
#include <ctime>
#include <string>
#include <vector>
using namespace std;
#define MAXLINE 1024

void sig_child(int signo)
{
    while (waitpid(-1, NULL, WNOHANG) > 0);
    return;
}

ssize_t readline(int fd, void *vptr, size_t maxlen)
{
    ssize_t n, rc;
    char    c, *ptr;
    int white = 0;

    ptr = (char*)vptr;
    for(n = 1; n < maxlen; n++)
    {
        again:
        if((rc = read(fd, &c, 1)) == 1) {
            if (white) {
                if (c != ' ') {
                    white = 0;
                    *ptr++ = c;
                }
            } else {
                if (c == ' ')
                    white = 1;
                *ptr++ = c;
            }
            if (c == '\n')
                break;
        }
        else if(rc == 0)
        {
            *ptr = 0;
            return n-1;
        }
        else
        {
            if(errno == EINTR)
                goto again;
            return -1;
        }
    }

    *ptr = 0;
    return n;
}

int message(int fd, string &mes)
{
    char tmp[MAXLINE];
    strcpy(tmp, mes.c_str());
    if(write(fd, tmp, strlen(tmp)) < 0) {
        cout << "fail to sent message" << endl;
        return 0;
    }
    return 1;
}

int main(int argc, char **argv)
{
    int					listenfd, flistenfd, connfd, sockfd, ffd, rand_user;
    ssize_t				n;
    char				buf[MAXLINE];
    socklen_t			clilen;
    struct sockaddr_in	cliaddr, servaddr, fservaddr;
    struct sigaction    act;
    string              tmp;
    pid_t pid;

    listenfd = socket(AF_INET, SOCK_STREAM, 0);

    bzero(&servaddr, sizeof(servaddr));
    servaddr.sin_family      = AF_INET;
    servaddr.sin_addr.s_addr = htonl(INADDR_ANY);
    servaddr.sin_port        = htons(20000);

    if(bind(listenfd, (struct sockaddr *) &servaddr, sizeof(servaddr)) < 0) {
        cout << "binding error" << endl;
        return 1;
    }
    listen(listenfd, 16);

    srand(time(NULL));

    act.sa_handler = sig_child;
    sigemptyset(&act.sa_mask);
    act.sa_flags = SA_RESTART;
    if(sigaction(SIGCHLD, &act, NULL) < 0)
    {
        cerr << "sigaction err" << endl;
        return 1;
    }

#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wmissing-noreturn"
    for ( ; ; ) {

        clilen = sizeof(cliaddr);
        connfd = accept(listenfd, (struct sockaddr *) &cliaddr, &clilen);
        if(connfd < 0)
        {
            if(errno == EINTR)
                continue;
            cerr << "accept err" << endl;
            return 1;
        }

        rand_user = rand()%FD_SETSIZE+1;
        pid = fork();
        if(pid < 0)
        {
            cerr << "fork fail" << endl;
            exit(EXIT_FAILURE);
        }
        else if(pid == 0)
        {
            close(listenfd);
            vector<string> files;
            string prefix = to_string(rand_user);

            while(1)
            {
                if(readline(connfd, buf, MAXLINE) == 0) {
                    for(vector<string>::iterator j = files.begin(); j != files.end(); j++)
                    {
                        tmp = prefix + "_" + *j;
                        unlink(tmp.c_str());
                    }
                    cout << "Client has closed the connection." << endl;
                    if(close(connfd) < 0)
                        perror("close connfd");
                    exit(EXIT_SUCCESS);
                }
                tmp = string(buf);
                char* tok = strtok(buf, " \n\r");

                if(strcmp(tok, "PUT") == 0)
                {
                    tok = strtok(NULL, " \n\r");
                    if(tok == NULL)
                    {
                        write(connfd, "destination file open failed\n", 31);
                        continue;
                    }
                    tok = strtok(NULL, " \n\r");
                    if(tok == NULL )
                    {
                        write(connfd, "destination file open failed\n", 31);
                        continue;
                    }
                    string df(tok);

                    string fname = prefix + "_" + df;
                    ffd = open(fname.c_str(), O_RDWR | O_CREAT, 0644);
                    if(ffd < 0)
                    {
                        write(connfd, "destination file open failed\n", 31);
                        continue;
                    }

                    flistenfd = socket(AF_INET, SOCK_STREAM, 0);
                    bzero(&fservaddr, sizeof(fservaddr));
                    fservaddr.sin_family      = AF_INET;
                    fservaddr.sin_addr.s_addr = htonl(INADDR_ANY);

                    int rand_port = rand()%45534 + 20001;
                    fservaddr.sin_port        = htons((uint16_t )rand_port);

                    if(bind(flistenfd, (struct sockaddr *) &fservaddr, sizeof(fservaddr)) < 0) {
                        cout << "file transfer binding error" << endl;
                        continue;
                    }

                    write(connfd, to_string(rand_port).c_str(), to_string(rand_port).size());

                    if(listen(flistenfd, 4) < 0)
                        perror(NULL);
                    clilen = sizeof(cliaddr);
                    sockfd = accept(flistenfd, (struct sockaddr *) &cliaddr, &clilen);

                    while((n = read(sockfd, buf, MAXLINE)))
                        n = write(ffd, buf, (size_t) n);
                    if(close(ffd) < 0)
                        perror("close file");
                    if(close(sockfd) < 0)
                        perror("close sockfd");
                    if(close(flistenfd) < 0)
                        perror("close listen");
                    files.push_back(df);
                    tmp.resize(tmp.size()-1);
                    tmp += " succeeded\n";
                    message(connfd, tmp);
                }
                else if(strcmp(tok, "LIST") == 0) {
                    for (vector<string>::iterator j = files.begin(); j != files.end(); j++) {
                        tmp = *j + "\n";
                        message(connfd, tmp);
                    }
                    tmp = "LIST succeeded\n";
                    message(connfd, tmp);
                }
                else if(strcmp(tok, "GET") == 0)
                {
                    tok = strtok(NULL, " \n\r");
                    if(tok == NULL )
                    {
                        write(connfd, "source file open failed\n", 31);
                        continue;
                    }
                    string sf(tok);
                    string fname = prefix + "_" + sf;
                    ffd = open(fname.c_str(), O_RDONLY);
                    if(ffd < 0)
                    {
                        write(connfd, "source file open failed\n", 31);
                        continue;
                    }
                    tok = strtok(NULL, " \n\r");
                    if(tok == NULL )
                    {
                        write(connfd, "source file open failed\n", 31);
                        close(ffd);
                        continue;
                    }

                    flistenfd = socket(AF_INET, SOCK_STREAM, 0);
                    bzero(&fservaddr, sizeof(fservaddr));
                    fservaddr.sin_family      = AF_INET;
                    fservaddr.sin_addr.s_addr = htonl(INADDR_ANY);

                    int rand_port = rand()%45534 + 20001;
                    fservaddr.sin_port        = htons((uint16_t )rand_port);

                    if(bind(flistenfd, (struct sockaddr *) &fservaddr, sizeof(fservaddr)) < 0) {
                        cout << "file transfer binding error" << endl;
                        continue;
                    }
                    write(connfd, to_string(rand_port).c_str(), to_string(rand_port).size());
                    if(listen(flistenfd, 4) < 0)
                        perror(NULL);
                    clilen = sizeof(cliaddr);
                    sockfd = accept(flistenfd, (struct sockaddr *) &cliaddr, &clilen);
                    while((n = read(ffd, buf, MAXLINE)))
                        n = write(sockfd, buf, (size_t) n);

                    if(close(ffd) < 0)
                        perror("close file");
                    if(close(sockfd) < 0)
                        perror("close sockfd");
                    if(close(flistenfd) < 0)
                        perror("close listen");
                    tmp.resize(tmp.size()-1);
                    tmp += " succeeded\n";
                    message(connfd, tmp);
                }
            }
        }
        if(close(connfd) < 0)
        {
            perror("close connfd");
            return 0;
        }
    }
#pragma clang diagnostic pop
}