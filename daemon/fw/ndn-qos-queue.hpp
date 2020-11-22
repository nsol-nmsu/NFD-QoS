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

#ifndef QOS_QUEUE_H
#define QOS_QUEUE_H

#include <list>
#include <ndn-cxx/encoding/block.hpp>
#include <NFD/daemon/table/pit-entry.hpp>
#include <NFD/daemon/face/face.hpp>
#include "ns3/log.h"

#define QUEUE_SIZE 10

using namespace std;

namespace nfd {
namespace fw {

enum QosPacketType
{
    INVALID = -1,
    INTEREST = 0,
    DATA,
    NACK
};

struct QueueItem
{
    ndn::Block wireEncode;
    QosPacketType packetType;
    shared_ptr<pit::Entry> pitEntry;
    const Face* inface;
    const Face* outface;

    QueueItem() : wireEncode(0),
    packetType(INVALID),
    pitEntry(NULL),
    inface(NULL),
    outface(NULL) { }
    QueueItem(const shared_ptr<pit::Entry>* pe) : wireEncode(0),
    packetType(INVALID),
    inface(NULL),
    outface(NULL) {
        shared_ptr<pit::Entry> temp(*pe);
        pitEntry = temp;
    }
};


class QosQueue
{

public:

    //constructor
    QosQueue ();

    //Set the queue size
    void
    SetMaxQueueSize (uint32_t size);

    //Get queue size
    uint32_t
    GetMaxQueueSize () const;

    void 
    SetWeight (float weight);

    float
    GetWeight ();

    void 
    SetLastVirtualFinishTime (uint64_t lvft);

    uint64_t
    GetLastVirtualFinishTime ();

    bool
    Enqueue (QueueItem item);

    QueueItem
    Dequeue ();

    void
    DisplayQueue ();

    //Check whether queue is empty
    bool
    IsEmpty () const;

    QueueItem
    GetFirstElement();

public:

    typedef std::list<QueueItem> Queue;

private:

    uint32_t m_maxQueueSize;
    float m_weight;
    uint64_t m_lastVirtualFinishTime;
    Queue m_queue;
};

}// namespace fw
}// namespace nfd
 
#endif // QOS_QUEUE_H
