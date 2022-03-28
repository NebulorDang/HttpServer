//
// Created by NebulorDang on 2021/11/25.
// HTTP解析器main函数
//
#include "HttpParse.h"

int HttpParse_Main(int argc, char *argv[]) {
    if (argc <= 2) {
        cout << "usage: " << basename(argv[0]) << " ip_address port_number" << endl;
        return -1;
    }
    const char *ip = argv[1];
    int port = atoi(argv[2]);
    struct sockaddr_in address;
    bzero(&address, sizeof(address));
    address.sin_family = AF_INET;
    inet_pton(AF_INET, ip, &address.sin_addr);
    address.sin_port = htons(port);

    int listen_fd = socket(PF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (listen_fd < 0) {
        cerr << "socket error";
        return -1;
    }

    int ret = bind(listen_fd, (struct sockaddr *) &address, sizeof(address));
    if (ret < 0) {
        cerr << "bind error";
        return -1;
    }

    ret = listen(listen_fd, 5);
    if (ret < 0) {
        cerr << "listen error";
        return -1;
    }

    struct sockaddr_in client_address;
    socklen_t client_address_len = sizeof(client_address);
    int accept_fd = accept(listen_fd, (struct sockaddr *) &client_address, &client_address_len);
    if (accept_fd < 0) {
        cerr << "accept error";
        return -1;
    } else {
        char buffer[BUFFER_SIZE];
        memset(buffer, 0, BUFFER_SIZE);
        int data_read = 0;
        int read_index = 0;
        int checked_index = 0;
        int start_index = 0;// keep tha last value of last checked_index
        HttpParse::CHECK_STATE check_state = HttpParse::CHECK_STATE_REQUESTLINE;
        while (1) {
            data_read = recv(accept_fd, buffer + read_index, BUFFER_SIZE - read_index, 0);
            if (data_read == -1) {
                cerr << "reading failed";
                break;
            } else if (data_read == 0) {
                cerr << ("remote client has closed the connection");
                break;
            }
            read_index += data_read;
            shared_ptr<HttpParse> httpParse;
            HttpParse::HTTP_CODE result = httpParse->parse_content(buffer, checked_index, check_state, read_index, start_index);
            if (result == HttpParse::NO_REQUEST) {
                continue;
            } else if (result == HttpParse::GET_REQUEST) {
                cout << "GET_REQUEST" << endl;
                send(accept_fd, szret[0], strlen(szret[0]), 0);
                break;
            } else {
                cout << "BAD_REQUEST" << endl;
                send(accept_fd, szret[1], strlen(szret[1]), 0);
                break;
            }
        }
        close(accept_fd);
    }
    close(listen_fd);
    return 0;
}