#ifndef OFFLOAD_NOOP
#include "offloadtask.hh"

static size_t serialize_ipv4(uint8_t *scif_payload, struct packet **pkts, unsigned n) {
    for ( unsigned i = 0; i < n; i++ ) {
        struct packet *p = pkts[i];
        uint8_t *pbuf = rte_pktmbuf_mtod(p->mbuf, uint8_t *);
        memcpy((void *)scif_payload, (void *)pbuf, sizeof(struct ether_hdr));
        scif_payload += sizeof(struct ether_hdr) + 2; // vector loads need to be 4-byte aligned
        pbuf += sizeof(struct ether_hdr);
        memcpy((void *)scif_payload, (void *)pbuf, sizeof(struct iphdr));
        scif_payload += sizeof(struct iphdr);
    }
    return (n * PER_PACKET_OFFLOAD_SIZE_IPV4);
}

static size_t serialize_ipv6(uint8_t *scif_payload, struct packet **pkts, unsigned n) {
	for ( unsigned i = 0; i < n; i++ ) {
        struct packet *p = pkts[i];
        uint8_t *pbuf = rte_pktmbuf_mtod(p->mbuf, uint8_t *);
        memcpy((void *)scif_payload, (void *)pbuf, sizeof(struct ether_hdr));
        scif_payload += sizeof(struct ether_hdr) + 2; // vector loads need to be 4-byte aligned
        pbuf += sizeof(struct ether_hdr);
        memcpy((void *)scif_payload, (void *)pbuf, sizeof(struct ipv6_hdr));
        scif_payload += sizeof(struct ipv6_hdr);
    }
    return (n * PER_PACKET_OFFLOAD_SIZE_IPV6);

    return 0;
}

static size_t serialize_ipsec(uint8_t *scif_payload, struct packet **pkts, unsigned n) {
    return 0;
}

static size_t serialize_ids(uint8_t *scif_payload, struct packet **pkts, unsigned n) {
    return 0;
}

size_t offload_task::serialize() {
    uint8_t *payload_buf = serialized + sizeof(struct taskitem);
    size_t ret = sizeof(struct taskitem);
    switch ( apptype ) {
        case APP_IPV4:
            ret += serialize_ipv4(payload_buf, pkts, count);
            break;
        case APP_IPV6:
            ret += serialize_ipv6(payload_buf, pkts, count);
            break;
        case APP_IPSEC:
            ret += serialize_ipsec(payload_buf, pkts, count);
            break;
        case APP_IDS:
            ret += serialize_ids(payload_buf, pkts, count);
            break;
        default:
            ret = SERIALIZED_LEN_INVALID;
    }
    b_is_serialized = true;
    serialized_len = ret;
    return ret;
}


pktprocess_result_t offload_task::offload_postproc(int index) {
#ifndef OFFLOAD_NOOP
    pktprocess_result_t result;
    uint8_t *pbuf = rte_pktmbuf_mtod(pkts[index]->mbuf, uint8_t *);
	uint8_t *res_base;
	int32_t res;
	struct iphdr *iph = (struct iphdr *)(((struct ether_hdr *) pbuf) + 1);
    assert ( index >= 0 && index < (int) count );
    switch(apptype) {
        case APP_IPV4_LOOKUP:
        case APP_IPV4:
			res_base = resultbuf + (PER_PACKET_RESULT_SIZE_IPV4 * index);
			res = *((int32_t *) res_base);
			res_base += sizeof(int32_t);
			result = (pktprocess_result_t) res;
			if ( result == CONTINUE ) {
				iph->ttl = *((uint16_t *) res_base);
				res_base += sizeof(uint16_t);
				iph->check = *((uint16_t *) res_base);
			}
            return result;
		case APP_IPV6:
			//struct ipv6hdr *ipv6 = (struct ipv6hdr *)(((struct ether_hdr *) pbuf) + 1);
			res = ((int32_t *) resultbuf)[index];
			result = (pktprocess_result_t) res;
			return result;
        default:
            return DROP;
    }
#else
    return CONTINUE;
#endif
}
#endif /* !OFFLOAD_NOOP */
