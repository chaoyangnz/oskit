
/*
 * Entropy device unit tests.
 */

#include <malloc.h> /* smemalign */
#include <oskit/types.h>
#include <oskit/clientos.h>
#include <oskit/dev/linux.h>
#include <oskit/dev/osenv.h>
#include <oskit/dev/entropy.h>
#include <oskit/c/string.h>
#include <oskit/startup.h>
#include <stdio.h>
#include <stdlib.h>
#include <assert.h>

extern oskit_error_t (*_init_devices_entropy)(void);

int
main(int argc, char **argv)
{
    oskit_osenv_t *osenv;
    int rc = 0;

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
     * Init for entropy device(s).
     */
    _init_devices_entropy = oskit_linux_init_entropy;
    (*_init_devices_entropy)();
    
    /*
     * Set in ta' Probin'
     */
    oskit_dev_probe();

	
    /*============================================================
      
      The first bunch of tests exercise the bottom half of the
      entropy pool as its used from a device standpoint.  We get
      it from the device registry and run through each of the
      interfaces.

      The second bunch exercise the top half of the entropy pool.

      ============================================================ */
    
    {
	int n_entropy_dev;
	oskit_entropy_t **entropy_aref;
	
	/* Lookup the entropy interface via the device registry...
	 */
	n_entropy_dev = osenv_device_lookup(&oskit_entropy_iid,
					    (void***)&entropy_aref);
	    
	osenv_log(OSENV_LOG_INFO,
		  "DLD: found %d entropy device(s):\n", n_entropy_dev);
	{
	    int i;
	    for (i = 0; i < n_entropy_dev; i++)
	    {
		osenv_log(OSENV_LOG_INFO,
			  "DLD:   entropy dev %d at %p\n", i, entropy_aref[i]);
	    
		/* Test the iunknown interface...
		 */	
		{
		    oskit_iunknown_t *iunknown;
		    oskit_entropy_t *ent;

		    oskit_entropy_query(entropy_aref[i],
					&oskit_iunknown_iid,
					(void**)&iunknown);
		    osenv_log(OSENV_LOG_INFO,
			      "DLD: iunknown is %p\n", iunknown);
		    assert((void*)(entropy_aref[i]) == (void*)iunknown);
		    
		    oskit_iunknown_query(iunknown,
					 &oskit_entropy_iid,
					 (void**)&ent);
		    osenv_log(OSENV_LOG_INFO,
			      "DLD: entropy from iunknown is %p\n", ent);
		    assert((void*)iunknown == (void*)ent);

		    oskit_entropy_release(ent);
		    oskit_iunknown_release(iunknown);
		}
	
		/*
		 * Test the device interface...
		 */
		{
		    oskit_device_t *device;
		    oskit_entropy_t *ent;
		    oskit_devinfo_t info;
		    oskit_driver_t *driver;

		    oskit_entropy_query(entropy_aref[i],
					&oskit_device_iid,
					(void**)&device);
		    osenv_log(OSENV_LOG_INFO, "DLD: device is %p\n", device);
		    assert((void*)(entropy_aref[i]) == (void*)device);
		    
		    oskit_device_query(device,
				       &oskit_entropy_iid,
				       (void**)&ent);
		    osenv_log(OSENV_LOG_INFO,
			      "DLD: entropy from device is %p\n", ent);
		    assert((void*)device == (void*)ent);

		    /* Test the getinfo call
		    */
		    oskit_device_getinfo(device, &info);
		    osenv_log(OSENV_LOG_INFO,
			      "DLD:        device info.name is: %s\n",
			      info.name);
		    osenv_log(OSENV_LOG_INFO,
			      "DLD: device info.description is: %s\n",
			      info.description);
		    osenv_log(OSENV_LOG_INFO,
			      "DLD:      device info.vendor is: %s\n",
			      info.vendor);
		    osenv_log(OSENV_LOG_INFO,
			      "DLD:     device info.version is: %s\n",
			      info.version);
		    /* Note: info has automatic storage class */

		    /* Test the getdriver call
		    */
		    oskit_device_getdriver(device, &driver);
		    oskit_driver_getinfo(driver, &info);
		    osenv_log(OSENV_LOG_INFO,
			      "DLD:        driver info.name is: %s\n",
			      info.name);
		    osenv_log(OSENV_LOG_INFO,
			      "DLD: driver info.description is: %s\n",
			      info.description);
		    osenv_log(OSENV_LOG_INFO,
			      "DLD:      driver info.vendor is: %s\n",
			      info.vendor);
		    osenv_log(OSENV_LOG_INFO,
			      "DLD:     driver info.version is: %s\n",
			      info.version);
		    
		    oskit_entropy_release(ent);
		    oskit_device_release(device);
		    oskit_driver_release(driver);
		}
		
		/*
		 * Test the driver interface...
		 */	
		{
		    oskit_driver_t *driver;
		    oskit_entropy_t *ent;
		    oskit_devinfo_t info;
		    
		    oskit_entropy_query(entropy_aref[i],
					&oskit_driver_iid,
					(void**)&driver);
		    osenv_log(OSENV_LOG_INFO, "DLD: driver is %p\n", driver);
		    assert((void*)(entropy_aref[i]) == (void*)driver);

		    oskit_driver_query(driver,
				       &oskit_entropy_iid,
				       (void**)&ent);
		    osenv_log(OSENV_LOG_INFO,
			      "DLD: entropy from driver is %p\n", ent);
		    assert((void*)driver == (void*)ent);
		    
		    oskit_driver_getinfo(driver, &info);
		    osenv_log(OSENV_LOG_INFO,
			      "DLD:        driver(2) info.name is: %s\n",
			      info.name);
		    osenv_log(OSENV_LOG_INFO,
			      "DLD: driver(2) info.description is: %s\n",
			      info.description);
		    osenv_log(OSENV_LOG_INFO,
			      "DLD:      driver(2) info.vendor is: %s\n",
			      info.vendor);
		    osenv_log(OSENV_LOG_INFO,
			      "DLD:     driver(2) info.version is: %s\n",
			      info.version);

		    oskit_entropy_release(ent);
		    oskit_driver_release(driver);
		}

		/*
		 * Test the entropy interface...
		 */	
		{
		    oskit_entropy_stats_t stats;
		    unsigned int n_calls_1;
		    unsigned int entropy_count_1;
		    unsigned int n_calls_2;
		    unsigned int entropy_count_2;
		    oskit_entropy_channel_t *channel;

		    /* Get stats */
		    
		    /* FIXME: I like this mem scheme better b/c there is not
		       a reference returned that the caller could corrupt.  But
		       I think the getinfo call is established already, so I
		       wonder if this shouldnt parrot that?
		    */
		    oskit_entropy_getstats(entropy_aref[i], &stats);
		    n_calls_1 = stats.n_calls;
		    entropy_count_1 = stats.entropy_count;
		    osenv_log(OSENV_LOG_INFO,
			      "DLD: stats n_calls is %d entropy_count is %d\n",
			      n_calls_1, entropy_count_1);
		    
		    oskit_entropy_getstats(entropy_aref[i], &stats);
		    n_calls_2 = stats.n_calls;
		    entropy_count_2 = stats.entropy_count;
		    osenv_log(OSENV_LOG_INFO,
			      "DLD: stats n_calls is %d entropy_count is %d\n",
			      n_calls_2, entropy_count_2);

		    assert(n_calls_1 == 1);
		    assert(n_calls_2 == n_calls_1 + 1);

		    /*
		      assert(entropy_count_1 == 0);
		      assert(entropy_count_1 == entropy_count_2);
		    */
		    
		    /* Attach & detach */

		    oskit_entropy_attach_source(entropy_aref[i],
						"unittest", 0, 0, &channel);
		    osenv_log(OSENV_LOG_INFO,
			      "DLD: entropy chan is %p\n", channel);
		    {
			/* Use the channel */
			unsigned int n_calls_1;
			unsigned int entropy_count_1;
			unsigned int n_calls_2;
			unsigned int entropy_count_2;

			/* Check out the query interface */
			{
			    oskit_iunknown_t *iunknown;
			    oskit_entropy_channel_query(channel,
							&oskit_iunknown_iid,
							(void**)&iunknown);
			    osenv_log(OSENV_LOG_INFO,
				      "DLD: entropy chan iunknown is %p\n",
				      iunknown);
			    oskit_iunknown_release(iunknown);
			}

			/* Get some starting statistics. */
			{
			    oskit_entropy_stats_t stats;
			    oskit_entropy_channel_get_stats(channel, &stats);
			    n_calls_1 = stats.n_calls;
			    entropy_count_1 = stats.entropy_count;
			    osenv_log(OSENV_LOG_INFO,
				      "DLD: (channel) stats n_calls is %d entropy_count is %d\n",
				      n_calls_1, entropy_count_1);
			}

			/* Add some entropy. */
			{
			    unsigned int foo = 42;
			    oskit_entropy_channel_add_data(channel,
							   (void*)&foo,
							   sizeof(foo), 3);
			}

			/* Get statistics to compare to starting statistics. */
			{
			    oskit_entropy_stats_t stats;
			    oskit_entropy_channel_get_stats(channel, &stats);
			    n_calls_2 = stats.n_calls;
			    entropy_count_2 = stats.entropy_count;
			    osenv_log(OSENV_LOG_INFO,
				      "DLD: (channel) stats n_calls is %d entropy_count is %d\n",
				      n_calls_2, entropy_count_2);
			}

			/* Make sure we get what we expect. */
			assert(n_calls_2 == n_calls_1 + 1);
			assert(entropy_count_2 > entropy_count_1);
		    }

		    oskit_entropy_channel_release(channel);		    
		}

		/*
		 * Use the non-blocking entropy stream.
		 */
		{
		    oskit_stream_t *stream;
		    oskit_u32_t actual_num_bytes;
		    unsigned char entropy_bytes[2];
		    oskit_entropy_stats_t stats;
		    unsigned int n_calls_1;
		    unsigned int entropy_count_1;
		    unsigned int n_calls_2;
		    unsigned int entropy_count_2;
		    unsigned int n_calls_3;
		    unsigned int entropy_count_3;

		    oskit_entropy_getstats(entropy_aref[i], &stats);
		    n_calls_1 = stats.n_calls;
		    entropy_count_1 = stats.entropy_count;
		    osenv_log(OSENV_LOG_INFO,
			      "DLD: stats n_calls is %d entropy_count is %d\n",
			      n_calls_1, entropy_count_1);
		    
		    oskit_entropy_open_nonblocking(entropy_aref[i], &stream);
		    oskit_stream_write(stream,
				       (const void*)"Anya V. Skaletskaya",
				       19, &actual_num_bytes);
		    osenv_log(OSENV_LOG_INFO,
			      "DLD: used oskit_stream_write from open_nonblocking\n");
		    
		    oskit_entropy_getstats(entropy_aref[i], &stats);
		    n_calls_2 = stats.n_calls;
		    entropy_count_2 = stats.entropy_count;
		    osenv_log(OSENV_LOG_INFO,
			      "DLD: stats n_calls is %d entropy_count is %d\n",
			      n_calls_2, entropy_count_2);

		    oskit_stream_read(stream, (void*)entropy_bytes,
				      2, &actual_num_bytes);
		    osenv_log(OSENV_LOG_INFO,
			      "DLD: read entropy bytes %x and %x\n",
			      entropy_bytes[0], entropy_bytes[1]);
		    
		    oskit_entropy_getstats(entropy_aref[i], &stats);
		    n_calls_3 = stats.n_calls;
		    entropy_count_3 = stats.entropy_count;
		    osenv_log(OSENV_LOG_INFO,
			      "DLD: stats n_calls is %d entropy_count is %d\n",
			      n_calls_3, entropy_count_3);
		    
		    oskit_stream_release(stream);
		}

		/*
		 * Open the good stream.
		 */
		{
		    oskit_stream_t *stream;
		    oskit_asyncio_t *asyncio;
		    oskit_u32_t actual_num_bytes;
		    oskit_error_t mask;
		    oskit_error_t readable;
		    oskit_entropy_stats_t stats;
		    unsigned int n_calls;
		    unsigned int entropy_count;
		    
		    oskit_entropy_open_good(entropy_aref[i],
					    &stream, &asyncio);
		    mask = oskit_asyncio_poll(asyncio);
		    osenv_log(OSENV_LOG_INFO,
			      "DLD: poll returned mask (or error) %x (%d)\n",
			      mask, mask);

		    readable = oskit_asyncio_readable(asyncio);
		    osenv_log(OSENV_LOG_INFO, "DLD: readable returned %d\n",
			      readable);

		    oskit_stream_write(stream,
				       (const void*)"ayakstelaks ayna",
				       16, &actual_num_bytes);
		    osenv_log(OSENV_LOG_INFO,
			      "DLD: (1) used oskit_stream_write from open_good\n");

		    readable = oskit_asyncio_readable(asyncio);
		    osenv_log(OSENV_LOG_INFO,
			      "DLD: (1) readable returned %d\n", readable);
		    
		    oskit_stream_write(stream,
				       (const void*)"avokaluk aytak",
				       14, &actual_num_bytes);
		    osenv_log(OSENV_LOG_INFO,
			      "DLD: (2) used oskit_stream_write from open_good\n");

		    readable = oskit_asyncio_readable(asyncio);
		    osenv_log(OSENV_LOG_INFO,
			      "DLD: (2) readable returned %d\n", readable);
		    
		    oskit_entropy_getstats(entropy_aref[i], &stats);
		    n_calls = stats.n_calls;
		    entropy_count = stats.entropy_count;
		    osenv_log(OSENV_LOG_INFO,
			      "DLD: stats n_calls is %d entropy_count is %d\n",
			      n_calls, entropy_count);

		    oskit_stream_release(stream);
		    oskit_asyncio_release(asyncio);
		}
	    }
	}
    }
    
    oskit_dump_drivers();
    
    return rc;
}


/* End of rndunittest.c */
