/*
 * Naive test application.
 */

#include <czmq.h>
#include <getopt.h>
#include "tracks.h"

static bool verbose = true;
static int port = 3306;
static const char *host = "192.168.16.15";
static const char *user = "root";
static const char *password = "V0st!novaled#";
static const char *database = "nlts";

// ----------------------------------------------------------------------------
// Read the mac address of the primary network interface, return as a string
static const char * get_mac_addr(void)
{
    FILE *fp;
    static char mac_addr_str[18];

    fp = fopen("/sys/class/net/eth0/address", "r");
    if (fgets(mac_addr_str, 18, fp) == NULL)
        return NULL;
    fclose(fp);
    return mac_addr_str;
}

// ----------------------------------------------------------------------------
// Read the box id from the 1-wire EEPROM UID chip on the measurement box
// and return as a string
static const char * get_box_id(void)
{
    static char box_id_str[20] = {' '};
    const char *dir_name = "/var/lib/w1/bus.0/bus.0";
    struct dirent *d_entp;
    DIR *dp;
    FILE *fp;
    int path_length;
    char path[PATH_MAX];

    dp = opendir(dir_name);
    if (dp == NULL)
        return NULL;

    /* Traverse directory, take address from first sub-directory matching family code 23 */
    while((d_entp = readdir(dp)) != NULL)
    {
        if(strncmp(d_entp->d_name, "23.", 3) == 0)
        {
            path_length = snprintf (path, PATH_MAX,
                    "%s/%s/%s", dir_name, d_entp->d_name, "address");
            if (path_length >= PATH_MAX)
                return NULL;
            fp = fopen(path, "r");
            if (fgets(box_id_str, 17, fp) == NULL)
                return NULL;
            fclose(fp);
            break;
        }
    }
    closedir(dp);
    return box_id_str;
}
// ----------------------------------------------------------------------------
// Main loop

void
ldms_main_loop (bool verbose)
{
    /* Daemon-specific initialization goes here */
    zsys_set_logsystem (true);
    /* Ctrl-C and SIGTERM will set zsys_interrupted. */
    zsys_catch_interrupts();
    // Create state-based script engine
    zactor_t *actor = zactor_new (tracks, NULL);
    assert (actor);
    if (verbose)
        zstr_sendx (actor, "VERBOSE", NULL);
    zsock_send (actor, "si", "CONFIGURE", 5560);
    char *hostname = zstr_recv (actor);
    assert (*hostname);
    free (hostname);

    //  Create speaker beacon to broadcast our service
    zactor_t *speaker = zactor_new (zbeacon, NULL);
    assert (speaker);
    zsock_send (speaker, "si", "CONFIGURE", 9999);
    hostname = zstr_recv (speaker);
    if (!*hostname) {
        exit(EXIT_FAILURE);
    }
    free (hostname);
    //  We will broadcast the magic value 'VP'+mac address string+box id string
    //  "VP AA:BB:CC:DD:EE:FF 230123456789ABCDEF"
    byte announcement [40] = {'V', 'P', ' ' };
    strcat((char *)announcement, get_mac_addr());
    const char *boxid = get_box_id();
    if (boxid != NULL) {
        strcat((char *)announcement, " ");
        strcat((char *)announcement, boxid);
    }
    zsock_send (speaker, "sbi", "PUBLISH", announcement, 34, 5000);
    // Main loop
    while(!zsys_interrupted) {
        ;
    }
    /* Tear down the beacon */
    zstr_sendx (speaker, "SILENCE", NULL);
    zactor_destroy (&speaker);
    /* Tear down the state-based script engine */
    zactor_destroy (&actor);
    //  @end
}

static void print_usage(const char *prog)
{
	printf("Usage: %s [-vpuPh]\n", prog);
	puts("  -v --verbose  Print debugging messages\n"
	     "  -p --port     Port number to use for connection\n"
	     "  -u --user     User for login if not current user\n"
	     "  -P --password Password for login\n"
	     "  -h --host     Connect to host\n"
         );
	exit(1);
}

static void parse_opts(int argc, char *argv[])
{
	while (1) {
		static const struct option lopts[] = {
			{ "verbose",  1, 0, 'v' },
			{ "port",  1, 0, 'P' },
			{ "password",   1, 0, 'p' },
			{ "user",   1, 0, 'u' },
			{ "host",   1, 0, 'h' },
			{ NULL, 0, 0, 0 },
		};
		int c;

		c = getopt_long(argc, argv, "v:P:p:u:h", lopts, NULL);

		if (c == -1)
			break;

		switch (c) {
		case 'v':
            verbose = true;
			break;
		case 'p':
            port = atoi(optarg);
			break;
		case 'u':
            user = optarg;
			break;
		case 'P':
            password = optarg;
			break;
		default:
			print_usage(argv[0]);
			break;
		}
	}
}

int main(int argc, char *argv[])
{
    parse_opts(argc, argv);
    ldms_main_loop(verbose);

    exit(EXIT_SUCCESS);
}
