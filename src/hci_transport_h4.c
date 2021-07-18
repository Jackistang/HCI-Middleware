#include <rtthread.h>
#include <rtdevice.h>
#include <board.h>
#include "hm_config.h"
#include "hm_hci_transport_h4.h"
#include "hm_hci_transport_h4_uart.h"

typedef enum {
    H4_RECV_STATE_NONE,
    H4_RECV_STATE_CMD = 1,
    H4_RECV_STATE_ACL,
    H4_RECV_STATE_SCO,
    H4_RECV_STATE_EVT,
    H4_RECV_STATE_ISO,
} h4_recv_state_t;


#define HCI_COMMAND_BUF_SIZE RT_ALIGN(255, RT_ALIGN_SIZE) 
#define HCI_EVENT_BUF_SIZE   RT_ALIGN(255,  RT_ALIGN_SIZE)
#define HCI_ACL_BUF_SIZE     RT_ALIGN(255, RT_ALIGN_SIZE)

struct h4_rx_evt {
    uint8_t *data;
    uint16_t cur;
    uint16_t len;
};

struct h4_rx_acl {
    uint8_t *data;
    uint16_t cur;
    uint16_t len;
};

struct h4_rx {
    h4_recv_state_t state;
    union {
        struct h4_rx_evt evt;
        struct h4_rx_acl acl;
    };
};

struct h4_tx {

};
// typedef struct package_callback {
//     hci_trans_h4_package_callback_t cb;
//     rt_list_t list;
// } package_callback_t;

// typedef struct hci_send_sync_object {
//     uint8_t sync_cmd;
//     hci_vendor_evt_callback_t cb;
//     struct rt_semaphore sync_sem;
//     uint8_t cc_evt;
// } hci_send_sync_object_t;

struct h4_object {
    struct h4_rx rx;
    struct h4_tx tx;

    rt_mailbox_t evt_mb;
    // hci_send_sync_object_t send_sync_object; 

    // rt_list_t   callback_list;
};

static struct h4_object h4_object;

static int hci_trans_h4_alloc(uint8_t type, uint8_t **ptr);
static void hci_trans_h4_free(void *ptr);

static struct rt_mempool cmd_pool;
ALIGN(RT_ALIGN_SIZE)
static uint8_t cmd_pool_buf[MEMPOOL_SIZE(1, HCI_COMMAND_BUF_SIZE)];

static struct rt_mempool evt_pool;
ALIGN(RT_ALIGN_SIZE)
static uint8_t evt_pool_buf[MEMPOOL_SIZE(2, HCI_EVENT_BUF_SIZE)];

static struct rt_mempool acl_pool;
ALIGN(RT_ALIGN_SIZE)
static uint8_t acl_pool_buf[MEMPOOL_SIZE(1, HCI_ACL_BUF_SIZE)];

void hci_trans_h4_init(struct hci_trans_h4_config *config)
{
    rt_mp_init(&cmd_pool, "cmd_pool", cmd_pool_buf, ARRAY_SIZE(cmd_pool_buf), HCI_COMMAND_BUF_SIZE);
    rt_mp_init(&evt_pool, "evt_pool", evt_pool_buf, ARRAY_SIZE(evt_pool_buf), HCI_EVENT_BUF_SIZE);
    rt_mp_init(&acl_pool, "acl_pool", acl_pool_buf, ARRAY_SIZE(acl_pool_buf), HCI_ACL_BUF_SIZE);

    h4_object.evt_mb = rt_mb_create("evt.mb", 5, RT_IPC_FLAG_PRIO);

    hci_trans_h4_uart_init(&config->uart_config);

    rt_kprintf("hci transport h4 init success.\n");
}

int hci_trans_h4_open(void)
{
    int err;
    h4_object.rx.state = H4_RECV_STATE_NONE;

    // rt_sem_init(&h4_object.send_sync_object.sync_sem, "send sync sem", 0, RT_IPC_FLAG_PRIO);

    if ((err = hci_trans_h4_uart_open()))
        return err;

    // h4_object.send_sync_object.sync_cmd = 0;
    // h4_object.send_sync_object.cb = NULL;

    // rt_list_init(&h4_object.callback_list);

    rt_kprintf("hci transport h4 open success.\n");

    return HM_SUCCESS;
}

int hci_trans_h4_close(void)
{
    int err;
    h4_object.rx.state = H4_RECV_STATE_NONE;

    // rt_sem_detach(&h4_object.send_sync_object.sync_sem);

    if ((err = hci_trans_h4_uart_close()))
        return err;

    rt_kprintf("hci transport h4 close success.\n");

    return HM_SUCCESS;
}

// int hci_trans_h4_register_callback(hci_trans_h4_package_callback_t callback)
// {
//     package_callback_t *pkg_callback;
//     rt_list_for_each_entry(pkg_callback, &h4_object.callback_list, list) {
//         if (pkg_callback->cb == callback)   /* This callback function has been registered. */
//             return HM_SUCCESS;
//     }

//     pkg_callback = rt_malloc(sizeof(package_callback_t));
//     if (pkg_callback == NULL)
//         return HM_NO_MEMORY;
    
//     pkg_callback->cb = callback;
//     rt_list_insert_after(&h4_object.callback_list, &pkg_callback->list);

//     return HM_SUCCESS;
// }

// void hci_trans_h4_remove_callback(hci_trans_h4_package_callback_t callback)
// {
//     package_callback_t *pkg_callback;
//     rt_list_for_each_entry(pkg_callback, &h4_object.callback_list, list) {
//         if (pkg_callback->cb == callback)
//             break;
//     }
//     rt_list_remove(&pkg_callback->list);
//     rt_free(pkg_callback);
// }

// static void process_sync_cmd_event(uint8_t type, uint8_t *pkg, uint16_t len)
// {
//     RT_ASSERT(type == HCI_TRANS_H4_TYPE_EVT);

//     /* hci_vendor_cmd_send_sync() function will set this, used for sync callback, 
//         and don't call other registered callback. */
//     if (h4_object.send_sync_object.cb) {
//         h4_object.send_sync_object.cb(pkg, len);
//     } else {
//         /* Check it is Complete event. */
//         if (pkg[0] == 0x0E) {
//             h4_object.send_sync_object.cc_evt = 1;
//         }
//     }

//     rt_sem_release(&h4_object.send_sync_object.sync_sem);
// }

// static void hci_trans_h4_pkg_notify(uint8_t type, uint8_t *pkg, uint16_t len)
// {
//     if (h4_object.send_sync_object.sync_cmd) {
//         process_sync_cmd_event(type, pkg, len);
//         h4_object.send_sync_object.sync_cmd = 0;
//         return ;
//     }

//     package_callback_t *pkg_callback;
//     rt_list_for_each_entry(pkg_callback, &h4_object.callback_list, list) {
//         pkg_callback->cb(type, pkg, len);
//     }
// }

static int hci_trans_h4_recv_type(uint8_t byte)
{
    switch (byte) {
    case H4_RECV_STATE_EVT:
        h4_object.rx.evt.data = NULL;
        h4_object.rx.evt.cur = 0;
        h4_object.rx.evt.len = 0;
        break;
    case H4_RECV_STATE_ACL:
        h4_object.rx.acl.data = NULL;
        h4_object.rx.acl.cur = 0;
        h4_object.rx.acl.len = 0;
        break;
    default:
        return HM_NOT_SUPPORT;
    }

    h4_object.rx.state = byte;
    return HM_SUCCESS;
}

static int hci_trans_h4_recv_acl(uint8_t byte)
{
    int err = HM_SUCCESS;
    struct h4_rx_acl *acl = &h4_object.rx.acl;

    if (!acl->data) {
        if ((err = hci_trans_h4_alloc(H4_RECV_STATE_ACL, &acl->data)))
            return err;
        rt_memset(acl->data, 0, HCI_ACL_BUF_SIZE);
    }

    acl->data[acl->cur++] = byte;

    if (acl->cur < sizeof(struct hm_hci_acl))
        return HM_SUCCESS;

    if (acl->cur == sizeof(struct hm_hci_acl)) {
        acl->len = (uint16_t)acl->data[2] | (uint16_t)acl->data[3] << 8;    // Parameter length
        acl->len += sizeof(struct hm_hci_acl);     // ACL package header.
    }

    if (acl->cur == acl->len) {
//         hci_trans_h4_pkg_notify(HCI_TRANS_H4_TYPE_ACL, acl->data, acl->len);
// #if HM_CONFIG_BTSTACK
//         /* Transfer the responsibility of free memory to btstack user.*/
// #else
//         hci_trans_h4_free(acl->data);
//         acl->data = NULL;
//         acl->cur = acl->len = 0;
// #endif
        // TODO

        h4_object.rx.state = H4_RECV_STATE_NONE;
    }

    return HM_SUCCESS;
}

static int hci_trans_h4_recv_evt(uint8_t byte)
{
    int err = HM_SUCCESS;
    struct h4_rx_evt *evt = &h4_object.rx.evt;

    if (!evt->data) {
        if ((err = hci_trans_h4_alloc(H4_RECV_STATE_EVT, &evt->data)))
            return err;

        rt_memset(evt->data, 0, HCI_EVENT_BUF_SIZE);
    }

    evt->data[evt->cur++] = byte;

    if (evt->cur < sizeof(struct hm_hci_evt))
        return HM_SUCCESS;
    
    if (evt->cur == sizeof(struct hm_hci_evt))
        evt->len = evt->data[1] + sizeof(struct hm_hci_evt);
    
    if (evt->cur == evt->len) {
//         hci_trans_h4_pkg_notify(HCI_TRANS_H4_TYPE_EVT, evt->data, evt->len);
// #if HM_CONFIG_BTSTACK
//         /* Transfer the responsibility of free memory to btstack user.*/
// #else
//         hci_trans_h4_free(evt->data);
//         evt->data = NULL;
//         evt->cur = evt->len = 0;
// #endif

        /* The user who receive mail from "event mailbox" is responsible for free it's memory.*/
        err = rt_mb_send(h4_object.evt_mb, (rt_ubase_t)evt->data);
        if (err) {
            rt_kprintf("event mailbox send mail fail\n");
        }

        h4_object.rx.state = H4_RECV_STATE_NONE;
    }

    return HM_SUCCESS;
}

int hci_trans_h4_recv_byte(uint8_t byte)
{
    int err = HM_SUCCESS;

    switch (h4_object.rx.state) {
    case H4_RECV_STATE_NONE:
        err = hci_trans_h4_recv_type(byte);
        break;
    case H4_RECV_STATE_ACL:
        err = hci_trans_h4_recv_acl(byte);
        break;
    case H4_RECV_STATE_EVT:
        err = hci_trans_h4_recv_evt(byte);
        break;
    case H4_RECV_STATE_CMD:
    case H4_RECV_STATE_SCO:
    case H4_RECV_STATE_ISO:
    default:
        return HM_NOT_SUPPORT;
    }

    if (err != HM_SUCCESS) {
        h4_object.rx.state = H4_RECV_STATE_NONE;
    }

    return err;
}

static int hci_trans_h4_alloc(uint8_t type, uint8_t **ptr)
{
    void *p = NULL;
    switch (type) {
    case HCI_TRANS_H4_TYPE_CMD:
        p = rt_mp_alloc(&cmd_pool, RT_WAITING_NO);
        break;
    case HCI_TRANS_H4_TYPE_EVT:
        p = rt_mp_alloc(&evt_pool, RT_WAITING_NO);
        break;
    case HCI_TRANS_H4_TYPE_ACL:
        p = rt_mp_alloc(&acl_pool, RT_WAITING_NO);
        break;
    case HCI_TRANS_H4_TYPE_SCO:
    case HCI_TRANS_H4_TYPE_ISO:
    default:
        return HM_NOT_SUPPORT;
    }
    
    if (p == NULL) {
        rt_kprintf("Memory pool (%d) is not enough.\n", type);
        return HM_NO_MEMORY;
    }

    *ptr = p;

    return HM_SUCCESS;
}

static void hci_trans_h4_free(void *ptr)
{
    rt_mp_free(ptr);
}

/*
    uint8_t *p = hci_trans_h4_send_alloc(0x01);
    fill hci command to p.
    hci_trans_h4_send(p);
    hci_trans_h4_send_free(p);
*/
int hci_trans_h4_send(uint8_t type, uint8_t *data)
{
    int err;
    uint16_t len = 0;
    
    switch (type) {
    case HCI_TRANS_H4_TYPE_CMD:
        len = 1 + data[2] + sizeof(struct hm_hci_cmd);
        break;
    case HCI_TRANS_H4_TYPE_ACL:
        len = 1 + ((uint16_t)data[2] | (uint16_t)data[3] << 8) + sizeof(struct hm_hci_acl);
        break;
    default:
        return HM_NOT_SUPPORT;
    }

    uint8_t *p = data - 1;
    *p = type;
    
    err = hci_trans_h4_uart_send(p, len);
    if (err != HM_SUCCESS)
        return err;

    return HM_SUCCESS;
}

/**
 * 
 * @note Alloced memory begin with the second byte in memory block, the first byte is used for H4 type when send.
*/
void *hci_trans_h4_send_alloc(uint8_t type)
{
    uint8_t *p = NULL;
    int err = HM_SUCCESS;

    if ((err = hci_trans_h4_alloc(type, &p)))
        return NULL;
    
    return p+1;
}

void hci_trans_h4_send_free(uint8_t *buf)
{
    RT_ASSERT(buf);

    uint8_t *p = buf - 1;
    hci_trans_h4_free(p);
}

// static int hci_cmd_send_sync_inline(uint8_t *hci_cmd, uint16_t len, int32_t time)
// {
//     int err;

//     uint8_t *p = hci_trans_h4_send_alloc(HCI_TRANS_H4_TYPE_CMD);
//     if (p == NULL) {
//         err = HM_NO_MEMORY;
//         return err;
//     }

//     rt_memcpy(p, hci_cmd, len);
//     hci_trans_h4_send(HCI_TRANS_H4_TYPE_CMD, p);

//     err = rt_sem_take(&h4_object.send_sync_object.sync_sem, time);
//     if (err) {
//         rt_kprintf("HCI command send sync timeout.\n");
//         hci_trans_h4_send_free(p);
//         return HM_TIMEOUT;
//     }

//     hci_trans_h4_send_free(p);
//     return HM_SUCCESS;
// }

// static void hci_cmd_send_sync_dummy_callback(uint8_t *hci_evt, uint16_t len)
// {
//     return ;
// }

// int hci_vendor_cmd_send_sync(uint8_t *hci_cmd, uint16_t len, int32_t time, hci_vendor_evt_callback_t callback)
// {
//     RT_ASSERT(hci_cmd);
//     RT_ASSERT(len > 0);

//     int err = HM_SUCCESS;
//     h4_object.send_sync_object.sync_cmd = 1;
    
//     if (callback == NULL)
//         h4_object.send_sync_object.cb = hci_cmd_send_sync_dummy_callback;
//     else
//         h4_object.send_sync_object.cb = callback;

//     err = hci_cmd_send_sync_inline(hci_cmd, len, time);

//     h4_object.send_sync_object.cb = NULL;
//     return err;
// }

// /* Should receive complete event. */
// int hci_cmd_send_sync(uint8_t *hci_cmd, uint16_t len, int32_t time)
// {
//     RT_ASSERT(hci_cmd);
//     RT_ASSERT(len > 0);

//     h4_object.send_sync_object.sync_cmd = 1;

//     int err;

//     err = hci_cmd_send_sync_inline(hci_cmd, len, time);
//     if (err) 
//         return err;

//     if (!h4_object.send_sync_object.cc_evt)
//         return HM_HCI_CMD_ERROR;

//     h4_object.send_sync_object.cc_evt = 0;
//     return HM_SUCCESS;
// }

// static uint8_t reset_cmd[] = {0x01, 0x03, 0x0C, 0x00};
// int hci_reset_cmd_send(void)
// {
//     return hci_trans_h4_uart_send(reset_cmd, ARRAY_SIZE(reset_cmd));
// }

/* The user is responsible for free it's memory with `hci_trans_h4_recv_free()`.*/
int hci_trans_h4_recv_event(uint8_t **buf, int ms)
{
    uint8_t *p = NULL;

    rt_err_t err = rt_mb_recv(h4_object.evt_mb, (rt_ubase_t *)&p, ms);
    if (err != RT_EOK)
        return -HM_TIMEOUT;
    
    *buf = p;
    return HM_SUCCESS;
}

void hci_trans_h4_recv_free(uint8_t *p)
{
    hci_trans_h4_free(p);
}
