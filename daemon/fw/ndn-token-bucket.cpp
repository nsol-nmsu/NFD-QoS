/* -*- Mode: C++; c-file-style: "gnu"; indent-tabs-mode:nil -*- */
/*
 * Copyright (C) 2020 New Mexico State University- Board of Regents
 *
 * George Torres, Anju Kunnumpurathu James
 * See AUTHORS.md for complete list of authors and contributors.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.

 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.

 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 */

#include "ndn-token-bucket.hpp"
#include "ns3/simulator.h"
#include "../../../helper/ndn-scenario-helper.hpp"

namespace nfd {
namespace fw {

TokenBucket::TokenBucket()
{
    m_capacity = 0;
    m_atCapacity = false;
    hasFaces = false;
}

void
TokenBucket::addToken()
{ 
  std::unordered_map<uint32_t,double >::iterator itt = m_tokens.begin();
  bool callsend = false;
  m_atCapacity = true;
  while(itt != m_tokens.end()) {
    if (itt->second < m_capacity) {
      m_atCapacity = false;
      m_tokens[itt->first]++;
    }

    if (m_need[itt->first] != 0 && m_tokens[itt->first] >= m_need[itt->first]) {
      callsend = true;
    }

    itt++;
  }
  ns3::Ptr<ns3::Node> node= ns3::NodeContainer::GetGlobal().Get( ns3::Simulator::GetContext() );
  if(callsend){
  	send();
  }
}

void
TokenBucket::consumeToken(double tokens, uint32_t face)
{
  if(m_tokens.find(face) == m_tokens.end()) {
    m_tokens[face] = m_capacity;
  }

  m_tokens[face] = m_tokens[face] - tokens;
  if(m_atCapacity){
     m_atCapacity = false;
     noLongerAtCapacity();
  }
}


} // namespace fw
} // namespace nfd
