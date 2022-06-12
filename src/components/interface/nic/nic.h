#ifndef NIC_H
#define NIC_H

#include <cos_types.h>
#include <cos_component.h>
#include <cos_stubs.h>
#include <shm_bm.h>

int nic_send_packet(shm_bm_objid_t pktid, u16_t pkt_size);
int nic_bind_port(u32_t ip_addr, u16_t port);

/*
 * caller will be suspended until there is a packet for this thread,
 * dpdk will then copy the packet to this shmem region specified by the shmid
 * and return its shmem objectid.
 */
shm_bm_objid_t nic_get_a_packet(cbuf_t shmid);
#endif /* NIC_H */
