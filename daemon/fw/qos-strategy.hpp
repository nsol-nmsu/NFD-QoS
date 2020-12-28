/* -*- Mode: C++; c-file-style: "gnu"; indent-tabs-mode:nil -*- */
/*
 * Copyright (  C  ) 2020 New Mexico State University
 *
 * George Torres, Anju Kunnumpurathu James
 * See AUTHORS.md for complete list of authors and contributors.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (  at your option  ) any later version.

 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.

 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 */

#ifndef NFD_DAEMON_FW_QoS_STRATEGY_HPP
#define NFD_DAEMON_FW_QoS_STRATEGY_HPP

#include "strategy.hpp"
#include "process-nack-traits.hpp"
#include "retx-suppression-exponential.hpp"

#include "ndn-token-bucket.hpp"
#include "ns3/node.h"
#include "ns3/ndnSIM/model/ndn-global-router.hpp"
#include "ns3/ndnSIM/helper/boost-graph-ndn-global-routing-helper.hpp"
#include "ndn-priority-tx-queue.hpp"
#include <unordered_map>


namespace nfd {
namespace fw {

/** \brief Indicates the state of a transport.
 */
enum class PacketType {
  INTEREST = 0,
  DATA,
  NACK
};


/** \brief A forwarding strategy that forwards Interest to all FIB nexthops.
 */
class QosStrategy : public Strategy
                    , public ProcessNackTraits<QosStrategy>
{
public:

  explicit
  QosStrategy( Forwarder& forwarder, const Name& name = getStrategyName() );

  static const Name&
  getStrategyName();

  void
  afterReceiveInterest( const Face& inFace, const Interest& interest,
      const shared_ptr<pit::Entry>& pitEntry ) override;

  void
  afterReceiveNack( const Face& inFace, const lp::Nack& nack,
      const shared_ptr<pit::Entry>& pitEntry ) override;

  void
  afterReceiveData( const shared_ptr<pit::Entry>& pitEntry,
      const Face& inFace, const Data& data );

  /** \brief Send the given data packet.
   *  \param pitEntry the corresponding pit entry for the packet.
   *  \param inface the incoming interface.
   *  \param data the data packet we will be forwarding.
   *  \param outFace the outgoing interface.
   */
  void
  prioritySendData( const shared_ptr<pit::Entry>& pitEntry,
      const Face& inFace, const Data& data, const Face& outFace );

  /** \brief Send the given nack packet.
   *  \param pitEntry the corresponding pit entry for the packet.
   *  \param inface the incoming interface.
   *  \param nack the nack packet we will be forwarding.
   */
  void
  prioritySendNack( const shared_ptr<pit::Entry>& pitEntry,
      const Face& inFace, const lp::Nack& nack );

  /** \brief Send the given interest packet.
   *  \param pitEntry the corresponding pit entry for the packet.
   *  \param inface the incoming interface.
   *  \param interest the interest packet we will be forwarding.
   *  \param outFace the outgoing interface.
   */
  void
  prioritySendInterest( const shared_ptr<pit::Entry>& pitEntry,
      const Face& inFace, const Interest& interest, const Face& outFace );

  /** \brief Dequeue all eligible packets from thier respective queues and forward them.
   */
  void
  prioritySend();

private:

  unordered_map<uint32_t, NdnPriorityTxQueue> m_tx_queue;
  friend ProcessNackTraits<QosStrategy>;
  RetxSuppressionExponential m_retxSuppression;
  TokenBucket m_sender1;
  TokenBucket m_sender2;
  TokenBucket m_sender3;
  int packetsDropped = 0;

PUBLIC_WITH_TESTS_ELSE_PRIVATE:
  static const time::milliseconds RETX_SUPPRESSION_INITIAL;
  static const time::milliseconds RETX_SUPPRESSION_MAX;
};

} // namespace fw
} // namespace nfd

#endif // NFD_DAEMON_FW_QoS_STRATEGY_HPP
