/*
 * Naive test application.
 */

#include "config.h"
#include <czmq.h>
#include <getopt.h>

#define PUBLISH_PERIOD_MSEC 5000
#define BEACON_PUBLISH_PORT 9999 

typedef struct {
    bool verbose;
    int port;
    char *host;
    char *user;
    char *password;
    char *database;
} ldms_config_t;

static int port = 3306;
static bool verbose = false;
static const char *host = "192.168.16.15";
static const char *user = "root";
static const char *password = "V0st!novaled#";
static const char *database = "nlts";

static  ldms_config_t *ldms_config;

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
			{ "verbose",  0, 0, 'v' },
			{ "port",  1, 0, 'P' },
			{ "password",   1, 0, 'p' },
			{ "user",   1, 0, 'u' },
			{ "host",   1, 0, 'h' },
			{ NULL, 0, 0, 0 },
		};
		int c;

		c = getopt_long(argc, argv, "vP:p:u:h", lopts, NULL);

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
    parse_opts (argc, argv);
    /* Daemon-specific initialization goes here */
    zsys_init ();
    zsys_set_logsystem (true);
    /* Ctrl-C and SIGTERM will set zsys_interrupted. */
    zsys_catch_interrupts ();
    /* Show version string */
    zsys_info ("This is %s\n", PACKAGE_STRING);
    
    //  Create speaker beacon to broadcast our service
    zactor_t *speaker = zactor_new (zbeacon, NULL);
    assert (speaker);
    zsys_info ("Beacon service initialized");

    zsock_send (speaker, "si", "CONFIGURE", BEACON_PUBLISH_PORT);
    char *hostname = zstr_recv (speaker);
    if (!*hostname) {
        exit(EXIT_FAILURE);
    }
    free (hostname);
    zsys_info ("Beacon service configured");
    //  We will broadcast the magic value 'VP'+mac address string
    //  "VP AA:BB:CC:DD:EE:FF"
    byte announcement [21] = {'V', 'P', ' ' };
    strcat ((char *)announcement, get_mac_addr ());
    // Publish announcement every 5000ms
    zsock_send (speaker, "sbi", "PUBLISH", announcement, 20, PUBLISH_PERIOD_MSEC);
    zsys_info ("Publish [[%s]] every %d ms", announcement, PUBLISH_PERIOD_MSEC);

    /* Main loop */
    while (!zsys_interrupted) {
        zclock_sleep (10u); /* release to let the OS do something else */
    }

    /* Tear down the beacon */
    zstr_sendx (speaker, "SILENCE", NULL);
    zsys_info ("Tear down beacon service");
    zactor_destroy (&speaker);
    exit (EXIT_SUCCESS);
}
