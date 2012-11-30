#include "iaxproxy.h"

#include "iaxproxy/ast_version.h"

static const char asterisk_version[] = "0.2.1.4";

static const char iaxproxy_release_name[] = "Blue Moon";

static const char asterisk_version_num[] = "00200";

const char *iax_get_release_name(void)
{
	return iaxproxy_release_name;
}

const char *ast_get_version(void)
{
	return asterisk_version;
}

const char *ast_get_version_num(void)
{
	return asterisk_version_num;
}

