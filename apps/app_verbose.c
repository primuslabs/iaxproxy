/*
 * Asterisk -- An open source telephony toolkit.
 *
 * Copyright (c) 2004 - 2005 Tilghman Lesher.  All rights reserved.
 *
 * Tilghman Lesher <app_verbose_v001@the-tilghman.com>
 *
 * This code is released by the author with no restrictions on usage.
 *
 * See http://www.iaxproxy.org for more information about
 * the Asterisk project. Please do not directly contact
 * any of the maintainers of this project for assistance;
 * the project provides a web site, mailing lists and IRC
 * channels for your use.
 *
 */

/*! \file
 *
 * \brief Verbose logging application
 *
 * \author Tilghman Lesher <app_verbose_v001@the-tilghman.com>
 *
 * \ingroup applications
 */

#include "iaxproxy.h"

ASTERISK_FILE_VERSION(__FILE__, "$Revision: 301176 $")

#include "iaxproxy/module.h"
#include "iaxproxy/app.h"
#include "iaxproxy/channel.h"

static char *app_verbose = "Verbose";
static char *app_log = "Log";

/*** DOCUMENTATION
	<application name="Verbose" language="en_US">
 		<synopsis>
			Send arbitrary text to verbose output.
		</synopsis>
		<syntax>
			<parameter name="level">
				<para>Must be an integer value.  If not specified, defaults to 0.</para>
			</parameter>
			<parameter name="message" required="true">
				<para>Output text message.</para>
			</parameter>
		</syntax>
		<description>
			<para>Sends an arbitrary text message to verbose output.</para>
		</description>
	</application>
	<application name="Log" language="en_US">
		<synopsis>
			Send arbitrary text to a selected log level.
		</synopsis>
		<syntax>
			<parameter name="level" required="true">
				<para>Level must be one of <literal>ERROR</literal>, <literal>WARNING</literal>, <literal>NOTICE</literal>,
				<literal>DEBUG</literal>, <literal>VERBOSE</literal> or <literal>DTMF</literal>.</para>	
			</parameter>
			<parameter name="message" required="true">
				<para>Output text message.</para>
			</parameter>
		</syntax>
		<description>
			<para>Sends an arbitrary text message to a selected log level.</para>
		</description>
	</application>
 ***/


static int verbose_exec(struct ast_channel *chan, void *data)
{
	int vsize;
	char *parse;
	AST_DECLARE_APP_ARGS(args,
		AST_APP_ARG(level);
		AST_APP_ARG(msg);
	);

	if (ast_strlen_zero(data)) {
		return 0;
	}

	parse = ast_strdupa(data);
	AST_STANDARD_APP_ARGS(args, parse);
	if (args.argc == 1) {
		args.msg = args.level;
		args.level = "0";
	}

	if (sscanf(args.level, "%30d", &vsize) != 1) {
		vsize = 0;
		ast_log(LOG_WARNING, "'%s' is not a verboser number\n", args.level);
	}
	if (option_verbose >= vsize) {
		switch (vsize) {
		case 0:
			ast_verbose("%s\n", args.msg);
			break;
		case 1:
			ast_verbose(VERBOSE_PREFIX_1 "%s\n", args.msg);
			break;
		case 2:
			ast_verbose(VERBOSE_PREFIX_2 "%s\n", args.msg);
			break;
		case 3:
			ast_verbose(VERBOSE_PREFIX_3 "%s\n", args.msg);
			break;
		default:
			ast_verbose(VERBOSE_PREFIX_4 "%s\n", args.msg);
		}
	}

	return 0;
}

static int log_exec(struct ast_channel *chan, void *data)
{
	char *parse;
	int lnum = -1;
	char extension[AST_MAX_EXTENSION + 5], context[AST_MAX_EXTENSION + 2];
	AST_DECLARE_APP_ARGS(args,
		AST_APP_ARG(level);
		AST_APP_ARG(msg);
	);

	if (ast_strlen_zero(data))
		return 0;

	parse = ast_strdupa(data);
	AST_STANDARD_APP_ARGS(args, parse);

	if (!strcasecmp(args.level, "ERROR")) {
		lnum = __LOG_ERROR;
	} else if (!strcasecmp(args.level, "WARNING")) {
		lnum = __LOG_WARNING;
	} else if (!strcasecmp(args.level, "NOTICE")) {
		lnum = __LOG_NOTICE;
	} else if (!strcasecmp(args.level, "DEBUG")) {
		lnum = __LOG_DEBUG;
	} else if (!strcasecmp(args.level, "VERBOSE")) {
		lnum = __LOG_VERBOSE;
	} else if (!strcasecmp(args.level, "DTMF")) {
		lnum = __LOG_DTMF;
	} else if (!strcasecmp(args.level, "EVENT")) {
		lnum = __LOG_EVENT;
	} else {
		ast_log(LOG_ERROR, "Unknown log level: '%s'\n", args.level);
	}

	if (lnum > -1) {
		snprintf(context, sizeof(context), "@ %s", chan->context);
		snprintf(extension, sizeof(extension), "Ext. %s", chan->exten);

		ast_log(lnum, extension, chan->priority, context, "%s\n", args.msg);
	}

	return 0;
}

static int unload_module(void)
{
	int res;

	res = ast_unregister_application(app_verbose);
	res |= ast_unregister_application(app_log);

	return res;	
}

static int load_module(void)
{
	int res;

	res = ast_register_application_xml(app_log, log_exec);
	res |= ast_register_application_xml(app_verbose, verbose_exec);

	return res;
}

AST_MODULE_INFO_STANDARD(ASTERISK_GPL_KEY, "Send verbose output");
