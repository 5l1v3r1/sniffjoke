/*
 *   SniffJoke is a software able to confuse the Internet traffic analysis,
 *   developed with the aim to improve digital privacy in communications and
 *   to show and test some securiy weakness in traffic analysis software.
 *   
 *   Copyright (C) 2010 vecna <vecna@delirandom.net>
 *                      evilaliv3 <giovanni.pellerano@evilaliv3.org>
 *
 *   This program is free software: you can redistribute it and/or modify
 *   it under the terms of the GNU General Public License as published by
 *   the Free Software Foundation, either version 3 of the License, or
 *   (at your option) any later version.
 *
 *   This program is distributed in the hope that it will be useful,
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *   GNU General Public License for more details.
 *
 *   You should have received a copy of the GNU General Public License
 *   along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include "UserConf.h"
#include "internalProtocol.h"

#include <cctype>
#include <sys/stat.h>

/* 
 * rules for parms:
 * sj_cmdline_opts contain only the option passed to the command line
 * ther other information are used as default.
 *
 * as priority, we use:
 * 1) a specific config or enabler file override location 
 * both of these file are looked by default in the INSTALL_SYSCONFDIR
 *
 * 3) if a location is used, is append to the base filename, and
 *    as default, is used the suffix ".generic"
 */
UserConf::UserConf(const struct sj_cmdline_opts &cmdline_opts) :
cmdline_opts(cmdline_opts),
chroot_status(false)
{
    debug.log(VERBOSE_LEVEL, __func__);

    memset(&runconfig, 0x00, sizeof (sj_config));

    if (cmdline_opts.location[0])
    {
        snprintf(configfile, sizeof (configfile), "%s%s/%s", WORK_DIR, cmdline_opts.location, FILE_CONF);
                snprintf(runconfig.chroot_dir, sizeof (configfile), "%s%s", WORK_DIR, cmdline_opts.location);
    }
    else
    {
        debug.log(VERBOSE_LEVEL, "is highly suggestes to use sniffjoke specifying a location (--location option)");
        debug.log(VERBOSE_LEVEL, "a defined location means that the network it's profiled for the best results");
        debug.log(VERBOSE_LEVEL, "a brief explanation about this can be found at: http://www.delirandom.net/sniffjoke/location");
        snprintf(configfile, sizeof (configfile), "%s%s/%s", WORK_DIR, cmdline_opts.location, FILE_CONF);
        snprintf(runconfig.chroot_dir, sizeof (configfile), "%s%s", WORK_DIR, DEFAULT_LOCATION);
    }

    mkdir(runconfig.chroot_dir, 0700);

    /* load does NOT memset to 0 the runconfig struct! and load defaults if file are not present */
    load();

    switch (runconfig.mode)
    {
    case '0':
        iplistmap = new IPListMap(FILE_IPBLACKLIST);
        break;
    case '1':
        iplistmap = new IPListMap(FILE_IPWHITELIST);
        break;
    }
}

UserConf::~UserConf()
{
    debug.log(DEBUG_LEVEL, "%s [process %d chroot %s], referred config file %s",
              __func__, getpid(), chroot_status ? "YES" : "NO", configfile);
}

/* private function useful for resolution of code/name */
const char *UserConf::resolve_weight_name(int command_code)
{
    switch (command_code)
    {
    case HEAVY: return "heavy";
    case NORMAL: return "normal";
    case LIGHT: return "light";
    case NONE: return "no hacks";
    default: debug.log(ALL_LEVEL, "danger: found invalid code in ports configuration");
        return "VERY BAD BUFFER CORRUPTION! I WISH NO ONE EVER SEE THIS LINE";
    }
}

void UserConf::autodetect_local_interface()
{
    /* check this command: the flag value, matched in 0003, is derived from:
     *     /usr/src/linux/include/linux/route.h
     */
    const char *cmd = "grep 0003 /proc/net/route | grep 00000000 | cut -b -7";
    FILE *foca;
    char imp_str[SMALLBUF];
    uint8_t i;

    debug.log(ALL_LEVEL, "++ detecting external gateway interface with [%s]", cmd);

    foca = popen(cmd, "r");
    fgets(imp_str, SMALLBUF, foca);
    pclose(foca);

    for (i = 0; i < strlen(imp_str) && isalnum(imp_str[i]); ++i)
        runconfig.interface[i] = imp_str[i];

    if (i < 3)
    {
        debug.log(ALL_LEVEL, "-- default gateway not present: sniffjoke cannot be started");
        SJ_RUNTIME_EXCEPTION("");
    }
    else
    {
        debug.log(ALL_LEVEL, "  == detected external interface with default gateway: %s", runconfig.interface);
    }
}

void UserConf::autodetect_local_interface_ip_address()
{
    char cmd[MEDIUMBUF];
    FILE *foca;
    char imp_str[SMALLBUF];
    uint8_t i;
    snprintf(cmd, MEDIUMBUF, "ifconfig %s | grep \"inet addr\" | cut -b 21-",
             runconfig.interface
             );

    debug.log(ALL_LEVEL, "++ detecting interface %s ip address with [%s]", runconfig.interface, cmd);

    foca = popen(cmd, "r");
    fgets(imp_str, SMALLBUF, foca);
    pclose(foca);

    for (i = 0; i < strlen(imp_str) && (isdigit(imp_str[i]) || imp_str[i] == '.'); ++i)
        runconfig.local_ip_addr[i] = imp_str[i];

    debug.log(ALL_LEVEL, "  == acquired local ip address: %s", runconfig.local_ip_addr);
}

void UserConf::autodetect_gw_ip_address()
{
    const char *cmd = "route -n | grep ^0.0.0.0 | grep UG | cut -b 17-32";
    FILE *foca;
    char imp_str[SMALLBUF];
    uint8_t i;

    debug.log(ALL_LEVEL, "++ detecting gateway ip address with [%s]", cmd);

    foca = popen(cmd, "r");
    fgets(imp_str, SMALLBUF, foca);
    pclose(foca);

    for (i = 0; i < strlen(imp_str) && (isdigit(imp_str[i]) || imp_str[i] == '.'); ++i)
        runconfig.gw_ip_addr[i] = imp_str[i];
    if (strlen(runconfig.gw_ip_addr) < 7)
    {
        debug.log(ALL_LEVEL, "  -- unable to autodetect gateway ip address, sniffjoke cannot be started");
        SJ_RUNTIME_EXCEPTION("");
    }
    else
    {
        debug.log(ALL_LEVEL, "  == acquired gateway ip address: %s", runconfig.gw_ip_addr);
    }
}

void UserConf::autodetect_gw_mac_address()
{
    char cmd[MEDIUMBUF];
    FILE *foca;
    char imp_str[SMALLBUF];
    uint8_t i;
    snprintf(cmd, MEDIUMBUF, "ping -W 1 -c 1 %s", runconfig.gw_ip_addr);

    debug.log(ALL_LEVEL, "++ pinging %s for ARP table popoulation motivations [%s]", runconfig.gw_ip_addr, cmd);

    foca = popen(cmd, "r");
    /* we do not need the output of ping, we need to wait the ping to finish
     * and pclose does this =) */
    pclose(foca);

    memset(cmd, 0x00, sizeof (cmd));
    snprintf(cmd, MEDIUMBUF, "arp -n | grep \"%s \" | cut -b 34-50", runconfig.gw_ip_addr);
    debug.log(ALL_LEVEL, "++ detecting mac address of gateway with %s", cmd);
    foca = popen(cmd, "r");
    fgets(imp_str, SMALLBUF, foca);
    pclose(foca);

    for (i = 0; i < strlen(imp_str) && (isxdigit(imp_str[i]) || imp_str[i] == ':'); ++i)
        runconfig.gw_mac_str[i] = imp_str[i];
    if (i != 17)
    {
        debug.log(ALL_LEVEL, "  -- unable to autodetect gateway mac address");
        SJ_RUNTIME_EXCEPTION("");
    }
    else
    {
        debug.log(ALL_LEVEL, "  == automatically acquired mac address: %s", runconfig.gw_mac_str);
        uint32_t mac[6];
        sscanf(runconfig.gw_mac_str, "%2x:%2x:%2x:%2x:%2x:%2x", &mac[0], &mac[1], &mac[2], &mac[3], &mac[4], &mac[5]);
        for (i = 0; i < 6; ++i)
            runconfig.gw_mac_addr[i] = mac[i];
    }
}

void UserConf::autodetect_first_available_tunnel_interface()
{
    const char *cmd = "ifconfig -a | grep tun | cut -b -7";
    FILE *foca;
    char imp_str[SMALLBUF];

    debug.log(ALL_LEVEL, "++ detecting first unused tunnel device with [%s]", cmd);

    foca = popen(cmd, "r");
    for (runconfig.tun_number = 0;; ++runconfig.tun_number)
    {
        memset(imp_str, 0x00, sizeof (imp_str));
        fgets(imp_str, SMALLBUF, foca);
        if (imp_str[0] == 0x00)
            break;
    }
    pclose(foca);
    debug.log(ALL_LEVEL, "  == detected %d as first unused tunnel device", runconfig.tun_number);
}

/* this method is called by SniffJoke.cc */
void UserConf::network_setup(void)
{
    debug.log(DEBUG_LEVEL, "Initializing network for service/child: %d", getpid());

    /* autodetect is always used, we should not trust the preloaded configuration */
    autodetect_local_interface();
    autodetect_local_interface_ip_address();
    autodetect_gw_ip_address();
    autodetect_gw_mac_address();
    autodetect_first_available_tunnel_interface();

    debug.log(VERBOSE_LEVEL, "-- system local interface: %s, %s address", runconfig.interface, runconfig.local_ip_addr);
    debug.log(VERBOSE_LEVEL, "-- default gateway mac address: %s", runconfig.gw_mac_str);
    debug.log(VERBOSE_LEVEL, "-- default gateway ip address: %s", runconfig.gw_ip_addr);
    debug.log(VERBOSE_LEVEL, "-- first available tunnel interface: tun%d", runconfig.tun_number);
}

/* internal function called by the overloaded parseMatch */
bool UserConf::parseLine(FILE *cf, char userchoose[SMALLBUF], const char *keyword)
{
    rewind(cf);
    char line[MEDIUMBUF];

    do
    {
        fgets(line, MEDIUMBUF, cf);

        if (line[0] == '#' || line[0] == '\n' || line[0] == ' ')
            continue;

        if (strlen(line) < (strlen(keyword) + 3))
            continue;

        if (!memcmp(keyword, line, strlen(keyword)))
        {
            /* C's chop() */
            line[strlen(line) - 1] = 0x00;
            memcpy(userchoose, (&line[strlen(keyword) + 1]), strlen(line) - strlen(keyword) - 1);
            return true;
        }
    }
    while (!feof(cf));

    return false;
}

/* start with the less used (only one time, for this reason differ) parseMatch overloaded name */
void UserConf::parseMatch(bool &dst, const char *name, FILE *cf, bool cmdopt, const bool difolt)
{
    char useropt[SMALLBUF];
    const char *debugfmt = NULL;

    /* command line priority always */
    if (cmdopt != difolt)
    {
        dst = cmdopt;
        debugfmt = "%s/bool: keyword %s used command line value: [%s]";
        goto EndparseMatchBool;
    }

    if (cf == NULL)
    {
        dst = difolt;
        debugfmt = "%s/bool: keyword %s config file not present, used default: [%s]";
        goto EndparseMatchBool;
    }

    memset(useropt, 0x00, SMALLBUF);

    if (parseLine(cf, useropt, name))
    {
        /* dst is large MEDIUMBUF, and none useropt will overflow this size */
        if (!memcmp(useropt, "true", strlen("true")))
            dst = true;

        if (!memcmp(useropt, "false", strlen("false")))
            dst = false;

        debugfmt = "%s/bool: keyword %s read from config file: [%s]";
    }
    else
    {
        dst = difolt;
        debugfmt = "%s/bool: not found %s option in conf file, using default: [%s]";
    }

EndparseMatchBool:
    debug.log(DEBUG_LEVEL, debugfmt, __func__, name, dst ? "true" : "false");
}

void UserConf::parseMatch(char *dst, const char *name, FILE *cf, const char *cmdopt, const char *difolt)
{
    char useropt[SMALLBUF];
    const char *debugfmt = NULL;

    memset(useropt, 0x00, SMALLBUF);

    /* only-plugin will be empty, no other cases */
    if (cf == NULL && difolt == NULL)
    {
        debugfmt = "%s/string: conf file not found and option neither: used no value in %s";
        memset(dst, 0x00, MEDIUMBUF);
        goto EndparseMatchString;
    }

    /* if the file is NULL, the default is used */
    if (cf == NULL)
    {
        debugfmt = "%s/string: conf file not found, for %s used default %s";
        memcpy(dst, difolt, strlen(difolt));
        goto EndparseMatchString;
    }

    if (parseLine(cf, useropt, name))
    {
        debugfmt = "%s/string: parsed keyword %s [%s] option in conf file";
        /* dst is large MEDIUMBUF, and none useropt will overflow this size */
        memcpy(dst, useropt, strlen(useropt));

        if (!memcmp(dst, cmdopt, strlen(dst)) && !memcmp(dst, difolt, strlen(difolt)))
        {
            debug.log(VERBOSE_LEVEL, "warning, config file specify '%s' as %s, command line as %s (default %s). used %s",
                      name, dst, cmdopt, difolt, cmdopt);
            memset(dst, 0x00, MEDIUMBUF);
            memcpy(dst, cmdopt, strlen(cmdopt));
            debugfmt = "%s/string: keyword %s command line override, %s used";
        }
        goto EndparseMatchString;
    }

    /* if was not found in the file, the default is used */
    if (difolt != NULL)
    {
        memset(dst, 0x00, MEDIUMBUF);
        memcpy(dst, difolt, strlen(difolt));
        debugfmt = "%s/string: %s not found in config file, used default %s";
    }

EndparseMatchString:
    debug.log(DEBUG_LEVEL, debugfmt, __func__, name, dst);
}

void UserConf::parseMatch(uint16_t &dst, const char *name, FILE *cf, uint16_t cmdopt, const uint16_t difolt)
{
    char useropt[SMALLBUF];
    const char *debugfmt = NULL;

    /* if the file is NULL, the default is used */
    if (cf == NULL)
    {
        memcpy((void *) &dst, (void *) &difolt, sizeof (difolt));
        debugfmt = "%s/uint16: conf file not found, for %s used default %d";
        goto EndparseMatchShort;
    }
    memset(useropt, 0x00, SMALLBUF);

    if (parseLine(cf, useropt, name))
    {
        debugfmt = "%s/uint16: parsed keyword %s [%d] option in conf file";
        dst = atoi(useropt);

        if (dst != cmdopt && dst != difolt)
        {
            debug.log(VERBOSE_LEVEL, "warning, config file specify '%s' as %d, command line as %d (default %d). used %d",
                      name, dst, cmdopt, difolt, cmdopt);
            dst = cmdopt;
        }
        goto EndparseMatchShort;
    }

    if (difolt)
    {
        debugfmt = "%s/uint16: %s not found in config file, used default %d";
        dst = difolt;
    }

EndparseMatchShort:
    debug.log(DEBUG_LEVEL, debugfmt, __func__, name, dst);
}

/* simple utiliy for dumping */
uint32_t UserConf::dumpIfPresent(uint8_t *p, uint32_t datal, const char *name, char *data)
{
    uint32_t written = 0;

    if (data[0])
    {
        written = snprintf((char *) p, datal, "%s:%s\n", name, data);
    }
    return written;
}

uint32_t UserConf::dumpComment(uint8_t *p, uint32_t datal, const char *writedblock)
{
    return snprintf((char *) p, datal, writedblock);
}

bool UserConf::load(void)
{
    FILE *loadfd;

    if ((loadfd = sj_fopen(configfile, "r")) == NULL)
        debug.log(ALL_LEVEL, "configuration file %s not accessible: %s, using default", configfile, strerror(errno));
    else
        debug.log(DEBUG_LEVEL, "opening configuration file: %s", configfile);

    parseMatch(runconfig.user, "user", loadfd, cmdline_opts.user, DROP_USER);
    parseMatch(runconfig.group, "group", loadfd, cmdline_opts.group, DROP_GROUP);
    parseMatch(runconfig.mode, "mode", loadfd, cmdline_opts.mode, DEFAULT_MODE);
    parseMatch(runconfig.admin_address, "management-address", loadfd, cmdline_opts.admin_address, DEFAULT_ADMIN_ADDRESS);
    parseMatch(runconfig.admin_port, "management-port", loadfd, cmdline_opts.admin_port, DEFAULT_ADMIN_PORT);
    parseMatch(runconfig.debug_level, "debug", loadfd, cmdline_opts.debug_level, DEFAULT_DEBUG_LEVEL);
    parseMatch(runconfig.onlyplugin, "only-plugin", loadfd, cmdline_opts.onlyplugin, NULL);
    if (runconfig.onlyplugin[0])
    {
        debug.log(VERBOSE_LEVEL, "single plugin %s will override plugins list in the enabler file", runconfig.onlyplugin);
    }

    parseMatch(runconfig.active, "active", loadfd, cmdline_opts.active, DEFAULT_START_STOPPED);

    /* TODO BOTH OF THEM */
    /* loadAggressivity(runconfig.aggressivity_file); */
    /* loadFrequency(runconfig.frequency_file); */
    /* YES, XXX, TODO INSTEAD OF: */
    for (uint16_t i = 0; i < PORTNUMBER; ++i)
        runconfig.portconf[i] = NORMAL;

    if (loadfd)
        fclose(loadfd);

    return true;

}

void UserConf::dump(void)
{

}