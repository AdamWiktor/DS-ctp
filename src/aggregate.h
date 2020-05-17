/**
 * \file
 *         Header file for hop-by-hop in-tree reliable data aggregation
 * \author
 *         Mateusz Banaszek
 */

/**
 * \addtogroup rime
 * @{
 */

/**
 * \defgroup rimeaggregate Tree-based hop-by-hop in-tree reliable data aggregation
 * @{
 *
 * The aggregation module implements a hop-by-hop in-tree reliable data
 *   aggregation mechanism.
 *
 * NOTE: This header is heavily inspired by collect.h. Only differences are
 *   documented here. Everything not discribed here follows RIME's collect.
 */

#ifndef AGGREGATE_H_
#define AGGREGATE_H_

#include "net/rime/announcement.h"
#include "net/rime/runicast.h"
#include "net/rime/neighbor-discovery.h"
#include "net/rime/collect-neighbor.h"
#include "net/rime/packetqueue.h"
#include "net/rime/netflood.h"
#include "sys/ctimer.h"
#include "lib/list.h"

#ifdef COLLECT_CONF_PACKET_ID_BITS
#define COLLECT_PACKET_ID_BITS COLLECT_CONF_PACKET_ID_BITS
#else /* COLLECT_CONF_PACKET_ID_BITS */
#define COLLECT_PACKET_ID_BITS 8
#endif /* COLLECT_CONF_PACKET_ID_BITS */

#ifdef COLLECT_CONF_TTL_BITS
#define COLLECT_TTL_BITS COLLECT_CONF_TTL_BITS
#else /* COLLECT_CONF_TTL_BITS */
#define COLLECT_TTL_BITS 4
#endif /* COLLECT_CONF_TTL_BITS */

#ifdef COLLECT_CONF_HOPS_BITS
#define COLLECT_HOPS_BITS COLLECT_CONF_HOPS_BITS
#else /* COLLECT_CONF_HOPS_BITS */
#define COLLECT_HOPS_BITS 4
#endif /* COLLECT_CONF_HOPS_BITS */

#ifdef COLLECT_CONF_MAX_REXMIT_BITS
#define COLLECT_MAX_REXMIT_BITS COLLECT_CONF_MAX_REXMIT_BITS
#else /* COLLECT_CONF_REXMIT_BITS */
#define COLLECT_MAX_REXMIT_BITS 5
#endif /* COLLECT_CONF_REXMIT_BITS */

#define COLLECT_ATTRIBUTES  { PACKETBUF_ADDR_ESENDER,     PACKETBUF_ADDRSIZE }, \
                            { PACKETBUF_ATTR_EPACKET_ID,  PACKETBUF_ATTR_BIT * COLLECT_PACKET_ID_BITS }, \
                            { PACKETBUF_ATTR_PACKET_ID,   PACKETBUF_ATTR_BIT * COLLECT_PACKET_ID_BITS }, \
                            { PACKETBUF_ATTR_TTL,         PACKETBUF_ATTR_BIT * COLLECT_TTL_BITS }, \
                            { PACKETBUF_ATTR_HOPS,        PACKETBUF_ATTR_BIT * COLLECT_HOPS_BITS }, \
                            { PACKETBUF_ATTR_MAX_REXMIT,  PACKETBUF_ATTR_BIT * COLLECT_MAX_REXMIT_BITS }, \
                            { PACKETBUF_ATTR_PACKET_TYPE, PACKETBUF_ATTR_BIT }, \
                            UNICAST_ATTRIBUTES

struct aggregate_callbacks {
  /**
   * Callback announcing to the sink the aggregated result.
   *
   * The callback is called only if the mote is the sink.
   * The callback is called only when the value of the aggregated result changes.
   *
   * @param aggregate Current aggregate function. It is the same as an argument
   *   to the last `aggregate_set_aggregate()` call.
   * @param result Current aggregated result.
   */
  void (* recv)(const char *aggregate, int32_t result);
};

#ifndef COLLECT_CONF_ANNOUNCEMENTS
#define COLLECT_ANNOUNCEMENTS 1
#else
#define COLLECT_ANNOUNCEMENTS COLLECT_CONF_ANNOUNCEMENTS
#endif /* COLLECT_CONF_ANNOUNCEMENTS */

struct aggregate_conn {
  struct unicast_conn unicast_conn;
  struct announcement announcement;
  struct ctimer transmit_after_scan_timer;
  const struct aggregate_callbacks *cb;
  struct ctimer retransmission_timer;
  LIST_STRUCT(send_queue_list);
  struct packetqueue send_queue;
  struct collect_neighbor_list neighbor_list;

  struct ctimer keepalive_timer;
  clock_time_t keepalive_period;

  struct ctimer proactive_probing_timer, resend_flood;

  linkaddr_t parent, current_parent;
  uint16_t rtmetric;
  uint8_t seqno;
  uint8_t sending, transmissions, max_rexmits;
  uint8_t eseqno;

  struct netflood_conn flood;
  uint8_t aggregate;
  uint8_t aggregate_seqno;
  int32_t value;
  uint8_t has_value;
  int32_t result;
  uint8_t has_result;
  uint8_t packet[PACKETBUF_SIZE];

  clock_time_t send_time;
};

enum {
  AGGREGATE_NO_ROUTER,
  AGGREGATE_ROUTER,
};

enum {
  AGGR_SUM = 0,
  AGGR_COUNT = 1,
  AGGR_AVG = 2,
  AGGR_MIN = 3,
  AGGR_MAX = 4,
  NUM_AGGR = 5
};

// is_router == AGGREGATE_NO_ROUTER doesn't have to be supported in the assignment.
void aggregate_open(struct aggregate_conn *c, uint16_t channels,
                  uint8_t is_router,
                  const struct aggregate_callbacks *callbacks);

void aggregate_close(struct aggregate_conn *c);

/**
 * Sends a value.
 *
 * @param c Aggregate connection.
 * @param value The value to be sent.
 * @result Status.
 */
int aggregate_send(struct aggregate_conn *c, int32_t value);

void aggregate_set_sink(struct aggregate_conn *c, int should_be_sink);

const linkaddr_t *aggregate_parent(struct aggregate_conn *c);

/**
 * Sets aggregate function.
 *
 * This function is called only by the sink.
 * When the aggregate function is changed, the network discards past values
 *   and starts aggregating follwing values with the new aggregate function.
 *
 * @param c Aggregate connection.
 * @param aggregate The aggregate function. Allowed functions:
 *   "sum": Sums values sent by motes.
 *          The value is wrapped around int32_t in the standard way.
 *   "count": Counts values sent by motes.
 *            The values are de facto ignored.
 *   "avg": Calculates average of values sent by motes.
 *          The result should be as exact as it is possible.
 *          A nonintegral result is casted to int32_t.
 *   "min": Calculates minimum of values sent by motes.
 *   "max": Calculates maximum of values sent by motes.
 *
 * "sum" is the default – it is applied after the connection is opened
 *    and `aggregate_set_aggregate()` is not called yet.
 */
void aggregate_set_aggregate(struct aggregate_conn *c, const char *aggregate);

#define COLLECT_MAX_DEPTH (COLLECT_LINK_ESTIMATE_UNIT * 64 - 1)

#endif /* AGGREGATE_H_ */
/** @} */
/** @} */
