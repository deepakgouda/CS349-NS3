/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */
/*
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
 */

#include <fstream>
#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/stats-module.h"
#include "ns3/flow-monitor-helper.h"
#include "ns3/flow-monitor-module.h"


using namespace ns3;

NS_LOG_COMPONENT_DEFINE ("SeventhScriptExample");

// ===========================================================================
//
//         node 0                 node 1
//   +----------------+    +----------------+
//   |    ns-3 TCP    |    |    ns-3 TCP    |
//   +----------------+    +----------------+
//   |    10.1.1.1    |    |    10.1.1.2    |
//   +----------------+    +----------------+
//   | point-to-point |    | point-to-point |
//   +----------------+    +----------------+
//           |                     |
//           +---------------------+
//                5 Mbps, 2 ms
//
//
// We want to look at changes in the ns-3 TCP congestion window.  We need
// to crank up a flow and hook the CongestionWindow attribute on the socket
// of the sender.  Normally one would use an on-off application to generate a
// flow, but this has a couple of problems.  First, the socket of the on-off
// application is not created until Application Start time, so we wouldn't be
// able to hook the socket (now) at configuration time.  Second, even if we
// could arrange a call after start time, the socket is not public so we
// couldn't get at it.
//
// So, we can cook up a simple version of the on-off application that does what
// we want.  On the plus side we don't need all of the complexity of the on-off
// application.  On the minus side, we don't have a helper, so we have to get
// a little more involved in the details, but this is trivial.
//
// So first, we create a socket and do the trace connect on it; then we pass
// this socket into the constructor of our simple application which we then
// install in the source node.
// ===========================================================================
//
class MyApp : public Application
{
public:
  MyApp ();
  virtual ~MyApp ();

  /**
   * Register this type.
   * \return The TypeId.
   */
  static TypeId GetTypeId (void);
  void Setup (Ptr<Socket> socket, Address address, uint32_t packetSize, uint32_t nPackets, DataRate dataRate);

private:
  virtual void StartApplication (void);
  virtual void StopApplication (void);

  void ScheduleTx (void);
  void SendPacket (void);

  Ptr<Socket>     m_socket;
  Address         m_peer;
  uint32_t        m_packetSize;
  uint32_t        m_nPackets;
  DataRate        m_dataRate;
  EventId         m_sendEvent;
  bool            m_running;
  uint32_t        m_packetsSent;
};

MyApp::MyApp ()
  : m_socket (0),
    m_peer (),
    m_packetSize (0),
    m_nPackets (0),
    m_dataRate (0),
    m_sendEvent (),
    m_running (false),
    m_packetsSent (0)
{
}

MyApp::~MyApp ()
{
  m_socket = 0;
}

/* static */
TypeId MyApp::GetTypeId (void)
{
  static TypeId tid = TypeId ("MyApp")
    .SetParent<Application> ()
    .SetGroupName ("Tutorial")
    .AddConstructor<MyApp> ()
    ;
  return tid;
}

void
MyApp::Setup (Ptr<Socket> socket, Address address, uint32_t packetSize, uint32_t nPackets, DataRate dataRate)
{
  m_socket = socket;
  m_peer = address;
  m_packetSize = packetSize;
  m_nPackets = nPackets;
  m_dataRate = dataRate;
}

void
MyApp::StartApplication (void)
{
  m_running = true;
  m_packetsSent = 0;
  if (InetSocketAddress::IsMatchingType (m_peer))
    {
      m_socket->Bind ();
    }
  else
    {
      m_socket->Bind6 ();
    }
  m_socket->Connect (m_peer);
  SendPacket ();
}

void
MyApp::StopApplication (void)
{
  m_running = false;

  if (m_sendEvent.IsRunning ())
    {
      Simulator::Cancel (m_sendEvent);
    }

  if (m_socket)
    {
      m_socket->Close ();
    }
}

void
MyApp::SendPacket (void)
{
  Ptr<Packet> packet = Create<Packet> (m_packetSize);
  m_socket->Send (packet);

  if (++m_packetsSent < m_nPackets)
    {
      ScheduleTx ();
    }
}

void
MyApp::ScheduleTx (void)
{
  if (m_running)
    {
      Time tNext (Seconds (m_packetSize * 8 / static_cast<double> (m_dataRate.GetBitRate ())));
      m_sendEvent = Simulator::Schedule (tNext, &MyApp::SendPacket, this);
    }
}



void simulateCBR(NodeContainer &nodes, OnOffHelper &onOff, uint64_t start_time, uint64_t stop_time)
{
  onOff.SetAttribute ("DataRate", StringValue ("300Kbps"));
  onOff.SetAttribute ("StartTime", TimeValue (MilliSeconds (start_time)));
  onOff.SetAttribute ("StopTime", TimeValue (MilliSeconds (stop_time)));
  ApplicationContainer cbr;
  cbr.Add (onOff.Install (nodes.Get (0)));

  // Start CBR
  cbr.Start (MilliSeconds (start_time));
  cbr.Stop (MilliSeconds (stop_time));
}

static void
CwndChange (Ptr<OutputStreamWrapper> stream, uint32_t oldCwnd, uint32_t newCwnd)
{
  NS_LOG_UNCOND (Simulator::Now ().GetSeconds () << "\t" << newCwnd);
  *stream->GetStream () << Simulator::Now ().GetSeconds () << "\t" << oldCwnd << "\t" << newCwnd << std::endl;
}

// static void
// RxDrop (Ptr<PcapFileWrapper> file, Ptr<const Packet> p)
// {
//   NS_LOG_UNCOND ("RxDrop at " << Simulator::Now ().GetSeconds ());
//   file->Write (Simulator::Now (), p);
// }

static void
RxDrop (Ptr<OutputStreamWrapper> file, Ptr<const Packet> p)
{
  NS_LOG_UNCOND ("RxDrop at " << Simulator::Now ().GetSeconds ());
  *file->GetStream () << Simulator::Now ().GetSeconds () << std::endl;
}

int
main (int argc, char *argv[])
{

  // Take arguments from cmd
  CommandLine cmd;
  // cmd.AddValue ("useIpv6", "Use Ipv6", useV6);
  cmd.Parse (argc, argv);
  std::string transport_prot = argv[1];
  std::cout<<transport_prot<<std::endl;

  if (transport_prot.compare ("TcpNewReno") == 0)
  {
    Config::SetDefault ("ns3::TcpL4Protocol::SocketType", TypeIdValue (TcpNewReno::GetTypeId ()));
  }
  else if (transport_prot.compare ("TcpHybla") == 0)
  {
    Config::SetDefault ("ns3::TcpL4Protocol::SocketType", TypeIdValue (TcpHybla::GetTypeId ()));
  }
  else if (transport_prot.compare ("TcpWestwood") == 0)
  {
  // the default protocol type in ns3::TcpWestwood is WESTWOOD
  // for WESTWOODPLUS, add Config::SetDefault ("ns3::TcpWestwood::ProtocolType", EnumValue (TcpWestwood::WESTWOODPLUS));
    Config::SetDefault ("ns3::TcpL4Protocol::SocketType", TypeIdValue (TcpWestwood::GetTypeId ()));
    Config::SetDefault ("ns3::TcpWestwood::FilterType", EnumValue (TcpWestwood::TUSTIN));
  }
  else if (transport_prot.compare ("TcpScalable") == 0)
  {
    Config::SetDefault ("ns3::TcpL4Protocol::SocketType", TypeIdValue (TcpScalable::GetTypeId ()));
  }
  else if (transport_prot.compare ("TcpVegas") == 0)
  {
    Config::SetDefault ("ns3::TcpL4Protocol::SocketType", TypeIdValue (TcpVegas::GetTypeId ()));
  }
  else
  {
    NS_LOG_DEBUG ("Invalid TCP version");
    exit (1);
  }

  // Create nodes
  NodeContainer nodes;
  nodes.Create (2);

  // Create Link between 2 nodes
  PointToPointHelper pointToPoint;
  pointToPoint.SetDeviceAttribute ("DataRate", StringValue ("1Mbps"));
  pointToPoint.SetChannelAttribute ("Delay", StringValue ("10ms"));
  pointToPoint.SetQueue ("ns3::DropTailQueue", "MaxSize", StringValue ("10KB"));

  // Connect link with nodes
  NetDeviceContainer devices;
  devices = pointToPoint.Install (nodes);

  // Set error rate ??
  Ptr<RateErrorModel> em = CreateObject<RateErrorModel> ();
  em->SetAttribute ("ErrorRate", DoubleValue (0.00001));
  devices.Get (1)->SetAttribute ("ReceiveErrorModel", PointerValue (em));

  // Installs internet ? :P
  InternetStackHelper stack;
  stack.Install (nodes);


  uint16_t sinkPort = 8080;
  Address sinkAddress;
  Address anyAddress;
  std::string probeType;
  std::string tracePath;

  Ipv4AddressHelper address;
  address.SetBase ("10.1.1.0", "255.255.255.0");
  Ipv4InterfaceContainer interfaces = address.Assign (devices);
  sinkAddress = InetSocketAddress (interfaces.GetAddress (1), sinkPort);
  anyAddress = InetSocketAddress (Ipv4Address::GetAny (), sinkPort);
  probeType = "ns3::Ipv4PacketProbe";
  tracePath = "/NodeList/*/$ns3::Ipv4L3Protocol/Tx";

  PacketSinkHelper packetSinkHelper ("ns3::TcpSocketFactory", anyAddress);
  ApplicationContainer sinkApps = packetSinkHelper.Install (nodes.Get (1));
  sinkApps.Start (MilliSeconds (0.));
  sinkApps.Stop (MilliSeconds (1800));

  Ptr<Socket> ns3TcpSocket = Socket::CreateSocket (nodes.Get (0), TcpSocketFactory::GetTypeId ());

  Ptr<MyApp> app = CreateObject<MyApp> ();
  app->Setup (ns3TcpSocket, sinkAddress, 1040, 1000, DataRate ("1Mbps"));
  nodes.Get (0)->AddApplication (app);
  app->SetStartTime (MilliSeconds (0.1));
  app->SetStopTime (MilliSeconds (1800));


//
// Create CBR Agents
//
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

  // Create a packet sink to receive the packets
  PacketSinkHelper udp_sink ("ns3::UdpSocketFactory",InetSocketAddress(Ipv4Address::GetAny (), cbrPort));

  ApplicationContainer cbr_sink = udp_sink.Install (nodes.Get (1));
  cbr_sink.Start (MilliSeconds (0));
  cbr_sink.Stop (MilliSeconds (1800));

  AsciiTraceHelper asciiTraceHelper;
  Ptr<OutputStreamWrapper> stream = asciiTraceHelper.CreateFileStream ("./Output/"+transport_prot+".cwnd");
  ns3TcpSocket->TraceConnectWithoutContext ("CongestionWindow", MakeBoundCallback (&CwndChange, stream));

  Ptr<OutputStreamWrapper> dropStream = asciiTraceHelper.CreateFileStream ("./Output/"+transport_prot+".drop");
  devices.Get (1)->TraceConnectWithoutContext ("PhyRxDrop", MakeBoundCallback (&RxDrop, dropStream));

  // PcapHelper pcapHelper;
  // Ptr<PcapFileWrapper> file = pcapHelper.CreateFile ("./Output/"+transport_prot+".pcap", std::ios::out, PcapHelper::DLT_PPP);
  // devices.Get (1)->TraceConnectWithoutContext ("PhyRxDrop", MakeBoundCallback (&RxDrop, file));

  // Use GnuplotHelper to plot the packet byte count over time
  GnuplotHelper plotHelper;

  // Configure the plot.  The first argument is the file name prefix
  // for the output files generated.  The second, third, and fourth
  // arguments are, respectively, the plot title, x-axis, and y-axis labels
  plotHelper.ConfigurePlot ("./Output/"+transport_prot+"-packet-byte-count",
                            "Packet Byte Count vs. Time",
                            "Time (Seconds)",
                            "Packet Byte Count");

  // Specify the probe type, trace source path (in configuration namespace), and
  // probe output trace source ("OutputBytes") to plot.  The fourth argument
  // specifies the name of the data series label on the plot.  The last
  // argument formats the plot by specifying where the key should be placed.
  plotHelper.PlotProbe (probeType,
                        tracePath,
                        "OutputBytes",
                        "Packet Byte Count",
                        GnuplotAggregator::KEY_BELOW);

  // Use FileHelper to write out the packet byte count over time
  FileHelper fileHelper;

  // Configure the file to be written, and the formatting of output data.
  fileHelper.ConfigureFile ("./Output/"+transport_prot+"-packet-byte-count",
                            FileAggregator::FORMATTED);

  // Set the labels for this formatted output file.
  fileHelper.Set2dFormat ("%.3e\t%.0f");
  // fileHelper.Set2dFormat ("Time (Seconds) = %.3e\tPacket Byte Count = %.0f");

  // Specify the probe type, trace source path (in configuration namespace), and
  // probe output trace source ("OutputBytes") to write.
  fileHelper.WriteProbe (probeType,
                         tracePath,
                         "OutputBytes");

// Flow monitor
  Ptr<FlowMonitor> flowMonitor;
  FlowMonitorHelper flowHelper;
  flowMonitor = flowHelper.InstallAll();


//
// Now, do the actual simulation.
//
  Simulator::Stop (MilliSeconds (1800));
  Simulator::Run ();

  Ptr<Ipv4FlowClassifier> classifier = DynamicCast<Ipv4FlowClassifier> (flowHelper.GetClassifier ());
  std::map<FlowId, FlowMonitor::FlowStats> stats = flowMonitor->GetFlowStats ();
  std::cout << std::endl << "Flow monitor output:" << std::endl;
  std::cout << "Tx Packets:   " << stats[1].txPackets << std::endl;
  std::cout << "Tx Bytes:     " << stats[1].txBytes << std::endl;
  std::cout << "Offered Load: " << stats[1].txBytes * 8.0 / (stats[1].timeLastTxPacket.GetSeconds () - stats[1].timeFirstTxPacket.GetSeconds ()) / 1000000 << " Mbps" << std::endl;
  std::cout << "Rx Packets:   " << stats[1].rxPackets << std::endl;
  std::cout << "Rx Bytes:     " << stats[1].rxBytes<< std::endl;
  std::cout << "Throughput:   " << stats[1].rxBytes * 8.0 / (stats[1].timeLastRxPacket.GetSeconds () - stats[1].timeFirstRxPacket.GetSeconds ()) / 1000000 << " Mbps" << std::endl;
  std::cout << "Mean delay:   " << stats[1].delaySum.GetSeconds () / stats[1].rxPackets << std::endl;
  std::cout << "Mean jitter:  " << stats[1].jitterSum.GetSeconds () / (stats[1].rxPackets - 1) << std::endl;

  flowMonitor->SerializeToXmlFile("./Output/"+transport_prot + ".flowMonitor", true, true);

  Simulator::Destroy ();
  NS_LOG_INFO ("Done.");

  Ptr<PacketSink> sinkptr = DynamicCast<PacketSink> (sinkApps.Get (0));
  std::cout << "Total Bytes Received on FTP Channel: " << sinkptr->GetTotalRx () << std::endl;
  // for (uint16_t cbrIndex = 1; cbrIndex <= n_cbr; cbrIndex++)
  //   {
  //     sinkptr = DynamicCast<PacketSink> (cbrSinkApps[cbrIndex-1].Get (0));
  //     std::cout << "Total Bytes Received on CBR" << cbrIndex << " Channel: " << sinkptr->GetTotalRx () << std::endl;
  //   }

  return 0;
}
