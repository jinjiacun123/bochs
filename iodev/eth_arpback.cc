/////////////////////////////////////////////////////////////////////////
// $Id: eth_arpback.cc,v 1.26 2011/01/24 20:35:51 vruppert Exp $
/////////////////////////////////////////////////////////////////////////
//
//  Copyright (C) 2001-2011  The Bochs Project
//
//  This library is free software; you can redistribute it and/or
//  modify it under the terms of the GNU Lesser General Public
//  License as published by the Free Software Foundation; either
//  version 2 of the License, or (at your option) any later version.
//
//  This library is distributed in the hope that it will be useful,
//  but WITHOUT ANY WARRANTY; without even the implied warranty of
//  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
//  Lesser General Public License for more details.
//
//  You should have received a copy of the GNU Lesser General Public
//  License along with this library; if not, write to the Free Software
//  Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301 USA
//

// eth_arpback.cc  - basic ethernet packetmover, only responds to ARP

// Various networking docs:
// http://www.graphcomp.com/info/rfc/
// rfc0826: arp
// rfc0903: rarp

// Define BX_PLUGGABLE in files that can be compiled into plugins.  For
// platforms that require a special tag on exported symbols, BX_PLUGGABLE
// is used to know when we are exporting symbols and when we are importing.
#define BX_PLUGGABLE

#include "iodev.h"

#if BX_NETWORKING && defined(ETH_ARPBACK)

#include "eth.h"
#include "crc32.h"
#include "eth_packetmaker.h"
#define LOG_THIS netdev->


//static const Bit8u external_mac[]={0xB0, 0xC4, 0x20, 0x20, 0x00, 0x00, 0x00};
//static const Bit8u internal_mac[]={0xB0, 0xC4, 0x20, 0x00, 0x00, 0x00, 0x00};
//static const Bit8u external_ip[]={ 192, 168, 0, 2, 0x00 };
//static const Bit8u ethtype_arp[]={0x08, 0x06, 0x00};


//
//  Define the class. This is private to this module
//
class bx_arpback_pktmover_c : public eth_pktmover_c {
public:
  bx_arpback_pktmover_c(const char *netif, const char *macaddr,
                        eth_rx_handler_t rxh,
                        bx_devmodel_c *dev);
  void sendpkt(void *buf, unsigned io_len);
private:
  int rx_timer_index;
  static void rx_timer_handler(void *);
  void rx_timer(void);
  FILE *pktlog, *pktlog_txt;
  //Bit8u arpbuf[MAX_FRAME_SIZE];
  //Bit32u buflen;
  //bx_bool bufvalid;
  //CRC_Generator mycrc;
  eth_ETHmaker packetmaker;
};


//
//  Define the static class that registers the derived pktmover class,
// and allocates one on request.
//
class bx_arpback_locator_c : public eth_locator_c {
public:
  bx_arpback_locator_c(void) : eth_locator_c("arpback") {}
protected:
  eth_pktmover_c *allocate(const char *netif, const char *macaddr,
                           eth_rx_handler_t rxh,
                           bx_devmodel_c *dev, const char *script) {
    return (new bx_arpback_pktmover_c(netif, macaddr, rxh, dev, script));
  }
} bx_arpback_match;


//
// Define the methods for the bx_arpback_pktmover derived class
//

// the constructor
bx_arpback_pktmover_c::bx_arpback_pktmover_c(const char *netif,
                                             const char *macaddr,
                                             eth_rx_handler_t rxh,
                                             bx_devmodel_c *dev,
                                             const char *script)
{
  this->netdev = dev;
  BX_INFO(("arpback network driver"));
  this->rx_timer_index =
    bx_pc_system.register_timer(this, this->rx_timer_handler, 1000,
                                1, 1, "eth_arpback"); // continuous, active
  this->rxh   = rxh;
  //bufvalid=0;
  packetmaker.init();
#if BX_ETH_NULL_LOGGING
  // Start the rx poll
  // eventually Bryce wants txlog to dump in pcap format so that
  // tcpdump -r FILE can read it and interpret packets.
  pktlog = fopen("ne2k-pkt.log", "wb");
  if (!pktlog) BX_PANIC(("open ne2k-pkt.log failed"));
  pktlog_txt = fopen("ne2k-pktlog.txt", "wb");
  if (!pktlog_txt) BX_PANIC(("open ne2k-pktlog.txt failed"));
  fprintf(pktlog_txt, "arpback packetmover readable log file\n");
  fprintf(pktlog_txt, "net IF = %s\n", netif);
  fprintf(pktlog_txt, "MAC address = ");
  for (int i=0; i<6; i++)
    fprintf(pktlog_txt, "%02x%s", 0xff & macaddr[i], i<5?":" : "");
  fprintf(pktlog_txt, "\n--\n");
  fflush(pktlog_txt);
#endif
}

void
bx_arpback_pktmover_c::sendpkt(void *buf, unsigned io_len)
{
  if(io_len<BX_PACKET_BUFSIZE) {
    eth_packet barney;
    memcpy(barney.buf,buf,io_len);
    barney.len=io_len;
    if(packetmaker.ishandler(barney)) {
      packetmaker.sendpacket(barney);
    }
    /*
    if(( (!memcmp(buf, external_mac, 6)) || (!memcmp(buf, broadcast_macaddr, 6)) )
       && (!memcmp(((Bit8u *)buf)+12, ethtype_arp, 2)) ) {
      Bit32u tempcrc;
      memcpy(arpbuf,buf,io_len); //move to temporary buffer
      memcpy(arpbuf, arpbuf+6, 6); //set destination to sender
      memcpy(arpbuf+6, external_mac, 6); //set sender to us
      memcpy(arpbuf+32, arpbuf+22, 10); //move destination to sender
      memcpy(arpbuf+22, external_mac, 6); //set sender to us
      memcpy(arpbuf+28, external_ip, 4); //set sender to us
      arpbuf[21]=2; //make this a reply and not a request
      tempcrc=mycrc.get_CRC(arpbuf,io_len);
      memcpy(arpbuf+io_len, &tempcrc, 4);
      buflen=io_len;//+4
      bufvalid=1;
    }
    */
  }
#if BX_ETH_NULL_LOGGING
  BX_DEBUG(("sendpkt length %u", io_len));
  // dump raw bytes to a file, eventually dump in pcap format so that
  // tcpdump -r FILE can interpret them for us.
  int n = fwrite (buf, io_len, 1, pktlog);
  if (n != 1) BX_ERROR (("fwrite to pktlog failed, length %u", io_len));
  // dump packet in hex into an ascii log file
  write_pktlog_txt(pktlog_txt, (const Bit8u *)buf, io_len, 0);
  // flush log so that we see the packets as they arrive w/o buffering
  fflush(pktlog);
#endif
}

void bx_arpback_pktmover_c::rx_timer_handler (void * this_ptr)
{
#if BX_ETH_NULL_LOGGING
  BX_DEBUG(("rx_timer_handler"));
#endif
  bx_arpback_pktmover_c *class_ptr = ((bx_arpback_pktmover_c *)this_ptr);

  class_ptr->rx_timer();
}

void bx_arpback_pktmover_c::rx_timer (void)
{
  eth_packet rubble;

  if (packetmaker.getpacket(rubble)) {
#if BX_ETH_NULL_LOGGING
    write_pktlog_txt(pktlog_txt, rubble.buf, rubble.len, 1);
#endif
    (*rxh)(this->netdev, rubble.buf, rubble.len);
  }
}

#endif /* if BX_NETWORKING && defined(ETH_ARPBACK) */
