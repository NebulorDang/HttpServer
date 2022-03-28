//
// Created by NebulorDang on 2021/11/25.
// HTTP解析器
//

#ifndef WEBSERVER_HTTPPARSE_H
#define WEBSERVER_HTTPPARSE_H

#include <iostream>
#include <libgen.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <cstring>
#include <arpa/inet.h>
#include <regex>
#include <unistd.h>

using namespace std;

//static const char *szret[] = {"I get a correct result\n", "Something wrong\n"};
static const char* szret[] = {"HTTP/1.0 404 NOT FOUND\r\nContent-Type: text/html\r\n\r\n<HTML><TITLE>Not Found</TITLE>\r\n<BODY><P>The server could not fulfill\r\nis unavailable or nonexistent.\r\n</BODY></HTML>\r\n",
                              "Something wrong\n"};
const int BUFFER_SIZE = 4096;

class HttpParse {
public:
    enum CHECK_STATE {
        CHECK_STATE_REQUESTLINE, CHECK_STATE_HEADER, CHECK_STATE_BODY
    };
    enum LINE_STATUS {
        LINE_OK, LINE_BAD, LINE_OPEN
    };
    enum HTTP_CODE {
        NO_REQUEST, GET_REQUEST, BAD_REQUEST, FORBIDDEN_REQUEST, INTERNAL_ERROR, CLOSED_CONNECTION
    };
    HTTP_CODE parse_request_line(char *cur, CHECK_STATE &check_state);
    HTTP_CODE parse_header_line(char *cur);
    LINE_STATUS parse_line(char *buffer, int &checked_index, int &read_index);
    HTTP_CODE parse_content(char *buffer, int &checked_index, CHECK_STATE &check_state, int &read_index, int &start_index);
};

#endif //WEBSERVER_HTTPPARSE_H
