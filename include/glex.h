/*
 * Copyright (C) 2012
 * 	Computer Department,
 * 	National University of Defense Technology (NUDT)
 *
 * 	Xie Min (xiemin@nudt.edu.cn)
 *
 */

/* GLEX user interface */

#ifndef	_GLEX_H_
#define	_GLEX_H_

#include <stdint.h>
#include <pthread.h>
#include <endian.h>

#ifdef __cplusplus
#  define BEGIN_C_DECLS extern "C" {
#  define END_C_DECLS   }
#else /* !__cplusplus */
#  define BEGIN_C_DECLS
#  define END_C_DECLS
#endif /* __cplusplus */

BEGIN_C_DECLS

/* return value of all glex functions */
typedef enum glex_ret {
	GLEX_SUCCESS = 0,
	GLEX_SYS_ERR,
	GLEX_INVALID_PARAM,
	GLEX_NO_EP_RESOURCE,
	GLEX_NO_MEM_RESOURCE,
	GLEX_BUSY,
	GLEX_NO_MP,
	GLEX_NO_EVENT,
	GLEX_INTR,
	GLEX_ACCESS_ERR,
	GLEX_NOT_IMPLEMENTED
} glex_ret_t;

typedef struct glex_device {
	pthread_mutex_t	mutex;
} glex_device_t;

typedef glex_device_t * glex_device_handle_t;

typedef struct glex_device_attr {
	uint32_t	nic_id;			/* NIC ID, initialized by NIO hardware */
	uint32_t	max_fast_ep_num;	/* max number of fast (PIO) endpoint */
	uint32_t	max_ep_num;		/* max number of normal (DMA) endpoint */
	uint32_t	max_emulate_ep_num;	/* max number of emulated endpoint */
	uint32_t	mmt_units;		/* capacity of memory management table */
	uint32_t	att_units;		/* capacity of Address Translation Table */
	uint32_t	max_imm_mp_data_len;	/* maximum data size of immediate MP */
	uint32_t	max_mp_data_len;	/* maximum data size of normal MP */
	uint32_t	max_imm_rdma_data_len;	/* maximum data size of immediate RDMA */
	uint32_t	max_rdma_data_len;
	uint32_t	def_ep_dq_capacity;
	uint32_t	def_ep_eq_capacity;
	uint32_t	def_ep_mpq_capacity;	/* units in immediate MP */
	uint32_t	max_ep_dq_capacity;
	uint32_t	max_ep_eq_capacity;
	uint32_t	max_ep_mpq_capacity;	/* units in immediate MP */
	uint32_t	max_ep_hc_eq_capacity;
	uint32_t	max_ep_hc_mpq_capacity;
} glex_device_attr_t;

struct glex_imm_mp_req;
struct glex_imm_rdma_req;
struct glex_rdma_req;

/* different operations for different endpoints */
typedef struct glex_ep {
	glex_ret_t	(*send_imm_mp)(struct glex_ep *,
				       struct glex_imm_mp_req *,
				       struct glex_imm_mp_req **);
	glex_ret_t	(*imm_rdma)(struct glex_ep *,
				    struct glex_imm_rdma_req *,
				    struct glex_imm_rdma_req **);
	glex_ret_t	(*rdma)(struct glex_ep *,
				struct glex_rdma_req *,
				struct glex_rdma_req **);
} glex_ep_t;

typedef glex_ep_t * glex_ep_handle_t;

enum glex_ep_type {
	GLEX_EP_TYPE_FAST	= 0,	/* PIO endpoint */
	GLEX_EP_TYPE_NORMAL	= 1,	/* DMA endpoint */
	GLEX_EP_TYPE_EMULATE	= 2,
	GLEX_EP_TYPE_MANAGEMENT	= 3
};

enum glex_mpq_type {
	GLEX_MPQ_TYPE_NORMAL		= 0,	/* get_free_pages -> mmap */
	GLEX_MPQ_TYPE_HIGH_CAPACITY	= 1,	/* vmalloc -> ATT -> mmap */
	GLEX_MPQ_TYPE_IOMMU		= 2
};

enum glex_eq_type {
	GLEX_EQ_TYPE_NORMAL		= 0,	/* get_free_pages -> mmap */
	GLEX_EQ_TYPE_HIGH_CAPACITY	= 1,	/* vmalloc -> ATT -> mmap */
	GLEX_EQ_TYPE_IOMMU		= 2
};

typedef union {
	struct {
#if __BYTE_ORDER == __LITTLE_ENDIAN
		uint16_t ep_num;	/* maximum 64K endpoints */
		uint16_t resv;		/* reserved, for emulate VP */
		uint32_t nic_id;
#else
		uint32_t nic_id;
		uint16_t resv;
		uint16_t ep_num;
#endif
	};
	uint64_t v;
} glex_ep_addr_t;

struct glex_ep_attr {
	enum glex_ep_type	type;
	enum glex_mpq_type	mpq_type;
	enum glex_eq_type	eq_type;
	uint32_t		num;
	uint32_t		key;
	uint32_t		dq_capacity;	/* each unit is a descriptor */
	uint32_t		mpq_capacity;	/* each unit is an immediate MP */
	uint32_t		eq_capacity;	/* each unit is an event */
};

#define	GLEX_ANY_EP_NUM			0xffffffff
#define	GLEX_EP_DQ_CAPACITY_DEFAULT	0
#define	GLEX_EP_MPQ_CAPACITY_DEFAULT	0
#define	GLEX_EP_EQ_CAPACITY_DEFAULT	0

/* each event is 16 bytes long, and the high 8 bytes should not be 0 */
typedef struct glex_event {
	volatile uint64_t cookie_0;
	volatile uint64_t cookie_1;
} glex_event_t;

typedef union glex_mem_handle {
	struct {
#if __BYTE_ORDER == __LITTLE_ENDIAN
		uint16_t mmt_index;
		uint16_t att_base_off;
		uint32_t att_index;
#else
		uint32_t att_index;
		uint16_t att_base_off;
		uint16_t mmt_index;
#endif
	};
	uint64_t v;
} glex_mem_handle_t;

enum glex_mem_flag {
	GLEX_MEM_READ	= 1,
	GLEX_MEM_WRITE	= 1 << 1,
	GLEX_MEM_IOMMU	= 1 << 2
};

enum glex_flag {
	GLEX_FLAG_LOCAL_INTR		= 1,
	GLEX_FLAG_REMOTE_INTR		= 1 << 1,
	GLEX_FLAG_FENCE			= 1 << 2,
	GLEX_FLAG_LOCAL_EVT		= 1 << 3,
	GLEX_FLAG_REMOTE_EVT		= 1 << 4,
	GLEX_FLAG_NO_CONNECTION		= 1 << 5,
	GLEX_FLAG_ROUTE_ADAPTIVE	= 1 << 6,
	GLEX_FLAG_COLL			= 1 << 7,	/* collective request */
	GLEX_FLAG_COLL_SEQ_START	= 1 << 8,	/* start of collective request sequence */
	GLEX_FLAG_COLL_SEQ_TAIL		= 1 << 9,	/* tail of collective request sequence */
	GLEX_FLAG_COLL_COUNTER		= 1 << 10,	/* collective control packet (CP) */
	GLEX_FLAG_COLL_CP_DATA		= 1 << 11,	/* data of CP will be put into MPQ */
	GLEX_FLAG_COLL_CP_SWAP_DATA	= 1 << 12,	/* swap data in MP request */
	GLEX_FLAG_COLL_CP_SWAP_ADDR	= 1 << 13,	/* swap address in RDMA request */
	GLEX_FLAG_COLL_SWAP		= 1 << 14,
	GLEX_FLAG_COLL_SWAP_EP		= 1 << 15,
	GLEX_FLAG_COLL_SWAP_SET		= 1 << 16,	/* it is a set of collective requests */
	GLEX_FLAG_COLL_REDUCE		= 1 << 17,
	GLEX_FLAG_COLL_REDUCE_RESULT	= 1 << 18
};

/* immediate MP request */
typedef struct glex_imm_mp_req {
	glex_ep_addr_t		rmt_ep_addr;
	void			*data;
	uint32_t		len;
	uint32_t		coll_counter;
	int32_t			flag;
	struct glex_imm_mp_req	*next;
} glex_imm_mp_req_t;

/*
 * RDMA GET:	local_mh <==== rmt_mh;
 * RDMA PUT:	local_mh ====> rmt_mh;
 */
enum glex_rdma_type {
	GLEX_RDMA_TYPE_PUT = 0,
	GLEX_RDMA_TYPE_GET = 1
};

/* immediate RDMA can only do RDMA PUT */
typedef struct glex_imm_rdma_req {
	glex_ep_addr_t			rmt_ep_addr;
	void				*data;
	uint32_t			len;
	glex_mem_handle_t		rmt_mh;
	uint32_t			rmt_offset;
	glex_event_t			rmt_evt;
	uint32_t			rmt_key;	/* key of remote endpoint */
	uint32_t			coll_counter;
	int32_t				flag;
	struct glex_imm_rdma_req	*next;
} glex_imm_rdma_req_t;

typedef struct glex_rdma_req {
	glex_ep_addr_t		rmt_ep_addr;
	glex_mem_handle_t	local_mh;
	uint32_t		local_offset;
	uint32_t		len;
	glex_mem_handle_t	rmt_mh;
	uint32_t		rmt_offset;
	enum glex_rdma_type	type;
	glex_event_t		local_evt;
	glex_event_t		rmt_evt;
	uint32_t		rmt_key;	/* key of remote endpoint */
	uint32_t		coll_counter;
	uint32_t		coll_set_num;	/* number of RDMA PUT request in a set */
	int32_t			flag;
	struct glex_rdma_req	*next;
} glex_rdma_req_t;

enum glex_coll_reduce_op {
	GLEX_COLL_REDUCE_SUM_FLOAT		= 0x0,
	GLEX_COLL_REDUCE_MAXLOC_FLOAT		= 0x1,
	GLEX_COLL_REDUCE_MINLOC_FLOAT		= 0x2,
	GLEX_COLL_REDUCE_SUM_SIGNED		= 0x3,
	GLEX_COLL_REDUCE_MAXLOC_SIGNED		= 0x4,
	GLEX_COLL_REDUCE_MINLOC_SIGNED		= 0x5,
	GLEX_COLL_REDUCE_SUM_UNSIGNED		= 0x6,
	GLEX_COLL_REDUCE_MAXLOC_UNSIGNED	= 0x7,
	GLEX_COLL_REDUCE_MINLOC_UNSIGNED	= 0x8,
	GLEX_COLL_REDUCE_LAND			= 0x9,
	GLEX_COLL_REDUCE_LOR			= 0xa,
	GLEX_COLL_REDUCE_LXOR			= 0xb,
	GLEX_COLL_REDUCE_BAND			= 0xc,
	GLEX_COLL_REDUCE_BOR			= 0xd,
	GLEX_COLL_REDUCE_BXOR			= 0xe
};

enum glex_coll_reduce_data_bits {
	GLEX_COLL_REDUCE_DATA_BITS_32 = 0,
	GLEX_COLL_REDUCE_DATA_BITS_64 = 1
};

union glex_coll_data_loc {
	struct {
		int32_t	 data;
		uint32_t location;
	} int32_s;
	struct {
		uint32_t data;
		uint32_t location;
	} uint32_s;
	struct {
		int64_t	 data;
		uint32_t location;
	} int64_s;
	struct {
		uint64_t data;
		uint32_t location;
	} uint64_s;
	struct {
		float	 data;
		uint32_t location;
	} float_s;
	struct {
		double	 data;
		uint32_t location;
	} double_s;
};

#define	GLEX_COLL_MAX_BRANCH_DEGREE	32
#define	GLEX_COLL_REDUCE_MAX_DATA_UNITS	6
#define	GLEX_COLL_SET_MAX_NUM		14

/* definitions for error descriptor */
enum glex_err_req_op {
	GLEX_ERR_OP_IMM_MP,
	GLEX_ERR_OP_MP,
	GLEX_ERR_OP_IMM_RDMA_PUT,
	GLEX_ERR_OP_RDMA_PUT,
	GLEX_ERR_OP_RDMA_GET
};

enum glex_err_req_status {
	GLEX_ERR_STATUS_FORMAT,
	GLEX_ERR_STATUS_SRC_HOST_DEAD,
	GLEX_ERR_STATUS_DEST_HOST_DEAD,
	GLEX_ERR_STATUS_PCIE_READ,
	GLEX_ERR_STATUS_ATT,
	GLEX_ERR_STATUS_MMT,
	GLEX_ERR_STATUS_SRC_VP_STOP,
	GLEX_ERR_STATUS_DEST_VP_STOP,
	GLEX_ERR_STATUS_DEST_MPQ_FULL,
	GLEX_ERR_STATUS_DEST_EQ_FULL,
	GLEX_ERR_STATUS_CONNECTION_MAX_RETRY,
	GLEX_ERR_STATUS_CONNECTION_RDMA_GET,
	GLEX_ERR_STATUS_CONNECTION_NACK,
	GLEX_ERR_STATUS_CONNECTION_ECC
};

struct glex_err_req {
	enum glex_err_req_op		op;
	enum glex_err_req_status	status;
	uint32_t			rmt_nic_id;
	uint32_t			rmt_ep_num;
	uint64_t			data0;
	uint64_t			data1;
	uint64_t			data2;
};

/**
 * glex_num_of_devices - Query how many devices
 */
glex_ret_t glex_num_of_device(uint32_t *num_of_devices);

/**
 * glex_open_deivce - Open a device for use
 */
glex_ret_t glex_open_device(uint32_t dev_id, glex_device_handle_t *dev);

/**
 * glex_close_device - Release device
 */
glex_ret_t glex_close_device(glex_device_handle_t dev);

/**
 * glex_query_device - Query attributes of a device
 */
glex_ret_t glex_query_device(glex_device_handle_t dev,
			     struct glex_device_attr *dev_attr);

/**
 * glex_create_ep - Create an endpoint with specified attribute
 */
glex_ret_t glex_create_ep(glex_device_handle_t dev,
			  struct glex_ep_attr *ep_attr,
			  glex_ep_handle_t *ep);

/**
 * glex_destroy_ep - Destroy an endpoint
 */
glex_ret_t glex_destroy_ep(glex_ep_handle_t ep);

/**
 * glex_query_ep - Query attributes of an endpoint
 */
glex_ret_t glex_query_ep(glex_ep_handle_t ep,
			 struct glex_ep_attr *ep_attr);

/**
 * glex_get_ep_addr - Get endpoint address
 */
glex_ret_t glex_get_ep_addr(glex_ep_handle_t ep, glex_ep_addr_t *ep_addr);

/**
 * glex_compose_ep_addr - Construct an endpoint address
 */
glex_ret_t glex_compose_ep_addr(uint32_t nic_id, uint32_t ep_num,
				enum glex_ep_type type,
				glex_ep_addr_t *ep_addr);

/**
 * glex_decompose_ep_addr - Extract information from endpoint address
 */
glex_ret_t glex_decompose_ep_addr(glex_ep_addr_t ep_addr,
				  enum glex_ep_type type,
				  uint32_t *nic_id, uint32_t *ep_num);

/**
 * glex_register_mem - Register a memory region
 */
glex_ret_t glex_register_mem(glex_ep_handle_t ep,
			     void *addr, uint64_t len,
			     int32_t flag,
			     glex_mem_handle_t *mh);

/**
 * glex_deregister_mem - Deregister a memory region
 */
glex_ret_t glex_deregister_mem(glex_ep_handle_t ep,
			       glex_mem_handle_t mh);

/**
 * glex_send_imm_mp - Post a list of immediate MP send requests
 */
static inline glex_ret_t glex_send_imm_mp(glex_ep_handle_t ep,
					  struct glex_imm_mp_req *mp_req,
					  struct glex_imm_mp_req **bad_mp_req)
{
	return ep->send_imm_mp(ep, mp_req, bad_mp_req);
}

/**
 * glex_probe_first_mp - Probe the first not received MP in MPQ, don't remove it from MPQ
 */
glex_ret_t glex_probe_first_mp(glex_ep_handle_t ep, int32_t timeout,
			       glex_ep_addr_t *src_ep_addr,
			       void **data, uint32_t *len);

/**
 * glex_probe_next_mp - Probe the next not probed MP in MPQ, don't remove it from MPQ
 */
glex_ret_t glex_probe_next_mp(glex_ep_handle_t ep,
			      glex_ep_addr_t *src_ep_addr,
			      void **data, uint32_t *len);

/**
 * glex_discard_probed_mp - Discards all probed MPs between probe_first and probe_next
 */
glex_ret_t glex_discard_probed_mp(glex_ep_handle_t ep);

/**
 * glex_receive_mp - Receive and remove the first MP from MPQ
 */
glex_ret_t glex_receive_mp(glex_ep_handle_t ep, int32_t timeout,
			   glex_ep_addr_t *src_ep_addr,
			   void *data, uint32_t *len);

/**
 * glex_imm_rdma - Post a list of immediate RDMA Put requests
 */
static inline glex_ret_t glex_imm_rdma(glex_ep_handle_t ep,
				       struct glex_imm_rdma_req *rdma_req,
				       struct glex_imm_rdma_req **bad_rdma_req)
{
	return ep->imm_rdma(ep, rdma_req, bad_rdma_req);
}

/**
 * glex_rdma - Post a list of RDMA requests
 */
static inline glex_ret_t glex_rdma(glex_ep_handle_t ep,
				   struct glex_rdma_req *rdma_req,
				   struct glex_rdma_req **bad_rdma_req)
{
	return ep->rdma(ep, rdma_req, bad_rdma_req);
}

/**
 * glex_probe_first_event - Probe the first event in EQ
 */
glex_ret_t glex_probe_first_event(glex_ep_handle_t ep, int32_t timeout,
				  glex_event_t **event);

/**
 * glex_probe_next_event - Probe the next un-probed event in EQ
 */
glex_ret_t glex_probe_next_event(glex_ep_handle_t ep,
				 glex_event_t **event);

/**
 * glex_discard_probed_event - Discard all probed events
 */
glex_ret_t glex_discard_probed_event(glex_ep_handle_t ep);

/**
 * glex_poll_error_desc - Poll error requests
 */
glex_ret_t glex_poll_error_req(glex_ep_handle_t ep,
			       uint32_t *num_er,
			       struct glex_err_req *err_req_list);

/**
 * glex_error_str - Show information about the error code
 */
const char * glex_error_str(glex_ret_t ret);

/**
 * glex_error_req_status_str - Show information about the status of error request
 */
const char * glex_error_req_status_str(enum glex_err_req_status status);

/**
 * glex_error_req_opcode_str - Show information about the opcode of error request
 */
const char * glex_error_req_opcode_str(enum glex_err_req_op opcode);

/**
 * glex_coll_compose_mp_swap_addr_data - Compose swap address for collective RDMA operation
 */
glex_ret_t glex_coll_compose_mp_swap_addr_data(glex_ep_handle_t ep,
					       glex_mem_handle_t mh,
					       uint32_t offset,
					       uint32_t key,
					       void *data,
					       uint32_t *data_len);

/**
 * glex_coll_compose_mp_reduce_data - Compose MP data for collective reduce operations
 */
glex_ret_t glex_coll_compose_mp_reduce_data(union glex_coll_data_loc send_buf[],
					    uint32_t count,
					    enum glex_coll_reduce_data_bits data_bits,
					    enum glex_coll_reduce_op op,
					    uint32_t branch_degree,
					    void *data,
					    uint32_t *data_len);

/**
 * glex_coll_decompose_mp_reduce_data - Decompose results from MP data
 */
glex_ret_t glex_coll_decompose_mp_reduce_data(union glex_coll_data_loc recv_buf[],
					      uint32_t count,
					      enum glex_coll_reduce_data_bits data_bits,
					      void *data);

/**
 * glex_coll_compose_mp_set_data - Compose data for MP set descriptor
 */
glex_ret_t glex_coll_compose_mp_set_data(glex_ep_addr_t ep_addrs[],
					 uint32_t count,
					 void *data,
					 uint32_t *data_len);

END_C_DECLS

#endif	/* _GLEX_H_ */
