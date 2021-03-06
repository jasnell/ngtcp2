/*
 * ngtcp2
 *
 * Copyright (c) 2018 ngtcp2 contributors
 *
 * Permission is hereby granted, free of charge, to any person obtaining
 * a copy of this software and associated documentation files (the
 * "Software"), to deal in the Software without restriction, including
 * without limitation the rights to use, copy, modify, merge, publish,
 * distribute, sublicense, and/or sell copies of the Software, and to
 * permit persons to whom the Software is furnished to do so, subject to
 * the following conditions:
 *
 * The above copyright notice and this permission notice shall be
 * included in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
 * NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE
 * LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION
 * OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION
 * WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 */
#include "ngtcp2_cc.h"

#include <assert.h>

#include "ngtcp2_log.h"
#include "ngtcp2_macro.h"

ngtcp2_cc_pkt *ngtcp2_cc_pkt_init(ngtcp2_cc_pkt *pkt, uint64_t pkt_num,
                                  size_t pktlen, ngtcp2_tstamp ts_sent) {
  pkt->pkt_num = pkt_num;
  pkt->pktlen = pktlen;
  pkt->ts_sent = ts_sent;

  return pkt;
}

void ngtcp2_default_cc_init(ngtcp2_default_cc *cc, ngtcp2_cc_stat *ccs,
                            ngtcp2_log *log) {
  cc->log = log;
  cc->ccs = ccs;
}

void ngtcp2_default_cc_free(ngtcp2_default_cc *cc) { (void)cc; }

static int default_cc_in_rcvry(ngtcp2_default_cc *cc, ngtcp2_tstamp sent_time) {
  return sent_time <= cc->ccs->recovery_start_time;
}

void ngtcp2_default_cc_on_pkt_acked(ngtcp2_default_cc *cc,
                                    const ngtcp2_cc_pkt *pkt) {
  ngtcp2_cc_stat *ccs = cc->ccs;

  if (default_cc_in_rcvry(cc, pkt->ts_sent)) {
    return;
  }

  if (ccs->cwnd < ccs->ssthresh) {
    ccs->cwnd += pkt->pktlen;
    ngtcp2_log_info(cc->log, NGTCP2_LOG_EVENT_RCV,
                    "packet %" PRIu64 " acked, slow start cwnd=%lu",
                    pkt->pkt_num, ccs->cwnd);
    return;
  }

  ccs->cwnd += NGTCP2_MAX_DGRAM_SIZE * pkt->pktlen / ccs->cwnd;
}

void ngtcp2_default_cc_congestion_event(ngtcp2_default_cc *cc,
                                        ngtcp2_tstamp ts_sent,
                                        ngtcp2_rcvry_stat *rcs,
                                        ngtcp2_tstamp ts) {
  ngtcp2_cc_stat *ccs = cc->ccs;

  if (!default_cc_in_rcvry(cc, ts_sent)) {
    return;
  }
  ccs->recovery_start_time = ts;
  ccs->cwnd = (uint64_t)((double)ccs->cwnd * NGTCP2_LOSS_REDUCTION_FACTOR);
  ccs->cwnd = ngtcp2_max(ccs->cwnd, NGTCP2_MIN_CWND);
  ccs->ssthresh = ccs->cwnd;

  if (rcs->pto_count > NGTCP2_PERSISTENT_CONGESTION_THRESHOLD) {
    ccs->cwnd = NGTCP2_MIN_CWND;
  }

  ngtcp2_log_info(cc->log, NGTCP2_LOG_EVENT_RCV,
                  "reduce cwnd because of packet loss cwnd=%lu", ccs->cwnd);
}
