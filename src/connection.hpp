//
// GreenSQL DB Connection class header.
//
// Copyright (c) 2007 GreenSQL.NET <stremovsky@gmail.com>
// License: GPL v2 (http://www.gnu.org/licenses/gpl.html)
//

#ifndef GREEN_SQL_CONNECTION_HPP
#define GREEN_SQL_CONNECTION_HPP

#include <string>

#ifdef WIN32
#include <winsock2.h>
#endif

#include <event.h>
#include "buffer.hpp"
#include "dbpermobj.hpp"
#include "patterns.hpp"     // for SQLPatterns
#include <string> // for std::string

class Connection
{
public:
    Connection(int proxy_id);
    virtual ~Connection() {};
    bool close();
    bool check_query(std::string & query);
    struct event proxy_event;
    struct event client_event;
    Buffer request_in;
    Buffer request_out;
    Buffer response_in;    
    Buffer response_out;
    bool first_request;
    virtual bool checkBlacklist(std::string & query, std::string & reason) = 0;
    virtual bool parseRequest(std::string & req, bool & hasResponse ) = 0;
    virtual bool parseResponse(std::string & response) = 0;
    virtual bool blockResponse(std::string & response) = 0;
    virtual SQLPatterns * getSQLPatterns() = 0;
    int iProxyId;    // the simplest method to transfer proxy id
    std::string db_name;
    std::string db_new_name;
    std::string db_user;
    DBPermObj * db;
private:
    unsigned int calculateRisk(std::string & query, std::string & pattern,
                           std::string &reason);
    
};

#endif

