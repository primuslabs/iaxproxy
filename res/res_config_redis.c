/*
 * Asterisk -- A telephony toolkit for Linux.
 *
 * res_config_redis.c <redis plugin for RealTime configuration engine>
 *
 * v0.3	  - (11-07-07) - Corrected memory leak issues found in stress testing
 * v0.2	  - (11-06-07) - Version now passes basic testing with IAX and SIP Peers
 * v0.1   - (11-05-07) - Created inital version based on res_config_mysql.c

TODO:

0) Voicemail support / testing (it's the last piece we need)
1) Database support (because it's dead simple to add, and the other res_config drivers expect it
2) Config file support
3) Figure out when (if ever) the multi-query lookup function is called so we can fix it
4) Implement reload / help / etc
5) Pooled connections?
6) Test, test, test


*/
#include <iaxproxy.h>

#include <iaxproxy/channel.h>
#include <iaxproxy/logger.h>
#include <iaxproxy/config.h>
#include <iaxproxy/module.h>
#include <iaxproxy/lock.h>
#include <iaxproxy/options.h>
#include <iaxproxy/cli.h>
#include <iaxproxy/utils.h>
#include <iaxproxy/threadstorage.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <stdio.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include "hiredis.h"

#define RES_CONFIG_REDIS_CONF "res_redis.conf"
#define	READHANDLE	0
#define	WRITEHANDLE	1

#define ESCAPE_STRING(buf, var) \
	do { \
		if ((valsz = strlen(var)) * 2 + 1 > ast_str_size(buf)) { \
			ast_str_make_space(&(buf), valsz * 2 + 1); \
		} \
	} while (0)

AST_THREADSTORAGE(sql_buf);
AST_THREADSTORAGE(sql2_buf);
AST_THREADSTORAGE(find_buf);
AST_THREADSTORAGE(scratch_buf);
AST_THREADSTORAGE(modify_buf);
AST_THREADSTORAGE(modify2_buf);
AST_THREADSTORAGE(modify3_buf);

enum requirements { RQ_WARN, RQ_CREATECLOSE, RQ_CREATECHAR };

struct redis_conn {
	AST_RWLIST_ENTRY(redis_conn) list;
	ast_mutex_t	lock;
	redisContext c;	
	char        host[50];
	int         port;
	int         connected;
	time_t      connect_time;
};

struct columns {
	char *name;
	char *type;
	char *dflt;
	char null;
	int len;
	AST_LIST_ENTRY(columns) list;
};

struct tables {
	ast_mutex_t lock;
	AST_LIST_HEAD_NOLOCK(mysql_columns, columns) columns;
	AST_LIST_ENTRY(tables) list;
	char name[0];
};

static AST_LIST_HEAD_STATIC(mysql_tables, tables);
static AST_RWLIST_HEAD_STATIC(databases, mysql_conn);

static int parse_config(int reload);
static int redis_reconnect(struct redis_conn *conn);
static char *handle_cli_realtime_redis_status(struct ast_cli_entry *e, int cmd, struct ast_cli_args *a);
static int load_redis_config(struct ast_config *config, const char *category, struct redis_conn *conn);
static int require_redis(const char *database, const char *tablename, va_list ap);


static struct ast_cli_entry cli_realtime_redis_status[] = {
        AST_CLI_DEFINE(handle_cli_realtime_redis_status, "Shows connection information for the Redis RealTime driver"),
};


#define release_database(a)	ast_mutex_unlock(&(a)->lock)



static struct ast_variable *realtime_redis(const char *database, const char *table, va_list ap)
{
	struct redisContext *c;
	redisReply *reply;
	redisReply *reply2;
	int numFields, i, valsz;
	struct ast_str *sql = ast_str_thread_get(&sql_buf, 16);
	struct ast_str *buf = ast_str_thread_get(&scratch_buf, 16);
	char *stringp;
	char *chunk;
	char *op;
	const char *newparam, *newval;
	struct ast_variable *var=NULL, *prev=NULL;

	struct timeval timeout = { 1, 500000 }; // 1.5 seconds
	if (!table) {
		ast_log(LOG_WARNING, "Redis RealTime: No table specified - this is required for the key search\n");
//		redisFree(c);
		return NULL;
	}
	// TODO - Put this all in a cfg file
	c = redisConnectWithTimeout((char*)"127.0.0.1", 6379, timeout);
        if (c->err) {
                ast_log(LOG_NOTICE,"Could not connect to RedisDB - realtime lookups will fail.  Connection error: %s\n", c->errstr);
                redisFree(c);
                return NULL;
        }
	/* Get the first parameter and first value in our list of passed paramater/value pairs */

	while ((newparam = va_arg(ap, const char *))) {
                        newval = va_arg(ap, const char *);
		if (!newparam || !newval)  {
			ast_log(LOG_WARNING, "MySQL RealTime: Realtime retrieval requires at least 1 parameter and 1 value to search on.\n");
			redisFree(c);
			return NULL;
		}

		/* Create the first part of the query using the first parameter/value pairs we just extracted
		   If there is only 1 set, then we have our query. Otherwise, loop thru the list and concat */
		if (ast_strlen_zero(newval)) {
			// No query, no query
			ast_log(LOG_DEBUG, "Redis Realtime - null value passed, skipping\n");
			continue;
		}
		ESCAPE_STRING(buf, newval);

		reply = redisCommand(c,"HKEYS %s:%s", table,newval);
		ast_log(LOG_DEBUG,"MG: newparam='%s', newval='%s'\n", newparam, newval);
		ast_log(LOG_DEBUG,"Redis Realtime Query A: HKEYS %s:%s\n", table,newval);
		if (reply == NULL) {
	
			// Let's look at the rest of the keys we were passed then.....
			freeReplyObject(reply);
			ast_log(LOG_NOTICE, "Redis RealTime: Failed to query database\n");
			freeReplyObject(reply);
			redisFree(c);
       		        return NULL;
		}
		if (reply->type == REDIS_REPLY_NIL) {
			ast_log(LOG_DEBUG, "Redis Realtime: NIL reply");
                }

		if (reply->type == REDIS_REPLY_ARRAY && reply->elements > 0) {
	        	ast_log(LOG_DEBUG,"Redis Realtime Query returned array, looping\n");
		        var = NULL;
	                for (i=0; i < reply->elements; i++) {
				//ast_debug(LOG_DEBUG, "Redis Query - HGET %s:%s %s", table, newval, reply->element[i]->str);	
	       	                 reply2	= redisCommand(c,"HGET %s:%s %s", table, newval, reply->element[i]->str);
				ast_debug(2, "Redis Realtime attribute_name: %s value: %s\n", reply->element[i]->str, reply2->str);
                	        for (stringp = ast_strdupa(reply2->str), chunk = strsep(&stringp, ";"); chunk; chunk = strsep(&stringp, ";")) {
                        	       if (prev) {
                                        if ((prev->next = ast_variable_new(reply->element[i]->str, chunk, ""))) {
                                                        prev = prev->next;
                                                }
                                        } else {
                                                prev = var = ast_variable_new(reply->element[i]->str, chunk, "");
                                        }
                        	}
				freeReplyObject(reply2);
			}
			freeReplyObject(reply);
        	} else {
              	  ast_debug(1, "Redis RealTime: Could not find any rows in table %s.\n", table);
       			freeReplyObject(reply);
	 	}
	}
	redisFree(c);
	return var;
}

static struct ast_config *realtime_multi_redis(const char *database, const char *table, va_list ap)
{
	struct redisContext *c;
        redisReply *reply;
        redisReply *reply2;
	struct ast_str *buf = ast_str_thread_get(&scratch_buf, 16);	
	int numFields, i, valsz;
	const char *initfield = NULL;
	char *stringp;
	char *chunk;
	char *op;
	const char *newparam, *newval;
	struct ast_variable *var = NULL;
	struct ast_config *cfg = NULL;
	struct ast_category *cat = NULL;


	/* Get the first parameter and first value in our list of passed paramater/value pairs */
	newparam = va_arg(ap, const char *);
	newval = va_arg(ap, const char *);
	if (!newparam || !newval)  {
		ast_log(LOG_WARNING, "MySQL RealTime: Realtime retrieval requires at least 1 parameter and 1 value to search on.\n");
		ast_config_destroy(cfg);
		return NULL;
}

	initfield = ast_strdupa(newparam);
	if (initfield && (op = strchr(initfield, ' '))) {
		*op = '\0';
	}

	
        /* Must connect to the server before anything else, as the escape function requires the mysql handle. */
        // Fix to use option from conf file (MG)
        struct timeval timeout = { 1, 500000 }; // 1.5 seconds
	c = redisConnectWithTimeout((char*)"127.0.0.1", 6379, timeout);
        if (c->err) {
                ast_log(LOG_NOTICE,"Could not connect to RedisDB.  Connection error: %s\n", c->errstr);
                redisFree(c);
                return NULL;
        }

        /* Create the first part of the query using the first parameter/value pairs we just extracted
           If there is only 1 set, then we have our query. Otherwise, loop thru the list and concat */
	ESCAPE_STRING(buf, newval);
        reply = redisCommand(c,"HKEYS %s:%s", table,ast_str_buffer(buf));
        ast_log(LOG_DEBUG,"Redis Realtime Query: HGET %s:%s\n", table,ast_str_buffer(buf));
        if (reply->type == REDIS_ERR) {
                ast_log(LOG_WARNING, "Redis RealTime: Failed to query database\n");
                redisFree(c);
                return NULL;
        }
/*
	if (!strchr(newparam, ' '))
		op = " =";
	else
		op = "";

	ESCAPE_STRING(buf, newval);
	ast_str_set(&sql, 0, "SELECT * FROM %s WHERE %s%s '%s'", table, newparam, op, ast_str_buffer(buf));
	while ((newparam = va_arg(ap, const char *))) {
		newval = va_arg(ap, const char *);
		if (!strchr(newparam, ' ')) op = " ="; else op = "";
		ESCAPE_STRING(buf, newval);
		ast_str_append(&sql, 0, " AND %s%s '%s'", newparam, op, ast_str_buffer(buf));
	}

	if (initfield) {
		ast_str_append(&sql, 0, " ORDER BY %s", initfield);
	}

	va_end(ap);

	ast_debug(1, "MySQL RealTime: Retrieve SQL: %s\n", ast_str_buffer(sql));
*/
	/* Execution. */
	/* Go over the result */

	if (reply->type == REDIS_REPLY_ARRAY) {
		var = NULL;
                cat = ast_category_new("", "", -1);
                if (!cat) {
                        ast_log(LOG_WARNING, "Out of memory!\n");
                	return NULL;
		}
                for (i=0; i < reply->elements; i++) {
			reply2= redisCommand(c,"HGET %s:%s %s", table, ast_str_buffer(buf), reply->element[i]->str);
			ast_debug(2, "Redis Realtime attribute_name: %s value: %s\n", reply->element[i]->str, reply2->str);
			if (ast_strlen_zero(reply2->str)) {
				continue;
			}	
			for (stringp = ast_strdupa(reply2->str), chunk = strsep(&stringp, ";"); chunk; chunk = strsep(&stringp, ";")) {
				if (chunk && !ast_strlen_zero(ast_strip(chunk))) {
					if (initfield && !strcmp(initfield, reply->element[i]->str)) {
						ast_category_rename(cat, chunk);
					}
					var = ast_variable_new(reply->element[i]->str, chunk, "");
					ast_variable_append(cat, var);
				}
			ast_category_append(cfg, cat);
			}
		}
		freeReplyObject(reply);
		freeReplyObject(reply2);
	} else {
		ast_debug(1, "Redis RealTime2: Could not find any rows in table %s.\n", table);
		freeReplyObject(reply);
	}
	redisFree(c);
	return cfg;
}

static int update_redis(const char *database, const char *tablename, const char *keyfield, const char *lookup, va_list ap)
{
	ast_log(LOG_DEBUG,"update redis called but not supported yet - returning");
	return 1;
}


static struct ast_config *config_redis(const char *database, const char *table, const char *file, struct ast_config *cfg, struct ast_flags config_flags, const char *unused, const char *who_asked)
{

		ast_log(LOG_WARNING, "Redis RealTime: Cannot configure using this module\n");
		return NULL;

}

#define PICK_WHICH_ALTER_ACTION(stringtype) \
	if (table->database->requirements == RQ_WARN) {                                                                       \
		ast_log(LOG_WARNING, "Realtime table %s@%s: column '%s' may not be large enough for "            \
			"the required data length: %d (detected stringtype)\n",                                      \
			tablename, database, column->name, size);                                                    \
		res = -1;                                                                                        \
	} else if (table->database->requirements == RQ_CREATECLOSE && modify_mysql(database, tablename, column, type, size) == 0) {     \
		table_altered = 1;                                                                               \
	} else if (table->database->requirements == RQ_CREATECHAR && modify_mysql(database, tablename, column, RQ_CHAR, size) == 0) {   \
		table_altered = 1;                                                                               \
	} else {                                                                                             \
		res = -1;                                                                                        \
	}

static int unload_redis(void) {
        /* To do */
        return 0;
}

static struct ast_config_engine redis_engine = {
	.name = "redis",
	.load_func = config_redis,
	.realtime_func = realtime_redis,
	.realtime_multi_func = realtime_multi_redis,
	.unload_func = unload_redis,
};

static int load_module(void)
{
	parse_config(0);

	ast_config_engine_register(&redis_engine);
	if (option_verbose > 1)
		ast_verbose(VERBOSE_PREFIX_2 "Redis RealTime driver loaded.\n");
	ast_cli_register_multiple(cli_realtime_redis_status, sizeof(cli_realtime_redis_status) / sizeof(struct ast_cli_entry));
	return 0;
}

static int unload_module(void)
{
	struct mysql_conn *cur;
	struct tables *table;

	ast_cli_unregister_multiple(cli_realtime_redis_status, sizeof(cli_realtime_redis_status) / sizeof(struct ast_cli_entry));
	ast_config_engine_deregister(&redis_engine);
	if (option_verbose > 1)
		ast_verbose(VERBOSE_PREFIX_2 "Redis RealTime unloaded.\n");

	ast_module_user_hangup_all();

	usleep(1);

	/* Destroy cached table info */

	return 0;
}

static int reload(void)
{
	parse_config(1);

	if (option_verbose > 1) {
		ast_verb(2, "Redis Realtime Reloaded - Okay I'm Reloaded!!!!\n");
	}

	return 0;
}

static int parse_config(int reload)
{
	struct ast_config *config = NULL;
	struct ast_flags config_flags = { reload ? CONFIG_FLAG_FILEUNCHANGED : 0 };
	const char *catg;
	struct mysql_conn *cur;

	if ((config = ast_config_load(RES_CONFIG_REDIS_CONF, config_flags)) == CONFIG_STATUS_FILEMISSING) {
		return 0;
	} else if (config == CONFIG_STATUS_FILEUNCHANGED) {
		return 0;
	} else if (config == CONFIG_STATUS_FILEINVALID) {
		ast_log(LOG_ERROR, "Not %sloading " RES_CONFIG_REDIS_CONF "\n", reload ? "re" : "");
	}

	return 0;
}

static int load_mysql_config(struct ast_config *config, const char *category, struct mysql_conn *conn)
{
	const char *s;

	return 1;
}


static char *handle_cli_realtime_redis_status(struct ast_cli_entry *e, int cmd, struct ast_cli_args *a)
{
	char status[256], status2[100] = "", type[20];
	char *ret = NULL;
	int ctime = 0, found = 0;
	struct mysql_conn *cur;
	int l, which;

	switch (cmd) {
	case CLI_INIT:
		e->command = "realtime redis status";
		e->usage =
			"Usage: realtime redis status\n"
			"       Shows connection information for the Redis RealTime driver\n";
		return NULL;
	case CLI_GENERATE:
		return CLI_SUCCESS;
	}
}

AST_MODULE_INFO(ASTERISK_GPL_KEY, AST_MODFLAG_DEFAULT, "Redis RealTime Configuration Driver",
		.load = load_module,
		.unload = unload_module,
		.reload = reload,
		);

