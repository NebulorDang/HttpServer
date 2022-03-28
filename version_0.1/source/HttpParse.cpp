//
// Created by NebulorDang on 2021/11/25.
// HTTP解析器
//

#include "HttpParse.h"

HttpParse::HTTP_CODE HttpParse::parse_request_line(char *cur, HttpParse::CHECK_STATE &check_state) {
    string str = cur;
    smatch str_match;
    regex pattern("^([^ ]+)[ ]+([^ ]+)[ ]+HTTP/([^ ]+)$", regex_constants::icase);//ignore case of "HTTP"
    string method, path, version;
    if (regex_match(str, str_match, pattern)) {
        method = str_match[1];
        path = str_match[2];
        version = str_match[3];
    } else {
        return BAD_REQUEST;
    }

    if (strcasecmp(method.c_str(), "GET") == 0) {
        cout << "The request method is GET" << endl;
    } else {
        return BAD_REQUEST;
    }

    if (version != "1.1") {
        return BAD_REQUEST;
    }

    if (strncasecmp(path.c_str(), "http://", 7) == 0) {
        path = path.substr(7);
    }
    if (path.find('/') == string::npos) {
        return BAD_REQUEST;
    } else {
        path = path.substr(path.find('/'));
        cout << "The request url is: " << path << endl;
    }

    check_state = CHECK_STATE_HEADER;
    return NO_REQUEST;
}

HttpParse::HTTP_CODE HttpParse::parse_header_line(char *cur) {
    if (cur && *cur == '\0') {
        return GET_REQUEST;
    } else if (strncasecmp(cur, "Host:", 5) == 0) {
        cur += 5;
        cur += strspn(cur, " \t");//drop the space of '\t'
        cout << "The request host is: " << cur << endl;
    } else {
        cout << "Do not support parse this header field " << cur << endl;
    }
    return NO_REQUEST;;
}

HttpParse::LINE_STATUS HttpParse::parse_line(char *buffer, int &checked_index, int &read_index) {
    for (; checked_index < read_index; checked_index++) {
        char ch = buffer[checked_index];
        if (ch == '\r') {
            if (checked_index + 1 == read_index) {
                return LINE_OPEN;
            } else if (buffer[checked_index + 1] == '\n') {
                buffer[checked_index++] = '\0';
                buffer[checked_index++] = '\0';
                return LINE_OK;
            }
        } else if (ch == '\n') {
            if (checked_index > 1 && buffer[checked_index - 1] == '\r') {
                buffer[checked_index - 1] = '\0';
                buffer[checked_index++] = '\0';
                return LINE_OK;
            }
            return LINE_BAD;
        }
    }
    return LINE_OPEN;
}

HttpParse::HTTP_CODE
HttpParse::parse_content(char *buffer, int &checked_index, HttpParse::CHECK_STATE &check_state, int &read_index,
                         int &start_index) {
    LINE_STATUS line_status;
    HTTP_CODE ret_code;
    while ((line_status = parse_line(buffer, checked_index, read_index)) == LINE_OK) {
        char *cur = buffer + start_index;
        start_index = checked_index;
        switch (check_state) {
            case CHECK_STATE_REQUESTLINE: {
                ret_code = parse_request_line(cur, check_state);
                if (ret_code == BAD_REQUEST) {
                    return BAD_REQUEST;
                }
                break;
            }
            case CHECK_STATE_HEADER: {
                ret_code = parse_header_line(cur);
                if (ret_code == BAD_REQUEST) {
                    return BAD_REQUEST;
                } else if (ret_code == GET_REQUEST) {
                    return GET_REQUEST;
                }
                break;
            }
            default: {
                return INTERNAL_ERROR;
            }
        }
    }
    if (line_status == LINE_OPEN) {
        return NO_REQUEST;
    } else {
        return BAD_REQUEST;
    }
}




