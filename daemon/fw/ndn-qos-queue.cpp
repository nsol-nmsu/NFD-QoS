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

#include "ns3/log.h"
#include "ns3/simulator.h"
#include "ns3/assert.h"

using namespace std;
using namespace boost;

NS_LOG_COMPONENT_DEFINE ("ndn.QosQueue");

namespace nfd {

    namespace fw {

        QosQueue::QosQueue ()
            : m_maxQueueSize (QUEUE_SIZE), 
            m_iterator (m_queue.end())
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


        bool
        QosQueue::Enqueue (shared_ptr<Interest> queEntry)
        {
            if (m_maxQueueSize > m_queue.size())
            {
                m_queue.push_back (queEntry);
                return true;
            }
            else
            {
                cout << "Queue is full";
                return false;
            }
        }


        shared_ptr<Interest>
        QosQueue::Dequeue ()
        {
            if ( m_queue.empty () )
            {
                //queue is empty
                return 0;
            }
            else
            {
                //TODO: is auto is fine or Ptr<queItem::Interest> temp is required?
                shared_ptr<Interest> temp = m_queue.front (); 
                m_queue.pop_front ();

                return temp;
            }
        }

        bool
        QosQueue::IsEmpty () const
        {
            return m_queue.empty ();
        }

    } // namespace ndn
} // namespace ns3


