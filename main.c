#include <rte_common.h>
#include <rte_eal.h>
#include <rte_errno.h>
#include <rte_ethdev.h>
#include <rte_ether.h>
#include <rte_launch.h>
#include <rte_lcore.h>
#include <rte_mbuf.h>
#include <rte_mbuf_core.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>

/* main.c: program launcher */

// Port related constants
#define RX_RING_DESC_NUM 128
#define RX_QUEUE_NUM 1
#define TX_RING_DESC_NUM 512
#define TX_QUEUE_NUM 1
#define BURST_SIZE 32

// MBUF constants
#define NUM_MBUFS 4095
#define MBUF_CACHE_SIZE 256

struct rte_mempool *mbuf_pool;


/*
 * Initializes a given port using global settings and with the RX buffers
 * coming from the mbuf_pool passed as a parameter.
 */

/* Main functional part of port initialization. 8< */
static inline int
port_init(
	uint16_t port_id, 
	struct rte_mempool *mbuf_pool, 
	uint16_t rx_ring_desc_num, 
	uint16_t tx_ring_desc_num,
	uint16_t rx_queue_num,
	uint16_t tx_queue_num
) {
	/* Variables */
	struct rte_eth_conf port_conf;
	struct rte_eth_dev_info dev_info;
	struct rte_eth_txconf txconf;
	struct rte_eth_rxconf rxconf;

	int retval;
	uint16_t queue_counter; 

	/* Validate port */
	retval = rte_eth_dev_is_valid_port(port_id); 
	if (retval != 1) {
		printf("rte_eth_dev_is_valid_port(port %" PRIu16 ") failed: %s (error %d)\n",
             port_id, rte_strerror(-retval), retval);
		return retval;
	}

	/* Explicitly fill port configuration struct with zeroes */
	memset(&port_conf, 0, sizeof(struct rte_eth_conf));

	/* Get device information */
	retval = rte_eth_dev_info_get(port_id, &dev_info);
	if (retval != 0) {
		printf("rte_eth_dev_info_get(port %" PRIu16 ") failed: %s (error %d)\n",
             port_id, rte_strerror(-retval), retval);
		return retval;
	} 

	/* Try to get does TX_FAST_FREE enabled in device */
	if (dev_info.tx_offload_capa & 
		RTE_ETH_TX_OFFLOAD_MBUF_FAST_FREE) 
	{
		port_conf.txmode.offloads |= 
		RTE_ETH_TX_OFFLOAD_MBUF_FAST_FREE;
	}

	/* Configure Ethernet device */
	retval = rte_eth_dev_configure(
		port_id, 
		rx_queue_num, 
		tx_queue_num, 
		 &port_conf
	);

	if (retval != 0) {
		printf("rte_eth_dev_configure(port %" PRIu16 ") failed: %s (error %d)\n",
             port_id, rte_strerror(-retval), retval);
		return retval;
	}

	retval = rte_eth_dev_adjust_nb_rx_tx_desc(
		port_id, 
		&rx_ring_desc_num,
		&tx_ring_desc_num
	);

	if (retval != 0) {
		printf(
			"rte_eth_dev_adjust_nb_rx_tx_desc(\
			port %" PRIu16 ", \
			nb_rx_desc: %" PRIu16 ", \
			nb_tx_desc: %" PRIu16" \
			) failed: %s (error %d)\n",
             port_id, 
			 rx_ring_desc_num,
			 tx_ring_desc_num, 
			 rte_strerror(-retval), 
			 retval
		);
		return retval;
	}


	/* Allocate RX queues for Ethernet Port */
	rxconf = dev_info.default_rxconf;
	rxconf.offloads = port_conf.rxmode.offloads;
	for (queue_counter = 0; queue_counter < rx_queue_num; queue_counter++) {
		retval = rte_eth_rx_queue_setup(
			port_id, 
			queue_counter, 
			rx_ring_desc_num, 
			rte_socket_id(), 
			&rxconf,
			mbuf_pool	
		);
		if (retval != 0) {
			printf(
				"rte_eth_rx_queue_setup(\
				port %" PRIu16 ", \
				rx_queue_id : %" PRIu16 ", \
				nb_rx_desc : %" PRIu16 ", \
				socket_id: %" PRIu16" \
				) failed: %s (error %d)\n",
				 port_id, 
				 queue_counter,
				 rx_ring_desc_num,
				 rte_socket_id(),
				 rte_strerror(-rte_errno), 
				 retval
			);
			return retval;
		}
	}

	/* Allocate TX queues for Ethernet port */
	txconf = dev_info.default_txconf;
	txconf.offloads = port_conf.txmode.offloads;
	for (queue_counter = 0; queue_counter < tx_queue_num; queue_counter++) {
		retval = rte_eth_tx_queue_setup(
			port_id, 
			queue_counter, 
			tx_ring_desc_num,
			rte_socket_id(), 
			&txconf
		);
		if (retval != 0) {
			printf(
				"rte_eth_tx_queue_setup(\
				port %" PRIu16 ", \
				tx_queue_id : %" PRIu16 ", \
				nb_tx_desc : %" PRIu16 ", \
				socket_id: %" PRIu16" \
				) failed: %s (error %d)\n",
				 port_id, 
				 queue_counter,
				 tx_ring_desc_num,
				 rte_socket_id(),
				 rte_strerror(-rte_errno), 
				 retval
			);
			return retval;
		}

	}

	/* Start Ethernet port */
	retval = rte_eth_dev_start(port_id);
	if ( retval < 0 ) {
		printf(
			"rte_eth_dev_start(\
			port %" PRIu16 ") failed: %s (error %d)\n",
			 port_id, 
			 rte_strerror(-retval), 
			 retval
		);
		return retval;
	}


	/* Enable RX in promiscuous mode for the Ethernet device */
	retval = rte_eth_promiscuous_enable(port_id);
	if (retval != 0) {
		printf(
			"rte_eth_promiscuous_enable(\
			port %" PRIu16 ") failed: %s (error %d)\n",
			 port_id, 
			 rte_strerror(-retval), 
			 retval
		);
	}

	/* Log the port */
	struct rte_ether_addr eth_addr; 
	retval = rte_eth_macaddr_get(port_id, &eth_addr);
	if (retval != 0) {
		printf(
			"rte_eth_macaddr_get(\
			port %" PRIu16 ") failed: %s (error %d)\n",
			 port_id, 
			 rte_strerror(-retval), 
			 retval
		);
	} else {
		printf("Port %" PRIu16 " MAC: %02" PRIx8 " %02" PRIx8 " %02" PRIx8
		   " %02" PRIx8 " %02" PRIx8 " %02" PRIx8 " initialized, promiscious mode enabled\n",
		port_id, RTE_ETHER_ADDR_BYTES(&eth_addr));
	}

	return EXIT_SUCCESS;
}





static int
lcore_main() {
	uint16_t port;

	/*
	 * Chech that the port is on the same NUMA node as the pooling thread
	 */
	RTE_ETH_FOREACH_DEV(port)
		if (rte_eth_dev_socket_id(port) >= 0 && 
			rte_eth_dev_socket_id(port) !=
			(int)rte_socket_id()
		) {
			printf("WARNING, port %u is on remote NUMA node to "
			"polling thread.\n\tPerformance will "
			"not be optimal.\n", port);		
		}

	printf("\nCore %u forwarding packets. [Ctrl+C to quit]\n",
			rte_lcore_id());

	/* Main loop */
	for (;;) {
				RTE_ETH_FOREACH_DEV(port) {

			/* Get burst of RX packets, from first port of pair. */
			struct rte_mbuf *bufs[BURST_SIZE];
			const uint16_t nb_rx = rte_eth_rx_burst(0, 0,
					bufs, BURST_SIZE);

			if (unlikely(nb_rx == 0))
				continue;

			/* Send burst of TX packets, to second port of pair. */
			const uint16_t nb_tx = rte_eth_tx_burst(1, 0,
					bufs, nb_rx);

			/* Free any unsent packets. */
			if (unlikely(nb_tx < nb_rx)) {
				uint16_t buf;
				for (buf = nb_tx; buf < nb_rx; buf++)
					rte_pktmbuf_free(bufs[buf]);
			}
		}
	}
	return 0;
}


int
main(int argc, char **argv) {
	unsigned ports_num;

	int retval; 
	uint16_t portid;



	/* Environment Abstraction Layer initialization */
	if ((retval = rte_eal_init(argc, argv)) < 0) {
		rte_exit(EXIT_FAILURE, "Failed init EAL: \
			 failed: %s\
			 (error %d)",
			 rte_strerror(-retval), 
			 retval
		);
	}
	
	/* Move arguments pointer and reduce number of arguments */
	argc -= retval;
	argv += retval;

	/* Validate number of ports */
	ports_num = rte_eth_dev_count_avail();
	if (ports_num < 2 || (ports_num & 1) == 1) {
		rte_exit(EXIT_FAILURE, "Error: odd number of ports\n");
	}

	/* Allocate mempool to hold the mbuffers */
	mbuf_pool = rte_pktmbuf_pool_create(
		"MBUF_POOL",
		NUM_MBUFS,
		MBUF_CACHE_SIZE,
		0,
		RTE_MBUF_DEFAULT_BUF_SIZE,
		rte_socket_id()
	);

	if (mbuf_pool == NULL) {
		perror("Cannot create mbuf pool");
		return retval;
	}

	/* Initialize all ports */
	RTE_ETH_FOREACH_DEV(portid)
		if (port_init(portid, mbuf_pool,
			 RX_RING_DESC_NUM,
			  TX_RING_DESC_NUM,
			   RX_QUEUE_NUM,
			    TX_QUEUE_NUM) != 0
			) 
			rte_exit(
			EXIT_FAILURE,
			"Cannot init port %" PRIu16"\n",
				portid
			);

	/* Launch lcore_main on all lcores */
	// if ((retval= rte_eal_mp_remote_launch(lcore_main, NULL, CALL_MAIN )) != 0 ) {
	// 	rte_exit(EXIT_FAILURE, "Error during lcores launch\n");
	// }

	/* Launch lcore_main on one lcore */
	lcore_main();

	return 0;
}