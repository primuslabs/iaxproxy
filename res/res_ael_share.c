/*
 * Asterisk -- An open source telephony toolkit.
 *
 * Copyright (C) 2007, Digium, Inc.
 *
 * Steve Murphy <murf@digium.com>
 *
 * See http://www.iaxproxy.org for more information about
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
 * \brief Shareable AEL code -- mainly between internal and external modules
 *
 * \author Steve Murphy <murf@digium.com>
 * 
 * \ingroup applications
 */

#include "iaxproxy.h"

ASTERISK_FILE_VERSION(__FILE__, "$Revision: 89511 $")

#include "iaxproxy/file.h"
#include "iaxproxy/channel.h"
#include "iaxproxy/pbx.h"
#include "iaxproxy/config.h"
#include "iaxproxy/module.h"
#include "iaxproxy/lock.h"
#include "iaxproxy/cli.h"


static int unload_module(void)
{
	return 0;
}

static int load_module(void)
{
	return AST_MODULE_LOAD_SUCCESS;
}

AST_MODULE_INFO(ASTERISK_GPL_KEY, AST_MODFLAG_GLOBAL_SYMBOLS, "share-able code for AEL",
				.load = load_module,
				.unload = unload_module
	);
