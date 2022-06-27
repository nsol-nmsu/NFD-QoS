/* -*- Mode: C++; c-file-style: "gnu"; indent-tabs-mode:nil -*- */
/*
 * Copyright (  C  ) 2020 New Mexico State University- Board of Regents
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

#ifndef NFD_DAEMON_FW_QoS_STRATEGY2_HPP
#define NFD_DAEMON_FW_QoS_STRATEGY2_HPP

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

/** \brief Enumerator used to indicate packet type.
 */
enum class PacketType {
  INTEREST = 0,
  DATA,
  NACK
};


/** 
 * @ingroup ndnQoS
 * \brief A forwarding strategy that uses WFQ to forward packets based on priority to enable QoS.
 *
 * Packets uses a hybrid approach, where high and medium priority packets are multicasted, 
 * while low priority level packets are unicasted. 
 * Each interface has three queues, one for each priority level. As packets arrive they
 * are enqueued onto the corresponding queue based on priority at the outgoing interface.
 */
class QosStrategy2 : public Strategy
                    , public ProcessNackTraits<QosStrategy2>
{
public:

  explicit
  QosStrategy2( Forwarder& forwarder, const Name& name = getStrategyName() );

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
   *  \param pitEntry The corresponding pit entry for the packet.
   *  \param inface The incoming interface.
   *  \param data The data packet we will be forwarding.
   *  \param outFace The outgoing interface.
   */
  void
  prioritySendData( const shared_ptr<pit::Entry>& pitEntry,
      const Face& inFace, const Data& data, const Face& outFace );

  /** \brief Send the given nack packet.
   *  \param pitEntry The corresponding pit entry for the packet.
   *  \param inface The incoming interface.
   *  \param nack The nack packet we will be forwarding.
   */
  void
  prioritySendNack( const shared_ptr<pit::Entry>& pitEntry,
      const Face& inFace, const lp::Nack& nack );

  /** \brief Send the given interest packet.
   *  \param pitEntry The corresponding pit entry for the packet.
   *  \param inface The incoming interface.
   *  \param interest The interest packet we will be forwarding.
   *  \param outFace The outgoing interface.
   */
  double
  getFaceProb(std::string namePrefix, uint32_t f, bool rel);


  void
  prioritySendInterest( const shared_ptr<pit::Entry>& pitEntry,
      const Face& inFace, const Interest& interest, const Face& outFace );

  void
  beforeSatisfyInterest(const shared_ptr<pit::Entry>& pitEntry,
                    const Face& inFace, const Data& data) override;

  void
  beforeExpirePendingInterest (const pit::Entry& entry);
  

  /** \brief Dequeue all eligible packets from respective queues and forward them.
   */
  void
  prioritySend();

  void
  bootstrap(std::string name);

  void
  initialize(std::string name, const shared_ptr<pit::Entry>& pitEntry);

protected:
  // set of ewma statistics for each face
  typedef struct {
    unordered_map<uint32_t, double> absLossRate; 
    unordered_map<uint32_t, double> relaLossRate; 
    double latency = 0;                 //latency stats (ewma mean - Mu)
    double latencyVariance = 0;         //variance for latency - Sigma
    bool bootstrapped = 0;		    //used to bootstrap latency variance and ewma
    uint32_t totalPacketsSent = 0;	    //used to keep track of bootstrapping statistics(need 3 to build stats)
  } FaceStats;


private:

  unordered_map<uint32_t, NdnPriorityTxQueue> m_tx_queue; //< @brief Hashtable that maps interface to their respective queues.
  friend ProcessNackTraits<QosStrategy2>;
  RetxSuppressionExponential m_retxSuppression;
  TokenBucket m_sender1; //< @brief Used to provide references to high priority token buckets to application layer.
  TokenBucket m_sender2; //< @brief Used to provide references to medium priority token buckets to application layer.
  TokenBucket m_sender3; //< @brief Used to provide references to low priority token buckets to application layer.
  int packetsDropped = 0;
  unordered_map<std::string, FaceStats> m_faceStats;

PUBLIC_WITH_TESTS_ELSE_PRIVATE:
  static const time::milliseconds RETX_SUPPRESSION_INITIAL;
  static const time::milliseconds RETX_SUPPRESSION_MAX;
};

} // namespace fw
} // namespace nfd

#endif // NFD_DAEMON_FW_QoS_STRATEGY_HPP
