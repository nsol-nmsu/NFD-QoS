/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */
/**
 * Copyright (c) 2014-2015,  Regents of the University of California,
 *                           Arizona Board of Regents,
 *                           Colorado State University,
 *                           University Pierre & Marie Curie, Sorbonne University,
 *                           Washington University in St. Louis,
 *                           Beijing Institute of Technology,
 *                           The University of Memphis.
 *
 * This file is part of NFD (Named Data Networking Forwarding Daemon).
 * See AUTHORS.md for complete list of NFD authors and contributors.
 *
 * NFD is free software: you can redistribute it and/or modify it under the terms
 * of the GNU General Public License as published by the Free Software Foundation,
 * either version 3 of the License, or (at your option) any later version.
 *
 * NFD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY;
 * without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR
 * PURPOSE.  See the GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along with
 * NFD, e.g., in COPYING.md file.  If not, see <http://www.gnu.org/licenses/>.
 */

#include "ndn-token-bucket.hpp"

namespace nfd {
namespace fw {

TokenBucket::TokenBucket()
  : m_capacity(80.0)
  , m_fillRate(2.0)
  , m_first(true)
  , m_timestamp(ns3::Simulator::Now())
{
}

TokenBucket::TokenBucket(double capacity, double fillRate)
  : m_capacity(capacity)
  , m_fillRate(fillRate)
  , m_first(true)
  , m_timestamp(ns3::Simulator::Now())
{ 
}

double
TokenBucket::GetTokens()
{ 
  ns3::Time timeNow = ns3::Simulator::Now();
  
  if (m_first == true) {
        m_tokens = m_capacity;
        m_first = false;
  }
  else {
        //Time timeNow = ns3::Simulator::Now();
        double delta = m_fillRate * (timeNow - m_timestamp).GetSeconds();
        
        //Check to make sure tokens are not generated beyong specified capacity
        if (m_capacity < (m_tokens + delta)) {
                m_tokens = m_capacity;
        }
        else {  
                m_tokens = m_tokens + delta;
        }
  }
  
  m_timestamp = timeNow;
  return m_tokens;
}

double
TokenBucket::ConsumeTokens(double tokens)
{

  if (tokens <= m_tokens) {
    m_tokens -= tokens;
    return tokens;
  }
  else {
    return -1.0;
  }

}


} // namespace fw
} // namespace nfd
