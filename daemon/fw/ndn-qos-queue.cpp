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

#include "ndn-qos-queue.hpp"

NS_LOG_COMPONENT_DEFINE ("ndn.QosQueue");

namespace nfd {
namespace fw {

QosQueue::QosQueue()
    : m_maxQueueSize (QUEUE_SIZE),
    m_weight(0.0),
    m_lastVirtualFinishTime(0.0)
{
}

void
QosQueue::SetMaxQueueSize (uint32_t size)
{
    m_maxQueueSize = size;
}

uint32_t
QosQueue::GetMaxQueueSize () const
{
    return m_maxQueueSize;
}

void
QosQueue::SetWeight (float weight)
{
    m_weight = weight;
}

float
QosQueue::GetWeight ()
{
    return m_weight;
}

void
QosQueue::SetLastVirtualFinishTime (uint64_t lvft)
{
    m_lastVirtualFinishTime = lvft;
}

uint64_t
QosQueue::GetLastVirtualFinishTime()
{
    return m_lastVirtualFinishTime;
}

void
QosQueue::Enqueue (QueueItem item)
{
    if (m_queue.size() < GetMaxQueueSize())
    {
        std::cout << "Enqueing Item: "<< endl;
        std::cout << "\tWireWncode: " << item.wireEncode;
        std::cout << "\tpacketType: " << item.packetType;
        std::cout << "\tpitEntry: " << item.pitEntry;
        std::cout << "\tinterface: " << item.interface->getId()  << endl;

        m_queue.push_back(item);

    } else
    {
        std::cout << "Enqueue failed. Queue full!!!!" << endl;
    }
}

QueueItem
QosQueue::Dequeue ()
{
    struct QueueItem m_item;
    if (!m_queue.empty())
    {
        m_item = m_queue.front();
        m_queue.pop_front();

        cout << "Dequeing item: " << std::endl;
        std::cout << "\tWireWncode: " << m_item.wireEncode;
        std::cout << "\tpacketType: " << m_item.packetType;
        std::cout << "\tpitEntry: " << m_item.pitEntry;
        std::cout << "\tinterface: " << m_item.interface->getId() << std::endl;

    } else
    {
        cout << "Dequeue failed. Queue is empty!!!" << endl;
    }
    return m_item;
}

void 
QosQueue::DisplayQueue ()
{
    std::cout << "QosQueue::DisplayQueue()" << std::endl;
    if (!m_queue.empty())
    {
            std::cout <<"Queue Items: "<< std::endl;
            for (auto it = m_queue.cbegin(); it != m_queue.cend(); ++it)
            {
                std::cout << "\tWireWncode: " << it->wireEncode;
                std::cout << "\tpacketType: " << it->packetType;
                std::cout << "\tpitEntry: " << it->pitEntry;
                std::cout << "\tinterface: " << it->interface->getId() << std::endl;
            }
    } else
    {
        std::cout << "Queue is empty!!!" << std::endl;
    }
}

bool
QosQueue::IsEmpty () const
{
    return m_queue.empty ();
}

QueueItem
QosQueue::GetFirstElement()
{
    struct QueueItem m_item;

    if (!m_queue.empty())
    {
        m_item = m_queue.front();
    } else
    {
        std::cout << "Queue is Empty !!!!" << endl;
    }

    return m_item;
}


} // namespace ndn
} // namespace ns3

