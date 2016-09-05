#include <stdio.h>
#include <stdlib.h>
#include <string.h> /* for memcpy */
#include <sg/container/bip_buf.h>

struct sg_bip_buf_real
{
    unsigned int a_start, a_end; /* region A */
    unsigned int b_end; /* region B */
    int b_inuse; /* is B inuse? */
    unsigned char data[];
    size_t data_size;
};

/* find out if we should turn on region B
 * ie. is the distance from A to buffer's end less than B to A? */
static void __check_for_switch_to_b(sg_bip_buf_t *buf)
{
    struct sg_bip_buf_real *me = (struct sg_bip_buf *)buf;

    if (me->data_size - me->a_end < me->a_start - me->b_end)
        me->b_inuse = 1;
}

sg_bip_buf_t *sg_bip_buf_create(size_t size)
{
    size_t total_size;
    struct sg_bip_buf_real *me;
    
    total_size = sizeof(struct sg_bip_buf_real) + size;
    me = (struct sg_bip_buf_real *)malloc(total_size);
    if (!me)
        return NULL;

    memset(me, 0, total_size);
    me->data_size = size;
    return me;
}

unsigned char *sg_bip_buf_peek(const sg_bip_buf_t *buf, const unsigned int size)
{
    struct sg_bip_buf_real *me = (struct sg_bip_buf *)buf;

    /* make sure we can actually peek at this data */
    if (me->data_size < me->a_start + size)
        return NULL;

    if (bipbuf_is_empty(me))
        return NULL;

    return (unsigned char*)me->data + me->a_start;
}

unsigned char *sg_bip_buf_get(const sg_bip_buf_t *buf, const unsigned int size)
{
    struct sg_bip_buf_real *me = (struct sg_bip_buf *)buf;

    if (sg_bip_buf_is_empty(buf))
        return NULL;

    /* make sure we can actually poll this data */
    if (me->data_size < me->a_start + size)
        return NULL;

    void *end = me->data + me->a_start;
    me->a_start += size;

    /* we seem to be empty.. */
    if (me->a_start == me->a_end) {
        /* replace a with region b */
        if (1 == me->b_inuse) {
            me->a_start = 0;
            me->a_end = me->b_end;
            me->b_end = me->b_inuse = 0;
        } else
            /* safely move cursor back to the start because we are empty */
            me->a_start = me->a_end = 0;
    }

    __check_for_switch_to_b(me);
    return end;
}

int sg_bip_buf_put(const sg_bip_buf_t *buf, const unsigned char *data, const int size)
{
    struct sg_bip_buf_real *me = (struct sg_bip_buf_real *)buf;

    /* not enough space */
    if (sg_bip_buf_unused_size(buf) < size)
        return 0;

    if (1 == me->b_inuse) {
        memcpy(me->data + me->b_end, data, size);
        me->b_end += size;
    } else {
        memcpy(me->data + me->a_end, data, size);
        me->a_end += size;
    }

    __check_for_switch_to_b(me);
    return size;
}

int sg_bip_buf_unused_size(const sg_bip_buf_t *buf)
{
    struct sg_bip_buf_real *me = (struct sg_bip_buf *)buf;

    if (1 == me->b_inuse)
        /* distance between region B and region A */
        return me->a_start - me->b_end;
    else
        return me->data_size - me->a_end;
}

int sg_bip_buf_used_size(const sg_bip_buf_t *buf)
{
    struct sg_bip_buf_real *me = (struct sg_bip_buf *)buf;

    return (me->a_end - me->a_start) + me->b_end;
}

int sg_bip_buf_size(const sg_bip_buf_t *buf)
{
    struct sg_bip_buf_real *me = (struct sg_bip_buf *)buf;

    return me->data_size;
}

int sg_bip_buf_is_empty(const sg_bip_buf_t *buf)
{
    struct sg_bip_buf_real *me = (struct sg_bip_buf *)buf;

    return (me->a_start == me->a_end) ? 1 : 0;
}

void sg_bip_buf_destroy(sg_bip_buf_t *buf)
{
    free(buf);
}