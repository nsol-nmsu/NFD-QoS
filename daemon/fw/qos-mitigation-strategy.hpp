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

#ifndef NFD_DAEMON_FW_QoS_MITIGATION_HPP
#define NFD_DAEMON_FW_QoS_MITIGATION_HPP

#include "qos-strategy.hpp"
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


/** 
 * @ingroup ndnQoS
 * \brief A forwarding strategy that uses WFQ to forward packets based on priority to enable QoS.
 *
 * Packets uses a hybrid approach, where high and medium priority packets are multicasted, 
 * while low priority level packets are unicasted. 
 * Each interface has three queues, one for each priority level. As packets arrive they
 * are enqueued onto the corresponding queue based on priority at the outgoing interface.
 */
class QosMitigation : public QosStrategy
{
public:

  explicit
  QosMitigation( Forwarder& forwarder, const Name& name = getStrategyName() );

  static const Name&
  getStrategyName();

  void
  setUp() override;

  void
  beforeSatisfyInterest(const shared_ptr<pit::Entry>& pitEntry,
                    const Face& inFace, const Data& data) override;

  void
  beforeExpirePendingInterest (const pit::Entry& entry);
  
  void
  findSusFlow ();

  int
  getPrType(Name pkt) override;

private:

  TokenBucket m_sender4; //< @brief Used to provide references to mitigation token buckets to application layer.
  vector<Name> lastHundred;
  unordered_map<Name, double> monitored;
  double totalLossRate = 0;
  double beta = 0.833;

};

} // namespace fw
} // namespace nfd

#endif // NFD_DAEMON_FW_QoS_STRATEGY_HPP
