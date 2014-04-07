/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */
/**
 * Copyright (c) 2014  Regents of the University of California,
 *                     Arizona Board of Regents,
 *                     Colorado State University,
 *                     University Pierre & Marie Curie, Sorbonne University,
 *                     Washington University in St. Louis,
 *                     Beijing Institute of Technology
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
 **/

#ifndef NFD_FACE_UDP_FACTORY_HPP
#define NFD_FACE_UDP_FACTORY_HPP

#include "protocol-factory.hpp"
#include "udp-channel.hpp"
#include "multicast-udp-face.hpp"


namespace nfd {

// @todo The multicast support for ipv6 must be implemented

class UdpFactory : public ProtocolFactory
{
public:
  /**
   * \brief Exception of UdpFactory
   */
  struct Error : public ProtocolFactory::Error
  {
    Error(const std::string& what) : ProtocolFactory::Error(what) {}
  };

  explicit
  UdpFactory(const std::string& defaultPort = "6363");

  /**
   * \brief Create UDP-based channel using udp::Endpoint
   *
   * udp::Endpoint is really an alias for boost::asio::ip::udp::endpoint.
   *
   * If this method called twice with the same endpoint, only one channel
   * will be created.  The second call will just retrieve the existing
   * channel.
   *
   * If a multicast face is already active on the same local endpoint,
   * the creation fails and an exception is thrown
   *
   * Once a face is created, if it doesn't send/receive anything for
   * a period of time equal to timeout, it will be destroyed
   * @todo this funcionality has to be implemented
   *
   * \returns always a valid pointer to a UdpChannel object, an exception
   *          is thrown if it cannot be created.
   *
   * \throws UdpFactory::Error
   *
   * \see http://www.boost.org/doc/libs/1_42_0/doc/html/boost_asio/reference/ip__udp/endpoint.html
   *      for details on ways to create udp::Endpoint
   */
  shared_ptr<UdpChannel>
  createChannel(const udp::Endpoint& localEndpoint,
         const time::seconds& timeout = time::seconds(600));

  /**
   * \brief Create UDP-based channel using specified host and port number
   *
   * This method will attempt to resolve the provided host and port numbers
   * and will throw UdpFactory::Error when channel cannot be created.
   *
   * Note that this call will **BLOCK** until resolution is done or failed.
   *
   * If localHost is a IPv6 address of a specific device, it must be in the form:
   * ip address%interface name
   * Example: fe80::5e96:9dff:fe7d:9c8d%en1
   * Otherwise, you can use ::
   *
   * Once a face is created, if it doesn't send/receive anything for
   * a period of time equal to timeout, it will be destroyed
   * @todo this funcionality has to be implemented
   *
   * \throws UdpChannel::Error if the bind on the socket fails
   * \throws UdpFactory::Error
   */
  shared_ptr<UdpChannel>
  createChannel(const std::string& localHost,
         const std::string& localPort,
         const time::seconds& timeout = time::seconds(600));

  /**
   * \brief Create MulticastUdpFace using udp::Endpoint
   *
   * udp::Endpoint is really an alias for boost::asio::ip::udp::endpoint.
   *
   * The face will join the multicast group
   *
   * If this method called twice with the same endpoint and group, only one face
   * will be created.  The second call will just retrieve the existing
   * channel.
   *
   * If an unicast face is already active on the same local NIC and port, the
   * creation fails and an exception is thrown
   *
   * \returns always a valid pointer to a MulticastUdpFace object, an exception
   *          is thrown if it cannot be created.
   *
   * \throws UdpFactory::Error
   *
   * \see http://www.boost.org/doc/libs/1_42_0/doc/html/boost_asio/reference/ip__udp/endpoint.html
   *      for details on ways to create udp::Endpoint
   */
  shared_ptr<MulticastUdpFace>
  createMulticastFace(const udp::Endpoint& localEndpoint,
                      const udp::Endpoint& multicastEndpoint);

  shared_ptr<MulticastUdpFace>
  createMulticastFace(const std::string& localIp,
                      const std::string& multicastIp,
                      const std::string& multicastPort);

  // from Factory
  virtual void
  createFace(const FaceUri& uri,
             const FaceCreatedCallback& onCreated,
             const FaceConnectFailedCallback& onConnectFailed);

protected:
  typedef std::map< udp::Endpoint, shared_ptr<MulticastUdpFace> > MulticastFaceMap;

  /**
   * \brief Keeps tracking of the MulticastUdpFace created
   */
  MulticastFaceMap m_multicastFaces;

private:

  void
  prohibitEndpoint(const udp::Endpoint& endpoint);

  void
  prohibitAllIpv4Endpoints(const uint16_t port);

  void
  prohibitAllIpv6Endpoints(const uint16_t port);

  void
  afterFaceFailed(udp::Endpoint& endpoint);

  /**
   * \brief Look up UdpChannel using specified local endpoint
   *
   * \returns shared pointer to the existing UdpChannel object
   *          or empty shared pointer when such channel does not exist
   *
   * \throws never
   */
  shared_ptr<UdpChannel>
  findChannel(const udp::Endpoint& localEndpoint);


  /**
   * \brief Look up multicast UdpFace using specified local endpoint
   *
   * \returns shared pointer to the existing multicast MulticastUdpFace object
   *          or empty shared pointer when such face does not exist
   *
   * \throws never
   */
  shared_ptr<MulticastUdpFace>
  findMulticastFace(const udp::Endpoint& localEndpoint);

  void
  continueCreateFaceAfterResolve(const udp::Endpoint& endpoint,
                                 const FaceCreatedCallback& onCreated,
                                 const FaceConnectFailedCallback& onConnectFailed);

  typedef std::map< udp::Endpoint, shared_ptr<UdpChannel> > ChannelMap;
  ChannelMap m_channels;

  std::string m_defaultPort;

  std::set<udp::Endpoint> m_prohibitedEndpoints;
};

} // namespace nfd

#endif // NFD_FACE_UDP_FACTORY_HPP
