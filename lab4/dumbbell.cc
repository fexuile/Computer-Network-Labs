#include "ns3/applications-module.h"
#include "ns3/core-module.h"
#include "ns3/internet-module.h"
#include "ns3/network-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/traffic-control-module.h"

#include <fstream>
#include <iostream>
#include <string>
#include <sys/stat.h>

using namespace ns3;

// - Network topology
//   - The dumbbell topology consists of 
//     - 4 servers (S0, S1, R0, R1)
//     - 2 routers (T0, T1) 
//   - The topology is as follows:
//
//                    S0                         R0
//     10 Mbps, 1 ms   |      1 Mbps, 10 ms       |   10 Mbps, 1 ms
//                    T0 ----------------------- T1
//     10 Mbps, 1 ms   |                          |   10 Mbps, 1 ms
//                    S1                         R1
//
// - Two TCP flows:
//   - TCP flow 0 from S0 to R0 using BulkSendApplication.
//   - TCP flow 1 from S1 to R1 using BulkSendApplication.

const uint32_t N1 = 2;    // Number of nodes in left side
const uint32_t N2 = 2;    // Number of nodes in right side
std::string dir = "lv1-results/";
uint32_t segmentSize = 1448;    // Segment size
Time startTime = Seconds(10.0);    // Start time for the simulation
Time stopTime = Seconds(60.0);    // Stop time for the simulation
int64_t flow0, flow1;

// Function to trace change in cwnd at n0
static void
CwndChange(std::string context, uint32_t oldCwnd, uint32_t newCwnd)
{
    std::ofstream fPlotQueue(dir + "cwnd/n" + context[10] + ".dat", std::ios::out | std::ios::app);
    fPlotQueue << Simulator::Now().GetMicroSeconds() << " " << oldCwnd / segmentSize << " " << newCwnd / segmentSize << std::endl;
    fPlotQueue.close();
}

// Trace Function for cwnd
void
TraceCwnd(uint32_t node, uint32_t cwndWindow, Callback<void, std::string, uint32_t, uint32_t> CwndTrace)
{
    Config::Connect("/NodeList/" + std::to_string(node) +
                                      "/$ns3::TcpL4Protocol/SocketList/" +
                                      std::to_string(cwndWindow) + "/CongestionWindow",
                                  CwndTrace);
}

static void FctChange(int flowId, const Ptr<const Packet> packet, const TcpHeader& header, const Ptr<const TcpSocketBase> socket)
{
    std::ofstream fPlotQueue(dir + "fct/fct.dat", std::ios::out);
    if(flowId == 0) flow0 = Simulator::Now().GetMicroSeconds();
    else flow1 = Simulator::Now().GetMicroSeconds();
    fPlotQueue << flow0 - startTime.GetMicroSeconds() << std::endl << flow1 - startTime.GetMicroSeconds() << std::endl;
    fPlotQueue.close();
}

void TraceFct(ApplicationContainer sourceApps, int flowId) {
    Ptr<BulkSendApplication> bulk_app = sourceApps.Get(0)->GetObject<BulkSendApplication>();
    Ptr<Socket> socket = bulk_app->GetSocket();
    socket->TraceConnectWithoutContext("Rx", MakeBoundCallback(&FctChange, flowId));
}

// Function to install BulkSend application
void
InstallBulkSend(Ptr<Node> node,
                Ipv4Address address,
                uint16_t port,
                std::string socketFactory,
                uint32_t flowSize,
                uint32_t nodeId,
                uint32_t cwndWindow, 
                uint32_t flowId, 
                Callback<void, std::string, uint32_t, uint32_t> CwndTrace)
{
    BulkSendHelper source(socketFactory, InetSocketAddress(address, port));
    source.SetAttribute("MaxBytes", UintegerValue(flowSize));    // "0" means there is no limit. This line should be changed in Exercise 1.2
    ApplicationContainer sourceApps = source.Install(node);
    sourceApps.Start(startTime);
    
    Simulator::Schedule(startTime + TimeStep(1), &TraceCwnd, nodeId, cwndWindow, CwndTrace);
    Simulator::Schedule(startTime + TimeStep(2), &TraceFct, sourceApps, flowId);
    sourceApps.Stop(stopTime);
}

// Function to install sink application
void
InstallPacketSink(Ptr<Node> node, uint16_t port, std::string socketFactory)
{
    PacketSinkHelper sink(socketFactory, InetSocketAddress(Ipv4Address::GetAny(), port));
    ApplicationContainer sinkApps = sink.Install(node);
    sinkApps.Start(startTime);
    sinkApps.Stop(stopTime);
}

int
main(int argc, char* argv[])
{
    uint32_t flowSize0, flowSize1; 

    CommandLine cmd;
    cmd.AddValue("flowSize0", "Size of flow 0", flowSize0);
    cmd.AddValue("flowSize1", "Size of flow 1", flowSize1);
    cmd.Parse (argc, argv);

    std::string socketFactory = "ns3::TcpSocketFactory";    // Socket factory to use
    std::string tcpTypeId = "ns3::TcpLinuxReno";    // TCP variant to use
    std::string qdiscTypeId = "ns3::FifoQueueDisc";    // Queue disc for gateway
    bool isSack = true;    // Flag to enable/disable sack in TCP
    uint32_t delAckCount = 1;    // Delayed ack count
    std::string recovery = "ns3::TcpClassicRecovery";    // Recovery algorithm type to use

    // Check if the qdiscTypeId and tcpTypeId are valid
    TypeId qdTid;
    NS_ABORT_MSG_UNLESS(TypeId::LookupByNameFailSafe(qdiscTypeId, &qdTid),
                        "TypeId " << qdiscTypeId << " not found");
    TypeId tcpTid;
    NS_ABORT_MSG_UNLESS(TypeId::LookupByNameFailSafe(tcpTypeId, &tcpTid),
                        "TypeId " << tcpTypeId << " not found");

    // Set recovery algorithm and TCP variant
    Config::SetDefault("ns3::TcpL4Protocol::RecoveryType",
                       TypeIdValue(TypeId::LookupByName(recovery)));
    Config::SetDefault("ns3::TcpL4Protocol::SocketType",
                       TypeIdValue(TypeId::LookupByName(tcpTypeId)));

    // Create directories to store dat files
    struct stat buffer;
    int retVal [[maybe_unused]];
    if ((stat(dir.c_str(), &buffer)) == 0)
    {
        std::string dirToRemove = "rm -rf " + dir;
        retVal = system(dirToRemove.c_str());
        NS_ASSERT_MSG(retVal == 0, "Error in return value");
    }
    std::string dirToSave = "mkdir -p " + dir;
    retVal = system(dirToSave.c_str());
    NS_ASSERT_MSG(retVal == 0, "Error in return value");
    retVal = system((dirToSave + "/pcap/").c_str());
    NS_ASSERT_MSG(retVal == 0, "Error in return value");
    retVal = system((dirToSave + "/cwnd/").c_str());
    NS_ASSERT_MSG(retVal == 0, "Error in return value");
    retVal = system((dirToSave + "/fct/").c_str());
    NS_ASSERT_MSG(retVal == 0, "Error in return value");


    // Create nodes
    NodeContainer leftNodes;
    NodeContainer rightNodes;
    NodeContainer routers;
    routers.Create(2);
    leftNodes.Create(N1);
    rightNodes.Create(N2);

    // Create the point-to-point link helpers and connect two router nodes
    PointToPointHelper pointToPointRouter;
    pointToPointRouter.SetDeviceAttribute("DataRate", StringValue("1Mbps"));
    pointToPointRouter.SetChannelAttribute("Delay", StringValue("10ms"));

    NetDeviceContainer routerToRouter = pointToPointRouter.Install(routers.Get(0), routers.Get(1));

    pointToPointRouter.EnablePcap(dir + "/pcap/lv1", routerToRouter.Get(0), true);

    // Create the point-to-point link helpers and connect leaf nodes to router
    PointToPointHelper pointToPointLeaf;
    pointToPointLeaf.SetDeviceAttribute("DataRate", StringValue("10Mbps"));
    pointToPointLeaf.SetChannelAttribute("Delay", StringValue("1ms"));

    std::vector<NetDeviceContainer> leftToRouter;
    std::vector<NetDeviceContainer> routerToRight;
    for (uint32_t i = 0; i < N1; i++)
    {
        NetDeviceContainer leftToRouterOne = pointToPointLeaf.Install(leftNodes.Get(i), routers.Get(0));
        leftToRouter.push_back(leftToRouterOne);
        pointToPointLeaf.EnablePcap(dir + "/pcap/lv1", leftToRouterOne.Get(1), true);
    }
    for (uint32_t i = 0; i < N2; i++)
    {
        routerToRight.push_back(pointToPointLeaf.Install(routers.Get(1), rightNodes.Get(i)));
    }

    // Install internet stack on all the nodes
    InternetStackHelper internetStack;

    internetStack.Install(leftNodes);
    internetStack.Install(rightNodes);
    internetStack.Install(routers);

    // Assign IP addresses to all the network devices
    Ipv4AddressHelper ipAddresses("10.0.0.0", "255.255.255.0");

    Ipv4InterfaceContainer routersIpAddress = ipAddresses.Assign(routerToRouter);
    ipAddresses.NewNetwork();

    std::vector<Ipv4InterfaceContainer> leftToRouterIPAddress;
    for (uint32_t i = 0; i < N1; i++)
    {
        leftToRouterIPAddress.push_back(ipAddresses.Assign(leftToRouter[i]));
        ipAddresses.NewNetwork();
    }

    std::vector<Ipv4InterfaceContainer> routerToRightIPAddress;
    for (uint32_t i = 0; i < N2; i++)
    {
        routerToRightIPAddress.push_back(ipAddresses.Assign(routerToRight[i]));
        ipAddresses.NewNetwork();
    }

    Ipv4GlobalRoutingHelper::PopulateRoutingTables();

    // Set default sender and receiver buffer size as 1MB
    Config::SetDefault("ns3::TcpSocket::SndBufSize", UintegerValue(1 << 20));
    Config::SetDefault("ns3::TcpSocket::RcvBufSize", UintegerValue(1 << 20));

    // Set default initial congestion window as 10 segments
    Config::SetDefault("ns3::TcpSocket::InitialCwnd", UintegerValue(10));

    // Set default delayed ack count to a specified value
    Config::SetDefault("ns3::TcpSocket::DelAckCount", UintegerValue(delAckCount));

    // Set default segment size of TCP packet to a specified value
    Config::SetDefault("ns3::TcpSocket::SegmentSize", UintegerValue(segmentSize));

    // Enable/Disable SACK in TCP
    Config::SetDefault("ns3::TcpSocketBase::Sack", BooleanValue(isSack));

    // Set default parameters for queue discipline
    Config::SetDefault(qdiscTypeId + "::MaxSize", QueueSizeValue(QueueSize("100p")));

    // Install queue discipline on router
    TrafficControlHelper tch;
    tch.SetRootQueueDisc(qdiscTypeId);
    QueueDiscContainer qd;
    tch.Uninstall(routers.Get(0)->GetDevice(0));
    qd.Add(tch.Install(routers.Get(0)->GetDevice(0)).Get(0));

    // Enable BQL
    tch.SetQueueLimits("ns3::DynamicQueueLimits");

    // Install packet sink at receiver side
    for (uint32_t i = 0; i < N2; i++)
    {
        uint16_t port = 50000 + i;
        InstallPacketSink(rightNodes.Get(i), port, "ns3::TcpSocketFactory");
    }

    for (uint32_t i = 0; i < N1; i++)
    {
        uint16_t port = 50000 + i;
        InstallBulkSend(leftNodes.Get(i),
                        routerToRightIPAddress[i].GetAddress(1),
                        port,
                        socketFactory,
                        i ==0 ? flowSize0 : flowSize1,
                        leftNodes.Get(i)->GetId(),
                        0,
                        i,
                        MakeCallback(&CwndChange));
    }

    // Set the stop time of the simulation
    Simulator::Stop(stopTime);

    // Start the simulation
    Simulator::Run();

    // Cleanup and close the simulation
    Simulator::Destroy();

    return 0;
}
