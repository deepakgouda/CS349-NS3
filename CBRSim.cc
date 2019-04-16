#include <string>
#include <fstream>
#include "ns3/core-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/internet-module.h"
#include "ns3/applications-module.h"
#include "ns3/network-module.h"
#include "ns3/packet-sink.h"
#include "ns3/on-off-helper.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE ("CBR Simulation");

void simulateCBR(NodeContainer &nodes, OnOffHelper &onOff, uint64_t start_time, uint64_t stop_time)
{
  // uint64_t start_time = 200;
  // uint64_t stop_time = 1800;
  onOff.SetAttribute ("DataRate", StringValue ("300Kbps"));
  onOff.SetAttribute ("StartTime", TimeValue (MilliSeconds (start_time)));
  onOff.SetAttribute ("StartTime", TimeValue (MilliSeconds (stop_time)));
  ApplicationContainer cbr;
  cbr.Add (onOff.Install (nodes.Get (0)));

  // Start CBR
  cbr.Start (MilliSeconds (start_time));
  cbr.Stop (MilliSeconds (stop_time));
}

int main(int argc, char const *argv[])
{
	bool trace = false;
  // Set time resolution to one nanosecond
  Time::SetResolution (Time::NS);

	// Create 2 nodes
	NodeContainer nodes;
  nodes.Create (2);
  printf("2 Nodes created.\n");
  // NS_LOG_INFO ("2 Nodes created.");

	// Create point-to-point link with Bandwidth 1 Mbps and link delay 10 ms
  PointToPointHelper pointToPoint;
  pointToPoint.SetDeviceAttribute ("DataRate", StringValue ("1Mbps"));
  pointToPoint.SetChannelAttribute ("Delay", StringValue ("10ms"));
  printf("Channel created.\n");

  // Attach the link to the nodes
	NetDeviceContainer devices;
  devices = pointToPoint.Install (nodes);
  printf("Link attached.\n");

	// Does what?
	InternetStackHelper internet;
  internet.Install (nodes);

  // Assign IP Addresses to the nodes
  Ipv4AddressHelper ipv4;
  ipv4.SetBase ("192.168.0.0", "255.255.0.0");
  Ipv4InterfaceContainer interfaces = ipv4.Assign (devices);
  printf("IP assigned.\n");

  // Trace packets
  trace = true;
  if(trace)
  {
    pointToPoint.EnablePcapAll ("CBR-TCP-Simulation", true);
    printf("Packets captured.\n");
  }

  // Setup the FTP Conenction
  Address TxAddress(InetSocketAddress(interfaces.GetAddress(1),10));
  OnOffHelper clientHelper ("ns3::TcpSocketFactory", TxAddress);
  ApplicationContainer ftp = clientHelper.Install (nodes.Get (0));

  ftp.Start (Seconds (0.0));
  ftp.Stop (Seconds (1.8));

  // Setup the CBR Connection
  uint16_t cbrPort = 8000;
  uint16_t packetSize = 512;
  OnOffHelper onOff ("ns3::UdpSocketFactory", InetSocketAddress (interfaces.GetAddress(1), cbrPort));
  onOff.SetAttribute ("PacketSize", UintegerValue (packetSize));
  onOff.SetAttribute ("OnTime",  StringValue ("ns3::ConstantRandomVariable[Constant=1]"));
  onOff.SetAttribute ("OffTime", StringValue ("ns3::ConstantRandomVariable[Constant=0]"));

  // CBR1 :  node 0 -> node 1 : 200 ms - 1800 ms
  simulateCBR(nodes, onOff, 200, 1800);
  
  // CBR2 :  node 0 -> node 1 : 400 ms - 1800 ms
  simulateCBR(nodes, onOff, 400, 1800);
  
  // CBR3 :  node 0 -> node 1 : 600 ms - 1200 ms
  simulateCBR(nodes, onOff, 600, 1200);
  
  // CBR4 :  node 0 -> node 1 : 800 ms - 1400 ms
  simulateCBR(nodes, onOff, 800, 1400);
  
  // CBR5 :  node 0 -> node 1 : 1000 ms - 1600 ms
  simulateCBR(nodes, onOff, 1000, 1600);

  // Start Simulator
  Simulator::Stop(MilliSeconds(1800));
  Simulator::Run ();

  // Cleanup
  Simulator::Destroy ();


  printf("Done\n");

	return 0;
}
