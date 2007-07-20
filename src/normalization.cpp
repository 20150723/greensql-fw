//
// GreenSQL SQL query normalization functions.
//
// Copyright (c) 2007 GreenSQL.NET <stremovsky@gmail.com>
// License: GPL v2 (http://www.gnu.org/licenses/gpl.html)
//

#include <string>
#include "normalization.hpp"

static bool fixNegativeNumbers(std::string & query);
static bool removeNumbers(std::string & query);
static bool removeHashComment(std::string & query);
static bool removeDashComment(std::string & query);
static bool removeCppComment(std::string & query);
static int lookForChar(std::string & query, int start, char delimeter);

/*
 * The following function is used to perform the following changes:
 * quoted text is changed to '?'
 * numbers are changed to '?'
 * remove comment
 * remove double spaces
 * the following sequence (X=?) is changed to "X = ?"
 */

bool normalizeQuery(std::string & query)
{
    removeQuotedText(query);
    removeComments(query);
    removeNumbers(query);
    removeSpaces(query);
    fixNegativeNumbers(query);

    return true;	
}

bool removeComments(std::string & query)
{
    removeCppComment(query);
    removeHashComment(query);
    removeDashComment(query);
    return true;
}

/*
 * The following function removes sequence of spaces.
 * The following chars are considered spaces: \r \n \t " "
 *
 */
bool removeSpaces(std::string & query)
{
    unsigned int i;
    unsigned int j;

    for (i = 0; i < query.size(); i++)
    {
        if (query[i] == '\r' || query[i] == '\n' || 
            query[i] == '\t' || query[i] == ' ')
        {
            for (j=i+1; j < query.size() &&
                 (query[j] == '\r' || query[j] == '\n' ||
                 query[j] == '\t' || query[j] == ' '); j++)
            {
                    ;
            }
            query[i] = ' ';
            if (j != i+1)
            {
                query.erase(i+1, j-i-1);
            }
        }
    }
    // remove last space
    j = query.size()-1;
    if (query[j] == '\r' || query[j] == '\n' ||
        query[j] == '\t' || query[j] == ' ')
    {
        query.erase(j,query.size());
    }
    return true;
}

/*
 * The following function is used to remove c++ style comment.
 * It undestands the following comment: \* abc *\
 */
static bool removeCppComment(std::string & query)
{
    unsigned int i;
    unsigned int j;
    
    for (i = 0; i< query.size()-1; i++)
    {
	// check for the start of the comment block
        if (query[i] == '/' && query[i+1] == '*')
	{
	    for(j=i+2; j < query.size()-1 && 
	        query[j] != '*' && query[j+1] != '/'; j++)
	    {
		    ;
	    }
	    //check if found end of comment
	    if (query[j] == '*' && query[j+1] == '/')
	    {
                if (j != i+2)
		{
                     query[i+2] = ' ';
		     query.erase(i+3, j-i-3);
		} else {
                     //nothing to do
		}
	    } else {
                query.erase(i+2,j-i+2);
	    }
	    // check if found end of comment block
	    //if (j != i+2)
	    //{
            //    query[i] = ' ';
	    //    query.erase(i+1, j-i+1);
	    //}
	}
    }
    return true;
}

/*
 * The following function is used to remove comment starting from
 * the hash symbol till the end of the line.
 */

static bool removeHashComment(std::string & query)
{
    unsigned int i;
    unsigned int j;

    for (i = 0; i< query.size(); i++)
    {
        // check for the start of the comment block
        if (query[i] == '#')
        {
            for (j=i+1; j < query.size() &&
	        (query[j] != '\r' && query[j] != '\n'); j++)
            {
		    ;
	    }
            for (; j < query.size() &&
                (query[j] == '\r' || query[j] == '\n'); j++)
            {
                    ;
            }
            //query[i] = ' ';
            query.erase(i+1, j-i-1);
        }
    }
    return true;
}

/*
 * The following function is used to remove comment starting from
 * the 2 dahses (--) till the end of the line.
 */

static bool removeDashComment(std::string & query)
{
    unsigned int i;
    unsigned int j;

    for (i = 0; i< query.size()-1; i++)
    {
        // check for the start of the comment block
        if (query[i] == '-' && query[i+1] == '-')
        {
            for (j=i+2; j < query.size() &&
                (query[j] != '\r' && query[j] != '\n'); j++)
            {
                    ;
            }
            for (; j < query.size() &&
                (query[j] == '\r' || query[j] == '\n'); j++)
            {
                    ;
            }
            //query[i] = ' ';
	    if (j != i+1)
            {
                query.erase(i+2, j-i-2);
	    }
        }
    }
    return true;
}

static bool removeNumbers(std::string & query)
{
    unsigned int i;
    unsigned int j;
    
    //the following chars can precede number " ,.(" 
    for (i = 1; i < query.size(); i++)
    {
        if ((query[i-1] == '.' || query[i-1] == ',' || 
             query[i-1] == '(' || query[i-1] == ' ' ||
	     query[i-1] == '=')	&&
	    query[i] >= '0' && query[i] <= '9')
	{
	    for (j=i+1; j < query.size() && query[j] >= '0' 
			    && query[j] <= '9'; j++)
	    {
		    ;
	    }
            query[i] = '?';
	    if (j != i+1)
	    {
	        query.erase(i+1, j-i-1);
	    }
	}
    }
    return true;
}

bool removeQuotedText(std::string & query)
{
    unsigned int i;
    int j;
    bool quoted = false;

    for (i = 0; i < query.size(); i++)
    {
        if (query[i] == '\'' && quoted == false)
	{
	    //look for the end.
	    j = lookForChar(query, i, '\'');
            query[i] = '?';
	    query.erase(i+1, j-i);
	}
	else if (query[i] == '"' && quoted == false)
	{
	    j = lookForChar(query, i, '"');
	    query[i] = '?';
	    query.erase(i+1,j-i);
	} else if (query[i] == '\\' && quoted == false)
	{
	    quoted = true;
	} else
	{
            quoted = false;
	}
    }
    return true;

}

static bool fixNegativeNumbers(std::string & query)
{
    // insert into ... values(-1,xx,x,-1)
    // select -1,xx, -1
    // select xxxxxxxxxxxx from xxx where -1=xx or -1=xx and -1=xx not -1==xx
    std::string last_token;
    size_t next = 0;
    size_t prev = 0;

    while ( (next = query.find_first_of(" ,(", prev)) !=
		    std::string::npos)
    {
	if (next + 1 > query.size())
            return true;

        last_token = query.substr(prev, next+1-prev);
	if ( next > query.size()-2)
            return true;
	if ( query[next+1] == '-' && query[next+2] == '?')
	{
            if (last_token == "select " || last_token == "where " ||
	        last_token == "and "    || last_token == "or "    || 
		last_token == "not "    ||
		last_token[last_token.size()-1] == '(' ||
		last_token[last_token.size()-1] == ',')
	    {
                query.erase(next+1,1);
	    }
	}
	if ( next > query.size()-3)
            return true;
	if ( query[next+1] == '-' && query[next+2] == ' ' && 
			query[next+3] == '?')
	{
            if (last_token == "select " || last_token == "where " ||
                last_token == "and "    || last_token == "or "    ||
                last_token == "not "    ||
                last_token[last_token.size()-1] == '(' ||
                last_token[last_token.size()-1] == ',')
            {
                // remove minus together with space
                query.erase(next+1,2);
            }

	}
        prev = next+1;
    }
    return true;
}

static int lookForChar(std::string & query, int start, char delimeter)
{
    unsigned int j = 0;
    bool quoted = false;

    for (j = start+1; j < query.size(); j++)
    {
        if (query[j] == delimeter && quoted == false)
	{
		return j;
	} else if (query[j] == '\\' && quoted == false)
	{
            quoted = true;
	} else
	{
            quoted = false;
	}
    }
    return j;
}

