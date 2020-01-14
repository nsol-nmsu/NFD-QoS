/* -*- Mode: C++; c-file-style: "gnu"; indent-tabs-mode:nil -*- */
/*
 * Copyright (c) 2012 University of California, Los Angeles
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation;
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 */

#ifndef NDN_PRIORITY_TX_QUEUE_H
#define NDN_PRIORITY_TX_QUEUE_H

#include <iostream>
#include <list>
#include <limits>
//#include "ns3/log.h"
//#include <time.hpp>
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
    NdnPriorityTxQueue ();

    float 
    GetFlowRate(QosQueue *queue);

    void
    UpdateTime(ndn::Block packet, QosQueue *queue);

    QosQueue*
    SelectQueueToSend();

    void
    DoEnqueue(QueueItem item, uint32_t dscp_value );

    QueueItem
    DoDequeue();

public:

    QosQueue m_highPriorityQueue;
    QosQueue m_mediumPriorityQueue;
    QosQueue m_lowPriorityQueue;
};

}// namespace fw
}// namespace nfd
 
#endif // NDN_PRIORITY_TX_QUEUE_H
