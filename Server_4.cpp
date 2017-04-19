#include <iostream>
#include <unistd.h>
#include <netinet/in.h>
#include <string.h>
#include <arpa/inet.h>
#include <fcntl.h>
#include <string>
#include <ctime>
#include <vector>
#include <map>
using namespace std;
#define MAXLINE 1024

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

int main(int argc, char **argv) {
    int i, maxi, maxfd, listenfd, flistenfd, connfd, sockfd, ffd;
    int nready, client[FD_SETSIZE];
    ssize_t n;
    fd_set rset, allset;
    char buf[MAXLINE];
    socklen_t clilen;
    struct sockaddr_in cliaddr, servaddr, fservaddr;
    string tmp, fname, id[FD_SETSIZE];
    map<string, vector<string>> own_file;

    listenfd = socket(AF_INET, SOCK_STREAM, 0);

    bzero(&servaddr, sizeof(servaddr));
    servaddr.sin_family = AF_INET;
    servaddr.sin_addr.s_addr = htonl(INADDR_ANY);
    servaddr.sin_port = htons(20000);

    if (bind(listenfd, (struct sockaddr *) &servaddr, sizeof(servaddr)) < 0) {
        cout << "binding error" << endl;
        return 1;
    }
    listen(listenfd, 16);

    maxfd = listenfd;            /* initialize */
    maxi = -1;                    /* index into client[] array */
    for (i = 0; i < FD_SETSIZE; i++)
        client[i] = -1;            /* -1 indicates available entry */
    FD_ZERO(&allset);
    FD_SET(listenfd, &allset);
    srand(time(NULL));

#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wmissing-noreturn"
    for (;;) {
        rset = allset;        /* structure assignment */
        nready = select(maxfd + 1, &rset, NULL, NULL, NULL);

        if (FD_ISSET(listenfd, &rset)) {    /* new client connection */
            clilen = sizeof(cliaddr);
            connfd = accept(listenfd, (struct sockaddr *) &cliaddr, &clilen);

            for (i = 0; i < FD_SETSIZE; i++)
                if (client[i] < 0) {
                    client[i] = connfd;    /* save descriptor */
                    break;
                }
            if (i == FD_SETSIZE)
                cout << "too many clients" << endl;

            FD_SET(connfd, &allset);    /* add new descriptor to set */
            if (connfd > maxfd)
                maxfd = connfd;            /* for select */
            if (i > maxi)
                maxi = i;                /* max index in client[] array */
            if (--nready <= 0)
                continue;                /* no more readable descriptors */
        }

        for (i = 0; i <= maxi; i++) {    /* check all clients for data */
            if ((sockfd = client[i]) < 0)
                continue;
            if (FD_ISSET(sockfd, &rset)) {
                n = readline(sockfd, buf, MAXLINE);
                tmp = string(buf);
                char *tok = strtok(buf, " \n\r");
                if (n == 0) {
                    /*4connection closed by client */
                    close(sockfd);
                    FD_CLR(sockfd, &allset);
                    client[i] =  -1;
                    id[i].clear();
                } else if (strcmp(tok, "CLIENTID:") == 0 && id[i].empty()) {
                    tok = strtok(NULL, " \n\r");
                    if(strlen(tok) != 5)
                        continue;
                    id[i] = string(tok);

                    map<string, vector<string>>::iterator it = own_file.find(id[i]);
                    if (it == own_file.end()) {
                        vector<string> no_file;
                        own_file.insert(pair<string, vector<string>>(id[i], no_file));
                    }
                } else if (strcmp(tok, "PUT") == 0 && !id[i].empty()) {
                    tok = strtok(NULL, " \n\r");
                    if (tok == NULL) {
                        write(sockfd, "destination file open failed\n", 31);
                        continue;
                    }
                    tok = strtok(NULL, " \n\r");
                    if (tok == NULL) {
                        write(sockfd, "destination file open failed\n", 31);
                        continue;
                    }
                    string df(tok);
                    fname = id[i] + "_" + df;
                    ffd = open(fname.c_str(), O_RDWR | O_CREAT, 0644);
                    if (ffd < 0) {
                        write(sockfd, "destination file open failed\n", 31);
                        continue;
                    }

                    flistenfd = socket(AF_INET, SOCK_STREAM, 0);
                    bzero(&fservaddr, sizeof(fservaddr));
                    fservaddr.sin_family = AF_INET;
                    fservaddr.sin_addr.s_addr = htonl(INADDR_ANY);

                    int rand_port = rand()%45534 + 20001;
                    fservaddr.sin_port        = htons((uint16_t )rand_port);

                    if (bind(flistenfd, (struct sockaddr *) &fservaddr, sizeof(fservaddr)) < 0) {
                        cout << "file transfer binding error" << endl;
                        continue;
                    }
                    write(sockfd, to_string(rand_port).c_str(), to_string(rand_port).size());

                    if (listen(flistenfd, 4) < 0)
                        perror(NULL);
                    clilen = sizeof(cliaddr);
                    connfd = accept(flistenfd, (struct sockaddr *) &cliaddr, &clilen);

                    while ((n = read(connfd, buf, MAXLINE)))
                        n = write(ffd, buf, (size_t) n);
                    close(ffd);
                    close(connfd);
                    close(flistenfd);
                    own_file[id[i]].push_back(df);
                    tmp.resize(tmp.size() - 1);
                    tmp += " succeeded\n";
                    message(sockfd, tmp);
                } else if (strcmp(tok, "LIST") == 0 && !id[i].empty()) {
                    for (vector<string>::iterator j = own_file[id[i]].begin(); j != own_file[id[i]].end(); j++) {
                        tmp = *j + "\n";
                        message(sockfd, tmp);
                    }
                    tmp = "LIST succeeded\n";
                    message(sockfd, tmp);
                } else if (strcmp(tok, "GET") == 0 && !id[i].empty()) {
                    tok = strtok(NULL, " \n\r");
                    if (tok == NULL) {
                        write(sockfd, "source file open failed\n", 31);
                        continue;
                    }
                    string sf(tok);
                    fname = id[i] + "_" + sf;
                    ffd = open(fname.c_str(), O_RDONLY);
                    if (ffd < 0) {
                        write(sockfd, "source file open failed\n", 31);
                        continue;
                    }
                    tok = strtok(NULL, " \n\r");
                    if (tok == NULL) {
                        write(sockfd, "source file open failed\n", 31);
                        close(ffd);
                        continue;
                    }

                    flistenfd = socket(AF_INET, SOCK_STREAM, 0);
                    bzero(&fservaddr, sizeof(fservaddr));
                    fservaddr.sin_family = AF_INET;
                    fservaddr.sin_addr.s_addr = htonl(INADDR_ANY);

                    int rand_port = rand()%45534 + 20001;
                    fservaddr.sin_port        = htons((uint16_t )rand_port);

                    if (bind(flistenfd, (struct sockaddr *) &fservaddr, sizeof(fservaddr)) < 0) {
                        cout << "file transfer binding error" << endl;
                        continue;
                    }
                    write(sockfd, to_string(rand_port).c_str(), to_string(rand_port).size());
                    if (listen(flistenfd, 4) < 0)
                        perror(NULL);
                    clilen = sizeof(cliaddr);
                    connfd = accept(flistenfd, (struct sockaddr *) &cliaddr, &clilen);
                    while ((n = read(ffd, buf, MAXLINE)))
                        n = write(connfd, buf, (size_t) n);

                    close(ffd);
                    close(connfd);
                    close(flistenfd);
                    tmp.resize(tmp.size() - 1);
                    tmp += " succeeded\n";
                    message(sockfd, tmp);
                }

                if (--nready <= 0)
                    break;                /* no more readable descriptors */
            }
        }
    }
#pragma clang diagnostic pop
}