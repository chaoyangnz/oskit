
/*
 * Entropy pool test kernel.  The code that runs the network and disk was
 * snarfed from other example kernels.
 */

#include <malloc.h> /* smemalign */
#include <oskit/types.h>
#include <oskit/clientos.h>
#include <oskit/dev/osenv.h>
#include <oskit/dev/entropy.h>
#include <oskit/dev/ethernet.h>
#include <oskit/dev/linux.h>
#include <oskit/net/ether.h>
#include <oskit/c/string.h>
#include <oskit/startup.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <stdlib.h>
#include <stdio.h>
#include <assert.h>
#include "bootp.h"
#include "netinet.h"

extern oskit_services_t *global_registry;
extern oskit_services_t *device_registry;
extern oskit_services_t *driver_registry;

#define MAX_ETHERNET_DEVICES 8		/* max number of ethernet cards */

extern void (*_init_devices_blk)(void);

extern oskit_error_t (*_init_devices_entropy)(void);

int test_block(int, int);
int read_test(int, int, char *);

oskit_blkio_t *io;

/* These are in terms of SECTORS. */
int test_offset[] = { 0, 1, 3,  7, 8, 63, 64, 127, 128, 1024, 1025 };
int test_size[]   = { 1, 2, 5, 11, 8, 63, 64, 128, 129, 333, 1024, 1025 };

#define n_offsets (sizeof(test_offset) / sizeof(test_offset[0]))
#define n_sizes (sizeof(test_size) / sizeof(test_size[0]))

static int ndev;

struct etherdev {
	oskit_etherdev_t	*dev;
	oskit_netio_t	*send_nio;
	oskit_netio_t	*recv_nio;
	oskit_devinfo_t	info;
	unsigned char	haddr[OSKIT_ETHERDEV_ADDR_SIZE];
	struct in_addr  myip;	/* My IP address in host order. */
	int		ipbuf[512];
};
static struct etherdev devs[MAX_ETHERNET_DEVICES];

static void whoami(int, oskit_etherdev_t *, struct in_addr *);
oskit_error_t net_receive(void *, oskit_bufio_t *, oskit_size_t);
static unsigned short ipcksum(void *, int);
static void swapn(unsigned char [], unsigned char [], int);
static char *iptoa(unsigned long);

void oskit_dump_drivers(void);

int
main(int argc, char **argv)
{
	oskit_error_t err = 0;
	oskit_etherdev_t **etherdev_aref;
	oskit_entropy_t **entropy_aref;
	oskit_osenv_t *osenv;
	int i;
	char name[10];
	oskit_size_t sec_size;
	oskit_off_t disk_size;
	int rc = 0;
	int n_tests, j;
	oskit_entropy_stats_t entropy_stats;
	unsigned int n_calls_1;
	unsigned int entropy_count_1;
	unsigned int n_calls_2;
	unsigned int entropy_count_2;

	/* Must be on little endian machine. */
	i = 1;
	assert(*(char *)&i == 1);
	
	oskit_clientos_init();
	osenv = start_osenv();

	/*
	 * Make it possible to query the device registry.
	 */
	osenv_device_registration_init();

	/*
	 * Initialize Linux devices.
	 */
	oskit_dev_init(osenv);
	oskit_linux_init_osenv(osenv);

	osenv_process_lock();
	start_devices();
	osenv_process_unlock();

	/*
	 * Init the entropy device(s).
	 */
	_init_devices_entropy = oskit_linux_init_entropy;
	(*_init_devices_entropy)();
	
	/*
	 * Init the block devices.
	 */
	_init_devices_blk = oskit_linux_init_blk;
	(*_init_devices_blk)();
	/**/

	/*
	 * Start the network.
	 */
	/*start_network();*/
	
	/*
	 * Employ frobiscus.
	 */
	oskit_dev_probe();

	/*
	 * Check to see if the entropy device(s are) there.
	 */
	ndev = osenv_device_lookup(&oskit_entropy_iid,
				   (void***)&entropy_aref);
	if (ndev > 0)
	{
	    osenv_log(OSENV_LOG_INFO, "DLD: found entropy device!\n");
	}
	else
	{
	    osenv_log(OSENV_LOG_INFO, "did not find entropy device!\n");
	}	    

	/*
	 * Start the network devices
	 */
	start_net_devices();
	
	/*
	 * Find all the Ethernet device nodes.
	 */
	osenv_log(OSENV_LOG_INFO, "Looking up enet devices\n");
	ndev = osenv_device_lookup(&oskit_etherdev_iid,
				   (void***)&etherdev_aref);
	if (ndev <= 0)
		panic("No ethernet adaptors found.");
	if (ndev > MAX_ETHERNET_DEVICES)
		panic("Too many Ethernet adaptors found.");

	/*
	 * Open all the devices and have them hand packets to the OS.
	 */
	osenv_log(OSENV_LOG_INFO, "%d Ethernet adaptor%s found:\n", ndev, ndev > 1 ? "s" : "");
	for (i = 0; i < ndev; i++) {
		int j;

		err = oskit_etherdev_getinfo(etherdev_aref[i], &devs[i].info);
		if (err)
			panic("error getting info from ethercard %d", i);

		oskit_etherdev_getaddr(etherdev_aref[i], devs[i].haddr);

		/* Show information about this adaptor */
		printf("  %-16s%-40s ", devs[i].info.name,
			devs[i].info.description
			? devs[i].info.description : "");
		for (j = 0; j < 5; j++) 
		    printf("%02x:", devs[i].haddr[j]);
		printf("%02x\n", devs[i].haddr[5]);

		/* get the IP address */
		whoami(i, etherdev_aref[i], &devs[i].myip);

		devs[i].dev = etherdev_aref[i];
		devs[i].recv_nio = oskit_netio_create(net_receive, &devs[i]);
		if (devs[i].recv_nio == NULL)
			panic("unable to create recv_nio");

		err = oskit_etherdev_open(etherdev_aref[i], 0,
					  devs[i].recv_nio,
					  &devs[i].send_nio);
	}

	if (!osenv_intr_enabled()) {
		printf("Interrupts not enabled! enabling!\n");
		osenv_intr_enable();
	}

	/*
	 * Start the block devices
	 */
	_init_devices_blk = oskit_linux_init_blk;
	(*_init_devices_blk)();
	oskit_dev_probe();

	for (;;) {

	    do {
		oskit_entropy_getstats(entropy_aref[0], &entropy_stats);
		n_calls_1 = entropy_stats.n_calls;
		entropy_count_1 = entropy_stats.entropy_count;
		
#ifndef RNDTEST_INTERACTIVE
		name[0] = 'h';
		name[1] = 'd';
		name[2] = 'a';
		name[3] = '0';
		name[4] = '\0';
#else
		osenv_log(OSENV_LOG_INFO,
			  "Start pinging, then enter drive (eg 'hda0') to test, `quit' to exit: ");
		gets(name);
#endif

		if (!strcmp(name, "quit")) {
		    rc = 1;
		    goto done;
		}

		if (!strcmp(name, ""))
		    continue;

		osenv_log(OSENV_LOG_INFO, "Opening disk %s...\n", name);
		err = oskit_linux_block_open(name, OSKIT_DEV_OPEN_READ, &io);

		if (err) {
			if (err == OSKIT_E_DEV_NOSUCH_DEV)
			    osenv_log(OSENV_LOG_INFO,
				      "disk %s does not exist!\n", name);
			else	
			    osenv_log(OSENV_LOG_INFO,
				      "error %x opening disk %s\n", err, name);
		}
		
	    } while (err);

	    sec_size = oskit_blkio_getblocksize(io);
	    osenv_log(OSENV_LOG_INFO,
		      "disk native sectors are %d bytes\n", sec_size);

	    if (oskit_blkio_getsize(io, &disk_size)) {
		osenv_log(OSENV_LOG_INFO,
			  "Can't determine disk size...assuming the disk is big enough!\n");
		disk_size = (test_offset[n_offsets - 1] + 
			     test_size[n_sizes - 1]) * sec_size;
	    } else
		osenv_log(OSENV_LOG_INFO, "size of disk is %d kbytes\n", 
			  (unsigned int)(disk_size >> 10));

	    n_tests = n_offsets;
	    osenv_log(OSENV_LOG_INFO, "\nRunning %d tests\n\n", n_tests);
	    for (i = 0; i < n_tests; i++) {
		for (j = 0; j < n_sizes; j++) {
		    if (((test_offset[i] + test_size[j]) * sec_size) <=
			disk_size) {
			rc = test_block(test_offset[i] * sec_size, 
					test_size[j] * sec_size);

			if (rc) {
			    osenv_log(OSENV_LOG_INFO,
				      "\nEXITING: errors in the disk tests.\n");
				
			    oskit_blkio_release(io);
			    goto done;
			}
		    }
		}
	    }
	    oskit_blkio_release(io);

	    oskit_entropy_getstats(entropy_aref[0], &entropy_stats);
	    n_calls_2 = entropy_stats.n_calls;
	    entropy_count_2 = entropy_stats.entropy_count;

	    osenv_log(OSENV_LOG_INFO,
		      "entropy count 1: %d, call count 1: %d\n",
		      entropy_count_1, n_calls_1);
	    osenv_log(OSENV_LOG_INFO,
		      "entropy count 2: %d, call count 2: %d\n",
		      entropy_count_2, n_calls_2);
	}

 done:
	oskit_entropy_release(entropy_aref[0]);
	return rc;
}

int
read_test(int offset, int size, char *buf)
{
	int err, amt;

	err = oskit_blkio_read(io, buf, offset, size, &amt);
	if (err) {
	    osenv_log(OSENV_LOG_INFO, "  Read ERROR %08x\n", err);
	    return(1);
	}

	return 0;
}

/*
 * This runs tests on a bunch of disk blocks.
 * Generally, these should be an integral multiple of sectors aligned
 * on a sector boundary.
 */
int
test_block(int offset, int size)
{
	char *wbuf;

	/* Allocate memory for the data */
	wbuf = smemalign(512, size);
	if (!wbuf) {
	    osenv_log(OSENV_LOG_INFO, "  can't allocate buf!\n");
	    _exit(1);
	}
	memset(wbuf, 0xA5, size);

	/* Read in blocks from the disk. */
	if (read_test(offset, size, wbuf))
		return -1;

	/* release the memory */
	sfree(wbuf, size);
	
	return 0;
}

/*
 * Figure out my IP address.
 */
static void
whoami(int ifnum, oskit_etherdev_t *dev, struct in_addr *myip)
{
	struct in_addr ip;

	char ipaddr[4*4];			/* nnn.nnn.nnn.nnnz */
	get_ipinfo(dev, ipaddr, 0, 0, 0);
	inet_aton(ipaddr, &ip);
	myip->s_addr = ntohl(ip.s_addr);
}

oskit_error_t
net_receive(void *data, oskit_bufio_t *b, oskit_size_t pkt_size)
{
	oskit_error_t rval = 0;
	unsigned char bcastaddr[6] = {0xff, 0xff, 0xff, 0xff, 0xff, 0xff};
	static unsigned char *frame;
	struct ether_header *eth;
	oskit_bufio_t *our_buf;
	int err;
	struct etherdev *dev = (struct etherdev *)data;

	if (pkt_size > ETH_MAX_PACKET) {
	    osenv_log(OSENV_LOG_INFO,
		      "%s: Hey Wally, I caught a big one! -- %d bytes\n",
		      dev->info.name, pkt_size);
	    rval = OSKIT_E_DEV_BADPARAM;
	    goto done;
	}

	err = oskit_bufio_map(b, (void **)&frame, 0, pkt_size);
	assert(err == 0);

	eth  = (struct ether_header *)frame;

	if (memcmp(eth->ether_dhost, &dev->haddr, sizeof dev->haddr) != 0
	    && memcmp(eth->ether_dhost, bcastaddr, sizeof bcastaddr) != 0) 
		goto done;			/* not for me */

	switch (ntohs(eth->ether_type)) {
	case ETHERTYPE_IP: {
		static int ipid = 0xb;		/* something random */
		/*static int count = 0;*/
		struct ip *ip;
		int hlen;
		struct in_addr hin;
		struct icmp *icmp;

		/* XXX deal with fragments. */

		ip = (struct ip *)(frame + sizeof(struct ether_header));
		hlen = ip->ip_hl << 2;

		/*
		 * Copy out from ip header on to ensure proper alignment
		 * on machines that are not byte addressable!
		 */
		memcpy(dev->ipbuf,
		       frame + sizeof(struct ether_header),
		       ntohs(ip->ip_len));
		ip = (struct ip *) dev->ipbuf;

		hin.s_addr = ntohl(ip->ip_src.s_addr);
		if (ipcksum(ip, hlen) != 0 || ip->ip_p != IPPROTO_ICMP)
			goto done;		/* bad cksum or not IP */

		icmp = (struct icmp *)((char *)ip + hlen);
		if (icmp->icmp_type != ICMP_ECHO
		    || ipcksum(icmp, ntohs(ip->ip_len) - hlen) != 0)
			goto done;	       /* bad cksum or not ICMP_ECHO */

		/* Send the reply. */
		icmp->icmp_type = ICMP_ECHOREPLY;
		/* Another optimization.  Only the icmp type field in the reply
		 * changes, so we can use this apriori knowledge to determine
		 * the new checksum based upon the old checksum.  Eliminates
		 * touching all of the data in the packet once more */
		if (icmp->icmp_cksum >= 65527)
			icmp->icmp_cksum += (unsigned short) 9;
		else
			icmp->icmp_cksum += (unsigned short) 8;

		ip->ip_id = htons(ipid++);

		/*
		 * We can't just swap the addresses,
		 * since broadcast packets didn't come to our address.
		 */
		ip->ip_dst = ip->ip_src;
		ip->ip_src.s_addr = htonl(dev->myip.s_addr);

		ip->ip_sum = 0;
		ip->ip_sum = ipcksum(ip, hlen);

		swapn(eth->ether_dhost, eth->ether_shost,
		      sizeof eth->ether_shost);

		/*
		 * Copy data back into original data buffer.
		 */
		memcpy(frame + sizeof(struct ether_header),
		       ip, ntohs(ip->ip_len));

		/* Removed this code because it's an unnecessary copy.
		 * We're already violating the semantics of the push
		 * by editing the data pushed to us _before_ we copy,
		 * so this copy is extraneous
		 */
		oskit_netio_push(dev->send_nio, b, pkt_size);

		break;
	}
	case ETHERTYPE_ARP: {
		struct arphdr *arp;
		struct in_addr ip;

		arp = (struct arphdr *)(frame + sizeof(struct ether_header));
		ip.s_addr = ntohl(*(unsigned long *)arp->ar_tpa);

		if (ntohs(arp->ar_hrd) != ARPHRD_ETHER
		    || ntohs(arp->ar_pro) != ETHERTYPE_IP
		    || ntohs(arp->ar_op) != ARPOP_REQUEST
		    || ip.s_addr != dev->myip.s_addr)
			goto done;		/* wrong proto or addr */

		/* Send the reply. */
		arp->ar_op = htons(ARPOP_REPLY);
		swapn(arp->ar_spa, arp->ar_tpa, sizeof arp->ar_tpa);
		swapn(arp->ar_sha, arp->ar_tha, sizeof arp->ar_tha);
		memcpy(arp->ar_sha, dev->haddr, sizeof dev->haddr);

		/* Fill in the ethernet addresses. */
		bcopy(&eth->ether_shost, &eth->ether_dhost,
		      sizeof(dev->haddr));
		bcopy(&dev->haddr, &eth->ether_shost, sizeof(dev->haddr));

		our_buf = oskit_bufio_create(pkt_size);
		if (b != NULL) {
			oskit_size_t got;

			oskit_bufio_write(our_buf, frame, 0, pkt_size, &got);
			assert(got == pkt_size);
			oskit_netio_push(dev->send_nio, our_buf, pkt_size);
			oskit_bufio_release(our_buf);
		} else {
			osenv_log(OSENV_LOG_INFO,
				  "couldn't allocate bufio for ARP reply\n");
		}

		osenv_log(OSENV_LOG_INFO, "%s: sent ARP reply to %s\n",
			  dev->info.name,
			  iptoa(*(unsigned long *)arp->ar_tpa));
		break;
	}
	default:
		break;
	}

done:
	return rval;
}

static unsigned short
ipcksum(void *buf, int nbytes)
{
	unsigned long sum;

	sum = 0;
	while (nbytes > 1) {
		sum += *(unsigned short *)buf;
		buf += 2;
		nbytes -= 2;
	}
	/* Add in odd byte if any. */
	if (nbytes == 1)
		sum += *(char *)buf;

	sum = (sum >> 16) + (sum & 0xffff);     /* hi + lo */
	sum += (sum >> 16);                     /* add carry, if any */

	return (~sum);
}

static void
swapn(unsigned char a[], unsigned char b[], int n)
{
	int i;
	unsigned char t[n];

	for (i = 0; i < n; i++) {
		t[i] = a[i];
		a[i] = b[i];
		b[i] = t[i];
	}
}

/* The address we are passed is in network order. */
static char *
iptoa(unsigned long x)
{
	static char buf[4*4];		/* nnn.nnn.nnn.nnnz */
	unsigned char *p = (unsigned char *)&x;

	sprintf(buf, "%d.%d.%d.%d", p[0], p[1], p[2], p[3]);
	return buf;
}

/* End of rndtest.c */
