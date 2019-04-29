#include <fstream>
#include <vector>
#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/stats-module.h"
#include "ns3/flow-monitor-helper.h"
#include "ns3/flow-monitor-module.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE ("CongestionWindow_PacketDrop_Study");

class MyApp : public Application
{
public:
  MyApp();
  virtual ~MyApp();

  static TypeId GetTypeId(void);
  void Setup(Ptr<Socket> socket, Address address, uint32_t packetSize, uint32_t nPackets, DataRate dataRate);

private:
  virtual void StartApplication(void);
  virtual void StopApplication(void);

  void ScheduleTx(void);
  void SendPacket(void);

  Ptr<Socket>     m_socket;
  Address         m_peer;
  uint32_t        m_packetSize;
  uint32_t        m_nPackets;
  DataRate        m_dataRate;
  EventId         m_sendEvent;
  bool            m_running;
  uint32_t        m_packetsSent;
};

// Overriding the member variables of Application clsss
MyApp::MyApp(): m_socket(0),m_peer(), m_packetSize(0), m_nPackets(0), m_dataRate(0), m_sendEvent(), m_running(false), m_packetsSent(0)
{
}

// Socket is deleted after the connection is closed
MyApp::~MyApp()
{
  m_socket = 0;
}

TypeId MyApp::GetTypeId(void)
{
  static TypeId tid = TypeId("MyApp").SetParent<Application>().SetGroupName("Tutorial").AddConstructor<MyApp>();
  return tid;
}

// Setup connection by initializing the member variables
void
MyApp::Setup(Ptr<Socket> socket, Address address, uint32_t packetSize, uint32_t nPackets, DataRate dataRate)
{
  m_socket = socket;
  m_peer = address;
  m_packetSize = packetSize;
  m_nPackets = nPackets;
  m_dataRate = dataRate;
}

// Does the initial bind and connect and starts data flow by calling SendPacket()
void
MyApp::StartApplication(void)
{
  m_running = true;
  m_packetsSent = 0;
  // Decides binding for Ipv4 or Ipv6
  if(InetSocketAddress::IsMatchingType(m_peer))
  {
    m_socket->Bind();
  }
  else
  {
    m_socket->Bind6();
  }
  m_socket->Connect(m_peer);
  SendPacket();
}

// Stops generating packets by cancelling any pending send events then closes the socket
void
MyApp::StopApplication(void)
{
  m_running = false;
  if(m_sendEvent.IsRunning())
  {
    Simulator::Cancel(m_sendEvent);
  }
  if(m_socket)
  {
    m_socket->Close();
  }
}

// Creates a Packet and sends it
void
MyApp::SendPacket(void)
{
  // Creating a Packet
  Ptr<Packet> packet = Create<Packet>(m_packetSize);
  // Sending the packet
  m_socket->Send(packet);

  // If number of packets sent is less than the max num of packets
  // to be sent, schedule the next packet transmission
  if(++m_packetsSent < m_nPackets)
  {
    ScheduleTx();
  }
}

// If connection is active, schedule the transmission of next packet
void
MyApp::ScheduleTx(void)
{
  if(m_running)
  {
    Time tNext(Seconds(m_packetSize * 8 / static_cast<double>(m_dataRate.GetBitRate())));
    m_sendEvent = Simulator::Schedule(tNext, &MyApp::SendPacket, this);
  }
}

// Creating FlowMonitor to get connection stats
Ptr<FlowMonitor> flowMonitor;
FlowMonitorHelper flowHelper;

// Map to store the cumulative number of packets dropped and the time of packet drop
std::vector< std::pair<float,int> > packetDropTime;

// Generate Constant Bit-Rate UDP traffic within the given start_time and stop_time 
void simulateCBR(NodeContainer &nodes, OnOffHelper &onOff, uint64_t start_time, uint64_t stop_time)
{
  // Set data rate for the connection
  onOff.SetAttribute("DataRate", StringValue("300Kbps"));
  // Set start time for the connection
  onOff.SetAttribute("StartTime", TimeValue(MilliSeconds(start_time)));
  // Set stop time for the connection
  onOff.SetAttribute("StopTime", TimeValue(MilliSeconds(stop_time)));
  
  // Simulation of an Application that sends CBR traffic
  ApplicationContainer cbr;
  // Install the application in the sender i.e. Node0
  cbr.Add(onOff.Install(nodes.Get(0)));

  // Start CBR traffic
  cbr.Start(MilliSeconds(start_time));
  cbr.Stop(MilliSeconds(stop_time));
}

static void
CwndChange(Ptr<OutputStreamWrapper> stream, uint32_t oldCwnd, uint32_t newCwnd)
{
  *stream->GetStream() << Simulator::Now().GetSeconds() << "\t" << oldCwnd << "\t" << newCwnd << std::endl;
}

// Obtain the cumulative number of TCP packet drops from flow monitor 
// and store the time and number of packet drops
void tracePacketDrop()
{
  std::map<FlowId, FlowMonitor::FlowStats> stats = flowMonitor->GetFlowStats();
  float currTime = Simulator::Now().GetSeconds();
  int numPackets = 0;
  // Count the TCP packet drops which are in Channel 1 & 2 only i.e. TCP Tx and Rx
  for(int j=1;j<=2;j++)
  {
    if(stats[j].packetsDropped.size()>=5)
    {
      numPackets+=stats[j].packetsDropped[3]+stats[j].packetsDropped[4];
    }
  }
  // packetDropTime stores time vs cumulative number of packets dropped
  packetDropTime.push_back({currTime, numPackets});
  Simulator::Schedule(Seconds(0.01), &tracePacketDrop);
}

int main(int argc, char *argv[])
{
  // Take arguments from cmd
  CommandLine cmd;
  cmd.Parse(argc, argv);
  // Obtain the TCP version as a command line argument
  std::string transport_prot = argv[1];
  std::cout<<transport_prot<<std::endl;

  // Set the TCP version
  if(transport_prot.compare("TcpNewReno") == 0)
  {
    Config::SetDefault("ns3::TcpL4Protocol::SocketType", TypeIdValue(TcpNewReno::GetTypeId()));
  }
  else if(transport_prot.compare("TcpHybla") == 0)
  {
    Config::SetDefault("ns3::TcpL4Protocol::SocketType", TypeIdValue(TcpHybla::GetTypeId()));
  }
  else if(transport_prot.compare("TcpWestwood") == 0)
  {
  // the default protocol type in ns3::TcpWestwood is WESTWOOD
  // for WESTWOODPLUS, add Config::SetDefault("ns3::TcpWestwood::ProtocolType", EnumValue(TcpWestwood::WESTWOODPLUS));
    Config::SetDefault("ns3::TcpL4Protocol::SocketType", TypeIdValue(TcpWestwood::GetTypeId()));
    Config::SetDefault("ns3::TcpWestwood::FilterType", EnumValue(TcpWestwood::TUSTIN));
  }
  else if(transport_prot.compare("TcpScalable") == 0)
  {
    Config::SetDefault("ns3::TcpL4Protocol::SocketType", TypeIdValue(TcpScalable::GetTypeId()));
  }
  else if(transport_prot.compare("TcpVegas") == 0)
  {
    Config::SetDefault("ns3::TcpL4Protocol::SocketType", TypeIdValue(TcpVegas::GetTypeId()));
  }
  else
  {
    NS_LOG_DEBUG("Invalid TCP version");
    exit(1);
  }

  // Create nodes
  NodeContainer nodes;
  nodes.Create(2);

  // Create Link between 2 nodes
  PointToPointHelper pointToPoint;
  // Set up the link speed and delay of the point to point  connection
  pointToPoint.SetDeviceAttribute("DataRate", StringValue("1Mbps"));
  pointToPoint.SetChannelAttribute("Delay", StringValue("10ms"));
  // Set the drop tail queue size as the bandwidth delay product
  pointToPoint.SetQueue("ns3::DropTailQueue", "MaxSize", StringValue("1500B"));

  // Connect link with nodes
  NetDeviceContainer devices;
  devices = pointToPoint.Install(nodes);

  // Install IPv4 related objects to the nodes which sets up the IPv4 routing
  InternetStackHelper stack;
  stack.Install(nodes);

  uint16_t sinkPort = 8080;
  Address sinkAddress;
  Address anyAddress;
  
  std::string probeType;
  std::string tracePath;

  // Initializing packetSize and maxPackets for MyApp object
  uint16_t packetSize = 512;
  uint32_t maxPackets = 100000;

  // Create base IP address and assign IP addresses to the nodes
  Ipv4AddressHelper address;
  address.SetBase("10.1.1.0", "255.255.255.0");
  // Assign the adresses to the nodes
  Ipv4InterfaceContainer interfaces = address.Assign(devices);

  // Initializing the sink address as Node1's IP address
  sinkAddress = InetSocketAddress(interfaces.GetAddress(1), sinkPort);
  anyAddress = InetSocketAddress(Ipv4Address::GetAny(), sinkPort);
  
  // Trace the packets transmitted by Node0
  probeType = "ns3::Ipv4PacketProbe";
  tracePath = "/NodeList/*/$ns3::Ipv4L3Protocol/Tx";

  // Simulate the Application that receive the TCP packets
  PacketSinkHelper packetSinkHelper("ns3::TcpSocketFactory", anyAddress);
  ApplicationContainer sinkApps = packetSinkHelper.Install(nodes.Get(1));
  
  sinkApps.Start(MilliSeconds(0.));
  sinkApps.Stop(MilliSeconds(1800));

  // Create the TCP socket and set the sender as Node0
  Ptr<Socket> ns3TcpSocket = Socket::CreateSocket(nodes.Get(0), TcpSocketFactory::GetTypeId());

  // Create the MyApp object for the FTP connection
  Ptr<MyApp> app = CreateObject<MyApp>();
  app->Setup(ns3TcpSocket, sinkAddress, packetSize, maxPackets, DataRate("1Mbps"));

  // Install the simulated Application to Node0
  nodes.Get(0)->AddApplication(app);
  app->SetStartTime(MilliSeconds(0));
  app->SetStopTime(MilliSeconds(1800));

  // Create CBR Applications
  uint16_t cbrPort = 8000;

  // OnOffHelper is used to simulate Constant-Bit-Rate traffic
  OnOffHelper onOff("ns3::UdpSocketFactory", InetSocketAddress(interfaces.GetAddress(1), cbrPort));
  onOff.SetAttribute("OnTime",  StringValue("ns3::ConstantRandomVariable[Constant=1]"));
  onOff.SetAttribute("OffTime", StringValue("ns3::ConstantRandomVariable[Constant=0]"));

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
  PacketSinkHelper udp_sink("ns3::UdpSocketFactory",InetSocketAddress(Ipv4Address::GetAny(), cbrPort));

  // Simulate an Application to receive the UDP traffic in Node1
  ApplicationContainer cbr_sink = udp_sink.Install(nodes.Get(1));
  cbr_sink.Start(MilliSeconds(0));
  cbr_sink.Stop(MilliSeconds(1800));

  // Store the congestion window data
  AsciiTraceHelper asciiTraceHelper;
  Ptr<OutputStreamWrapper> stream = asciiTraceHelper.CreateFileStream("./Output/"+transport_prot+".cwnd");
  ns3TcpSocket->TraceConnectWithoutContext("CongestionWindow", MakeBoundCallback(&CwndChange, stream));

  // Use FileHelper to write out the packet byte count over time
  FileHelper fileHelper;

  // Configure the file to be written, and the formatting of output data.
  fileHelper.ConfigureFile("./Output/"+transport_prot+"-packet-byte-count", FileAggregator::FORMATTED);

  // Set the labels for this formatted output file.
  fileHelper.Set2dFormat("%.3e\t%.0f");

  // Specify the probe type, trace source path(in configuration namespace), and
  // probe output trace source("OutputBytes") to write.
  fileHelper.WriteProbe(probeType, tracePath, "OutputBytes");

  // Flow monitor
  flowMonitor = flowHelper.InstallAll();

  // Run simulation.
  Simulator::Schedule(Seconds(0.01), &tracePacketDrop);
  Simulator::Stop(MilliSeconds(1800));
  Simulator::Run();

  // Get the stats from Flow Monitor
  std::map<FlowId, FlowMonitor::FlowStats> stats = flowMonitor->GetFlowStats();
  std::cout << std::endl << "Flow monitor output:" << std::endl;
  std::cout << "Tx Packets:   " << stats[1].txPackets << std::endl;
  std::cout << "Tx Bytes:     " << stats[1].txBytes << std::endl;
  std::cout << "Offered Load: " << stats[1].txBytes * 8.0 /(stats[1].timeLastTxPacket.GetSeconds() - stats[1].timeFirstTxPacket.GetSeconds()) / 1000000 << " Mbps" << std::endl;
  std::cout << "Rx Packets:   " << stats[1].rxPackets << std::endl;
  std::cout << "Rx Bytes:     " << stats[1].rxBytes<< std::endl;
  std::cout << "Throughput:   " << stats[1].rxBytes * 8.0 /(stats[1].timeLastRxPacket.GetSeconds() - stats[1].timeFirstRxPacket.GetSeconds()) / 1000000 << " Mbps" << std::endl;
  std::cout << "Mean delay:   " << stats[1].delaySum.GetSeconds() / stats[1].rxPackets << std::endl;
  std::cout << "Mean jitter:  " << stats[1].jitterSum.GetSeconds() /(stats[1].rxPackets - 1) << std::endl;

  // Write the Flow Monitor data to file
  flowMonitor->SerializeToXmlFile("./Output/"+transport_prot + ".flowMonitor", true, true);

  Simulator::Destroy();
  NS_LOG_INFO("Done.");

  // Write the data of the packets dropped into file
  std::ofstream fileOutput;
  fileOutput.open("./Output/"+transport_prot+".drop");
  std::vector<std::pair <float, int> > :: iterator itr;
  for(itr = packetDropTime.begin(); itr != packetDropTime.end(); ++itr)
  {
    fileOutput<<itr->first<<" "<<itr->second<<'\n';
  }
  return 0;
}