/* Martin Vit support@voipmonitor.org
 * This program is free software, distributed under the terms of
 * the GNU General Public License Version 2.
*/

#ifndef SNIFF_H
#define SNIFF_H

#include <queue>
#include <map>
#include <semaphore.h>

#include "rqueue.h"
#include "voipmonitor.h"
#include "calltable.h"
#include "pcap_queue_block.h"
#include "fraud.h"

#ifdef FREEBSD
#include <machine/endian.h>
#else
#include "asm/byteorder.h"
#endif

#define RTP_FIXED_HEADERLEN 12

#define IP_DF           0x4000          /* Flag: "Don't Fragment"       */
#define IP_MF           0x2000          /* Flag: "More Fragments"       */
#define IP_OFFSET       0x1FFF          /* "Fragment Offset" part       */

struct iphdr2 {
#if __BYTE_ORDER == __LITTLE_ENDIAN
	unsigned int ihl:4;
	unsigned int version:4;
#elif __BYTE_ORDER == __BIG_ENDIAN
	unsigned int version:4;
	unsigned int ihl:4;
#else
# error "Please fix <bits/endian.h>"
#endif 
	u_int8_t tos;
	u_int16_t tot_len;
	u_int16_t id;
	u_int16_t frag_off;
	u_int8_t ttl;
	u_int8_t protocol;
	u_int16_t check;
	u_int32_t saddr;
	u_int32_t daddr;
	/*The options start here. */
#ifdef PACKED
} __attribute__((packed));
#else
};
#endif

void *rtp_read_thread_func(void *arg);
void add_rtp_read_thread();
void set_remove_rtp_read_thread();
int get_index_rtp_read_thread_min_size();
int get_index_rtp_read_thread_min_calls(bool incCalls);
double get_rtp_sum_cpu_usage(double *max = NULL);
string get_rtp_threads_cpu_usage(bool callPstat);

#ifdef HAS_NIDS
void readdump_libnids(pcap_t *handle);
#endif
void readdump_libpcap(pcap_t *handle, u_int16_t handle_index);


typedef std::map<in_addr_t, in_addr_t> nat_aliases_t; //!< 


/* this is copied from libpcap sll.h header file, which is not included in debian distribution */
#define SLL_ADDRLEN       8               /* length of address field */
struct sll_header {
	u_int16_t sll_pkttype;          /* packet type */
	u_int16_t sll_hatype;           /* link-layer address type */
	u_int16_t sll_halen;            /* link-layer address length */
	u_int8_t sll_addr[SLL_ADDRLEN]; /* link-layer address */
	u_int16_t sll_protocol;         /* protocol */
};

struct udphdr2 {
        uint16_t        source;
        uint16_t        dest;
        uint16_t        len;
        uint16_t        check;
};

struct packet_s {
	#if USE_PACKET_NUMBER
	u_int64_t packet_number;
	#endif
	u_int32_t saddr;
	u_int32_t daddr; 
	u_int32_t datalen; 
	pcap_pkthdr *header_pt; 
	const u_char *packet; 
	pcap_block_store *block_store; 
	u_int32_t block_store_index; 
	u_int32_t sensor_ip;
	u_int16_t handle_index; 
	u_int16_t dlt; 
	u_int16_t sensor_id_u;
	u_int16_t source; 
	u_int16_t dest;
	u_int16_t dataoffset;
	u_int16_t header_ip_offset;
	unsigned int istcp : 2;
	bool is_ssl : 1;
	bool is_skinny : 1;
	bool is_need_sip_process : 1;
	bool _blockstore_lock : 1;
	bool _packet_alloc : 1;
	inline char *data_() {
		return((char*)(packet + dataoffset));
	}
	inline iphdr2 *header_ip_() {
		return((iphdr2*)(packet + header_ip_offset));
	}
	inline int sensor_id_() {
		return(sensor_id_u == 0xFFFF ? -1 : sensor_id_u);
	}
	inline packet_s() {
		init();
	}
	inline void init() {
		_blockstore_lock = false;
		_packet_alloc = false;
	}
	inline void blockstore_lock() {
		if(!_blockstore_lock && block_store) {
			block_store->lock_packet(block_store_index);
			_blockstore_lock = true;
		}
	}
	inline void blockstore_setlock() {
		if(block_store) {
			_blockstore_lock = true;
		}
	}
	inline void blockstore_relock(int check_limit_lock = 0) {
		if(_blockstore_lock && block_store) {
			block_store->lock_packet(block_store_index, check_limit_lock);
		}
	}
	inline void blockstore_unlock() {
		if(_blockstore_lock && block_store && !is_terminating()) {
			block_store->unlock_packet(block_store_index);
			_blockstore_lock = false;
		}
	}
	inline void blockstore_clear() {
		block_store = NULL; 
		block_store_index = 0; 
		_blockstore_lock = false;
	}
	inline void packetdelete() {
		if(_packet_alloc) {
			delete header_pt;
			delete [] packet;
			_packet_alloc = NULL;
		}
	}
};

struct packet_s_plus_pointer : public packet_s {
	void *pointer[2];
};

struct packet_s_process_rtp_call_info {
	Call *call;
	bool iscaller;
	bool is_rtcp;
	s_sdp_flags sdp_flags;
	bool use_sync;
};

struct packet_s_process_0 : public packet_s {
	char *data;
	struct iphdr2 *header_ip; 
	cHeapItemsPointerStack *stack;
	int isSip;
	bool isSkinny;
	unsigned int hash[2];
	packet_s_process_rtp_call_info call_info[20];
	int call_info_length;
	bool call_info_find_by_dest;
	volatile int hash_find_flag;
	inline packet_s_process_0() {
		init();
		init2();
	}
	inline void init() {
		packet_s::init();
		stack = NULL;
	}
	inline void init2() {
		data = (char*)(packet + dataoffset);
		header_ip = (iphdr2*)(packet + header_ip_offset);
		isSip = -1;
		isSkinny = false;
		hash[0] = 0;
		hash[1] = 0;
		call_info_length = -1;
	}
};

struct packet_s_process : public packet_s_process_0 {
	ParsePacket::ppContentsX parseContents;
	u_int32_t sipDataOffset;
	u_int32_t sipDataLen;
	char callid_short[128];
	string callid_long;
	int sip_method;
	bool is_register;
	bool sip_response;
	int lastSIPresponseNum;
	char lastSIPresponse[128];
	bool call_cancel_lsr487;
	Call *call;
	int merged;
	Call *call_created;
	bool _getCallID;
	bool _getSipMethod;
	bool _getLastSipResponse;
	bool _findCall;
	bool _createCall;
	inline packet_s_process() {
		init();
		init2();
	}
	inline void init() {
		packet_s_process_0::init();
	}
	inline void init2() {
		packet_s_process_0::init2();
		sipDataOffset = 0;
		sipDataLen = 0;
		callid_short[0] = 0;
		callid_long.resize(0);
		sip_method = -1;
		is_register = false;
		sip_response = false;
		lastSIPresponseNum = -1;
		lastSIPresponse[0] = 0;
		call_cancel_lsr487 = false;
		call = NULL;
		merged = 0;
		call_created = NULL;
		_getCallID = false;
		_getSipMethod = false;
		_getLastSipResponse = false;
		_findCall = false;
		_createCall = false;
	}
	void set_callid(char *callid, unsigned callid_length = 0) {
		if(!callid_length) {
			callid_length = strlen(callid);
		}
		if(callid_length < sizeof(callid_short)) {
			strncpy(callid_short, callid, callid_length);
			callid_short[callid_length] = 0;
		} else {
			callid_long = string(callid, callid_length);
		}
	}
	inline char *get_callid() {
		return(callid_long.size() ? (char*)callid_long.c_str() : callid_short);
	}
	inline void new_alloc_packet_header() {
		pcap_pkthdr *header_pt_new = new FILE_LINE pcap_pkthdr;
		u_char *packet_new = new FILE_LINE u_char[header_pt->caplen];
		*header_pt_new = *header_pt;
		memcpy(packet_new, packet, header_pt->caplen);
		header_pt = header_pt_new;
		packet = packet_new;
		data = (char*)(packet + dataoffset);
		header_ip = (iphdr2*)(packet + header_ip_offset);
	}
};


void save_packet(Call *call, packet_s *packetS, int type);
void save_packet(Call *call, packet_s_process *packetS, int type);


typedef struct {
	Call *call;
	packet_s packet;
	char iscaller;
	char find_by_dest;
	char is_rtcp;
	char save_packet;
} rtp_packet_pcap_queue;

struct rtp_read_thread {
	rtp_read_thread()  {
		this->rtpp_queue = NULL;
		this->rtpp_queue_quick = NULL;
		this->rtpp_queue_quick_boost = NULL;
		this->calls = 0;
	}
	pthread_t thread;	       // ID of worker storing CDR thread 
	volatile int threadId;
	rqueue<rtp_packet_pcap_queue> *rtpp_queue;
	rqueue_quick<rtp_packet_pcap_queue> *rtpp_queue_quick;
	rqueue_quick_boost<rtp_packet_pcap_queue> *rtpp_queue_quick_boost;
	pstat_data threadPstatData[2];
	volatile bool remove_flag;
	u_int32_t last_use_time_s;
	volatile u_int32_t calls;
};

#define MAXLIVEFILTERS 10
#define MAXLIVEFILTERSCHARS 64

typedef struct livesnifferfilter_s {
	struct state_s {
		bool all_saddr;
		bool all_daddr;
		bool all_bothaddr;
		bool all_addr;
		bool all_srcnum;
		bool all_dstnum;
		bool all_bothnum;
		bool all_num;
                bool all_fromhstr;
                bool all_tohstr;
                bool all_bothhstr;
                bool all_hstr;
		bool all_siptypes;
		bool all_all;
	};
        unsigned int lv_saddr[MAXLIVEFILTERS];
        unsigned int lv_daddr[MAXLIVEFILTERS];
	unsigned int lv_bothaddr[MAXLIVEFILTERS];
        char lv_srcnum[MAXLIVEFILTERS][MAXLIVEFILTERSCHARS];
        char lv_dstnum[MAXLIVEFILTERS][MAXLIVEFILTERSCHARS];
	char lv_bothnum[MAXLIVEFILTERS][MAXLIVEFILTERSCHARS];
        char lv_fromhstr[MAXLIVEFILTERS][MAXLIVEFILTERSCHARS];
        char lv_tohstr[MAXLIVEFILTERS][MAXLIVEFILTERSCHARS];
        char lv_bothhstr[MAXLIVEFILTERS][MAXLIVEFILTERSCHARS];
	unsigned char lv_siptypes[MAXLIVEFILTERS];
        int uid;
        time_t created_at;
	state_s state;
	void updateState();
	string getStringState();
} livesnifferfilter_t;

struct livesnifferfilter_use_siptypes_s {
	bool u_invite;
	bool u_register;
	bool u_options;
	bool u_subscribe;
	bool u_message;
	bool u_notify;
};

struct gre_hdr {
#if defined(__LITTLE_ENDIAN_BITFIELD)
#ifdef FREEBSD
        u_int16_t rec:3,
#else
        __u16   rec:3,
#endif
                srr:1,
                seq:1,
                key:1,
                routing:1,
                csum:1,
                version:3,
                reserved:4,
                ack:1;
#elif defined(__BIG_ENDIAN_BITFIELD)
#ifdef FREEBSD
        u_int16_t   csum:1,
#else
	__u16   csum:1,
#endif
                routing:1,
                key:1,
                seq:1,
                srr:1,
                rec:3,
                ack:1,
                reserved:4,
                version:3;
#else
#error "Adjust your <asm/byteorder.h> defines"
#endif
#ifdef FREEBSD
        u_int16_t  protocol;
#else
	__be16	protocol;
#endif
};


void process_packet__push_batch();


#define enable_save_sip(call)		(call->flags & FLAG_SAVESIP)
#define enable_save_register(call)	(call->flags & FLAG_SAVEREGISTER)
#define enable_save_rtcp(call)		((call->flags & FLAG_SAVERTCP) || (call->isfax && opt_saveudptl))
#define enable_save_rtp(call)		((call->flags & (FLAG_SAVERTP | FLAG_SAVERTPHEADER)) || (call->isfax && opt_saveudptl) || opt_saverfc2833)
#define enable_save_sip_rtp(call)	(enable_save_sip(call) || enable_save_rtp(call))
#define enable_save_packet(call)	(enable_save_sip(call) || enable_save_register(call) || enable_save_rtp(call))
#define enable_save_audio(call)		((call->flags & FLAG_SAVEAUDIO) || opt_savewav_force)
#define enable_save_sip_rtp_audio(call)	(enable_save_sip_rtp(call) || enable_save_audio(call))
#define enable_save_any(call)		(enable_save_packet(call) || enable_save_audio(call))


#endif
