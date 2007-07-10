//
// GreenSQL functions used to manage a number of proxy objects.
//
// Copyright (c) 2007 GreenSQL.NET <stremovsky@gmail.com>
// License: GPL v2 (http://www.gnu.org/licenses/gpl.html)
//

#include "greensql.hpp"
#include "proxymap.hpp"
#include "mysql/mysql_con.hpp"

#include<map>

static const char * const q_proxy = "SELECT proxyid, INET_NTOA(frontend_ip), "
            "frontend_port, backend_server, "
            "INET_NTOA(backend_ip), backend_port, dbtype "
            "FROM proxy";


	    
class GreenMySQL : public GreenSQL
{
public:
    virtual void Server_cb(int fd, short which, void * arg,
            Connection * conn, int sfd, int cfd)
    {
        logevent(NET_DEBUG, "MySQL Server_cb(), sfd=%d, cfd=%d\n", sfd, cfd);
        if(PrepareNewConn(fd, sfd, cfd))
        {
            conn = new MySQLConnection(iProxyId);
            GreenSQL::Server_cb(fd, which, arg, conn, sfd, cfd);
        }
        else
        {
            return;
        }
    }
};

static std::map<int, GreenMySQL * > proxies;

void wrap_Server(int fd, short which, void * arg)
{
    int proxy_id;
    
    proxy_id = (int) arg;
    logevent(NET_DEBUG, "[%d]wrap_Server\n", proxy_id);
    GreenSQLConfig * conf = GreenSQLConfig::getInstance();
    
    GreenMySQL * cls = proxies[proxy_id];
    
    if(cls != NULL)
        if (conf->bRunning == false)
            cls->Close();
        else
            cls->Server_cb(fd, which, arg, NULL, 0, 0);

}

void wrap_Proxy(int fd, short which, void * arg)
{
    int proxy_id;
    Connection * con = (Connection *) arg;
    proxy_id = con->iProxyId;

    logevent(NET_DEBUG, "[%d]frontend socket fired %d\n", proxy_id, fd);
    GreenSQLConfig * conf = GreenSQLConfig::getInstance();

    GreenMySQL * cls = proxies[proxy_id];
    
    if(cls != NULL)
        if (conf->bRunning == false)
            cls->Close();
        else
            cls->Proxy_cb(fd, which, arg);

}

void wrap_Backend(int fd, short which, void * arg)
{
    int proxy_id;
    Connection * con = (Connection *) arg;
    proxy_id = con->iProxyId;

    logevent(NET_DEBUG, "[%d]backend socket fired %d\n", proxy_id, fd);

    GreenSQLConfig * conf = GreenSQLConfig::getInstance();

    GreenMySQL * cls = proxies[proxy_id];

    if(cls != NULL)
        if (conf->bRunning == false)
            cls->Close();
        else
            cls->Backend_cb(fd, which, arg);

}


bool proxymap_init()
{
    proxymap_reload();
    if (proxies.size() == 0)
        return false;
    
    return true;
}

bool proxymap_close()
{
    std::map<int, GreenMySQL * >::iterator iter;
    GreenMySQL * cls = NULL;

    while (proxies.size() != 0)
    {
	iter = proxies.begin();
	cls = iter->second;
        cls->Close();
        delete cls;
	proxies.erase(iter);
    }
    return true;
}

bool proxymap_reload()
{
    GreenSQLConfig * conf = GreenSQLConfig::getInstance();
    MYSQL * dbConn = &conf->dbConn;
    //get records from the database
    MYSQL_RES *res; /* To be used to fetch information into */
    MYSQL_ROW row;
    GreenMySQL * cls;
    bool ret;

    std::string backendIP;
    int backendPort;
    std::string proxyIP;
    int proxyPort;
    int proxy_id = 99;

    
    backendIP = "127.0.0.1";
    backendPort = 3306;

    proxyIP = "127.0.0.1";
    proxyPort = 3305;

    //        GreenMySQL * cls = new GreenMySQL();
    //        cls->ProxyInit(proxy_id, proxyIP, proxyPort,
    //                        backendIP, backendPort);
    //        proxies[proxy_id] = cls;
    //return true;
    
    /* read new urls from the database */
    if( mysql_query(dbConn, q_proxy) )
    {
        /* Make query */
        logevent(STORAGE,"%s\n",mysql_error(dbConn));
        return false;
    }

    /* Download result from server */
    res=mysql_store_result(dbConn);
    if (res == NULL)
    {
	logevent(STORAGE, "Records Found: 0, error:%s\n", mysql_error(dbConn));
        return false;
    }
    //my_ulonglong mysql_num_rows(MYSQL_RES *result)
    logevent(STORAGE, "Records Found: %lld\n", mysql_num_rows(res) );

    /* Get a row from the results */
    while ((row=mysql_fetch_row(res)))
    {
        proxy_id = atoi(row[0]);
        
        proxyIP = row[1];
        proxyPort = atoi(row[2]);
        // backendServer = row[3]
        backendIP = row[4];
        backendPort = atoi(row[5]);
        //new object
        
	std::map<int, GreenMySQL * >::iterator itr;
	itr = proxies.find(proxy_id);
	
        if (itr == proxies.end())
	{
            cls = new GreenMySQL();

            bool ret = cls->ProxyInit(proxy_id, proxyIP, proxyPort, 
			    backendIP, backendPort);
	    if (ret == false)
	    {
                // failed to init proxy
		proxymap_set_db_status(proxy_id, 2);
		cls->Close();
		delete cls;
	    } else {
		proxymap_set_db_status(proxy_id, 1);
                proxies[proxy_id] = cls;
	    }
            continue; 
        }
	
        cls = itr->second;

        // check if db settings has been changed
        if (cls->sProxyIP != proxyIP ||
            cls->iProxyPort != proxyPort)
	{
            ret = cls->ProxyReInit(proxy_id, proxyIP, proxyPort,
                                   backendIP, backendPort);
            if (ret == false)
            {
                proxymap_set_db_status(proxy_id, 2);
            } else {
                proxymap_set_db_status(proxy_id, 1);
            }
	    continue;
        } else if (cls->sBackendIP != backendIP ||
                   cls->iBackendPort != backendPort)
        {
            // may be backend ip has been changed
            cls->sBackendIP = backendIP;
            cls->iBackendPort = backendPort;
	}

        if (cls->ServerInitialized() == true)
        {
            proxymap_set_db_status(proxy_id, 1);
        } else {
            // try to reinitialize server socket
            ret = cls->ProxyReInit(proxy_id, proxyIP, proxyPort,
                               backendIP, backendPort);
            if (ret == false)
            {
                proxymap_set_db_status(proxy_id, 2);
            } else {
                proxymap_set_db_status(proxy_id, 1);
	    }
	}
        
    }
    /* Release memory used to store results. */
    mysql_free_result(res);
 
    return true;
}

/*
 *  proxy status
 *  0 - reload
 *  1 - good
 *  2 - failed to load
 *
 */

bool proxymap_set_db_status(unsigned int proxy_id, int status )
{
    GreenSQLConfig * conf = GreenSQLConfig::getInstance();
    MYSQL * dbConn = &conf->dbConn;
    char query[1024];

    snprintf(query, sizeof(query), 
		    "UPDATE proxy set status=%d where proxyid=%d",
		    status, proxy_id);

    if( mysql_query(dbConn, query) )
    {
        /* Make query */
        logevent(STORAGE,"%s\n",mysql_error(dbConn));
        return false;
    }

    return true;
}
