/*
 * Asterisk -- An open source telephony toolkit.
 *
 * Copyright (C) 1999 - 2006, Digium, Inc.
 *
 * Mark Spencer <markster@digium.com>
 *
 * See http://www.asterisk.org for more information about
 * the Asterisk project. Please do not directly contact
 * any of the maintainers of this project for assistance;
 * the project provides a web site, mailing lists and IRC
 * channels for your use.
 *
 * This program is free software, distributed under the terms of
 * the GNU General Public License Version 2. See the LICENSE file
 * at the top of the source tree.
 */

/*! \file
 *
 * \brief GeoIP Based ACL 
 *
 * \author Matthew M. Gamble <mgamble@primustel.ca>
 */

#include "iaxproxy.h"

ASTERISK_FILE_VERSION(__FILE__, "$Revision: 00001 $")

#include "iaxproxy/logger.h"
#include "iaxproxy/network.h"
#include "iaxproxy/acl.h"
#include "iaxproxy/geoacl.h"
#include <GeoIP.h>


int geo_acl_check(struct sockaddr_in *sin, char *allowed_countries, char *peername)
{
	GeoIP * gi;
        const char * returnedCountry;
        gi = GeoIP_open("/opt/GeoIP/share/GeoIP/GeoIP.dat", GEOIP_STANDARD | GEOIP_CHECK_CACHE);
	int retValue = 0;
	int j, i=0; // used to iterate through array
	char lookup[255];
    	char *token[86];
        if (gi == NULL) {
                ast_log(LOG_WARNING, "Failed to open GeoIP Lookup database - ignoring GeoACL");
        } else {
		if((allowed_countries == NULL) || (strlen(allowed_countries) == 0) ) {
			ast_log(LOG_NOTICE, "GeoACL - Peer %s does not have allowed/denied countries defined, ignoring check\n", peername);
		} else {
                	ast_log(LOG_DEBUG, "Using %s for allowed geo acl list\n", allowed_countries);
			returnedCountry = GeoIP_country_code_by_addr(gi,ast_inet_ntoa(sin->sin_addr));
	                if (returnedCountry == NULL) {
        	                ast_log(LOG_WARNING, "GeoACL Lookup failed for '%s' - ignoring GeoACL\n", ast_inet_ntoa(sin->sin_addr));
             	   	} else {
				// Now we default to deny since we know what they want and where the IP is from 
				strcpy(lookup, allowed_countries);
				retValue = 1;			
				//ast_log(LOG_NOTICE, "GeoACL - value of allowed_countries is %s", allowed_countries);						
				token[0] = strtok (lookup,",");
  				while(token[i]!= NULL) {   //ensure a pointer was found
        				i++;
        				token[i] = strtok(NULL, ","); //continue to tokenize the string
    				}
  				for(j = 0; j <= i-1; j++) {
			//		ast_log(LOG_DEBUG, "Checking token %s", token[j]);
					if (strncmp(returnedCountry, token[j], 2) == 0) {
						retValue = 0;
					}
				}
				if (retValue == 1)  {
					ast_log(LOG_NOTICE, "GeoACL - peer '%s' failed ACL check.  Allowed from %s but connected from %s\n", peername, allowed_countries, returnedCountry);
				} else {
                	       		 ast_log(LOG_NOTICE, "GeoACL passed for peer '%s' - connected from '%s'\n", peername, returnedCountry);
                		}
			}
		}
        }
        // Free resources
        GeoIP_delete(gi);
	return retValue;
}

