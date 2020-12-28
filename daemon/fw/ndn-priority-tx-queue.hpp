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

#ifndef NDN_PRIORITY_TX_QUEUE_H
#define NDN_PRIORITY_TX_QUEUE_H

#include <list>
#include <limits>
#include "ns3/log.h"
#include "ndn-qos-queue.hpp"

#include <ctime> 
#include <chrono>

#define PACKET_SIZE 1024
#define TOTAL_QUEUES 3

using namespace std;

namespace nfd {
namespace fw {

class NdnPriorityTxQueue : private QosQueue
{

public:

  //constructor
  NdnPriorityTxQueue();

  /** \brief Find flow rate of the given queue.
   *  \param queue the queue for which we will find the flow rate.
   */
  float
  GetFlowRate( QosQueue *queue );

  /** \brief Update the time of the given packet within the queue.
   *  \param packet the packet we are updating.
   *  \param queue the queue which has the packet of interest.
   */
  void
  UpdateTime( ndn::Block packet, QosQueue *queue );

  /** \brief Use WFQ algorithm to select the next queue to send from taking the token count into account.
   *  \param highTokens the number of tokens the high priorty queue has.
   *  \param midTokens the number of tokens the mid priorty queue has.
   *  \param lowTokens the number of tokens the low priorty queue has.
   */
  int
  SelectQueueToSend( double highTokens, double midTokens, double lowTokens );

  /** \brief Push the given packet and corresponding metainfo onto a queue.
   *  \param item the packet and its metainfo, incoming face, pit entry, etc.
   *  \param dscp_value the value which determins packet priorty.
   */
  bool
  DoEnqueue( QueueItem item, uint32_t dscp_value );

  /** \brief Dequeue a packet from the indicated queue.
   *  \param choice an int value repersenting one of the three queues.
   */
  QueueItem
  DoDequeue( int choice );

  bool
  IsEmpty();

  int
  tokenReqHig();

  int
  tokenReqMid();

  int
  tokenReqLow();

public:

  QosQueue m_highPriorityQueue;
  QosQueue m_mediumPriorityQueue;
  QosQueue m_lowPriorityQueue;
};

}// namespace fw
}// namespace nfd
 
#endif // NDN_PRIORITY_TX_QUEUE_H
