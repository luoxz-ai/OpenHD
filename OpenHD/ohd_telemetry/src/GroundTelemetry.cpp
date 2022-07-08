//
// Created by consti10 on 13.04.22.
//

#include "GroundTelemetry.h"

#include "mav_param/XMavlinkParamProvider.h"

#include <iostream>

#include "mav_helper.h"

GroundTelemetry::GroundTelemetry(): MavlinkSystem(OHD_SYS_ID_GROUND) {
  /*tcpGroundCLient=std::make_unique<TCPEndpoint>(OHD_GROUND_CLIENT_TCP_PORT);
  tcpGroundCLient->registerCallback([this](MavlinkMessage& msg){
          onMessageGroundStationClients(msg);
  });*/
  /*udpGroundClient =std::make_unique<UDPEndpoint>("GroundStationUDP",
                                                                                                 OHD_GROUND_CLIENT_UDP_PORT_OUT, OHD_GROUND_CLIENT_UDP_PORT_IN,
                                                                                                 "127.0.0.1","127.0.0.1",true);//127.0.0.1
  udpGroundClient->registerCallback([this](MavlinkMessage &msg) {
        onMessageGroundStationClients(msg);
  });*/
  udpGroundClient =
      std::make_unique<UDPEndpoint2>("GroundStationUDP",OHD_GROUND_CLIENT_UDP_PORT_OUT, OHD_GROUND_CLIENT_UDP_PORT_IN,
                                     "127.0.0.1","127.0.0.1");
  udpGroundClient->registerCallback([this](MavlinkMessage &msg) {
    onMessageGroundStationClients(msg);
  });
  // any message coming in via wifibroadcast is a message from the air pi
  udpWifibroadcastEndpoint = UDPEndpoint::createEndpointForOHDWifibroadcast(false);
  udpWifibroadcastEndpoint->registerCallback([this](MavlinkMessage &msg) {
    onMessageAirPi(msg);
  });
  _ohd_main_component=std::make_shared<OHDMainComponent>(*this,false);
  components.push_back(_ohd_main_component);
  std::cout << "Created GroundTelemetry\n";
}

void GroundTelemetry::onMessageAirPi(MavlinkMessage &message) {
  //debugMavlinkMessage(message.m,"GroundTelemetry::onMessageAirPi");
  const mavlink_message_t &m = message.m;
  // we do not need to forward heartbeat messages coming from the air telemetry service
  /*if (m.msgid == MAVLINK_MSG_ID_HEARTBEAT && m.sysid == OHD_SYS_ID_AIR) {
	// heartbeat coming from the air service
	return;
  }*/
  // for now, forward everything
  sendMessageGroundStationClients(message);
}

void GroundTelemetry::onMessageGroundStationClients(MavlinkMessage &message) {
  //debugMavlinkMessage(message.m, "GroundTelemetry::onMessageGroundStationClients");
  const auto &msg = message.m;
  // for now, forward everything
  sendMessageAirPi(message);
  // OpenHD components running on the ground station don't need to talk to the air unit.
  for(auto& component:components){
    const auto responses=component->process_mavlink_message(message);
    for(const auto& response:responses){
      // for now, send to the ground station clients only
      sendMessageGroundStationClients(response);
    }
  }
}

void GroundTelemetry::sendMessageGroundStationClients(const MavlinkMessage &message) {
  //debugMavlinkMessage(message.m, "GroundTelemetry::sendMessageGroundStationClients");
  // forward via TCP or UDP
  if (tcpGroundCLient) {
	tcpGroundCLient->sendMessage(message);
  }
  if (udpGroundClient) {
	udpGroundClient->sendMessage(message);
  }
}

void GroundTelemetry::sendMessageAirPi(const MavlinkMessage &message) {
  // transmit via wifibroadcast
  if (udpWifibroadcastEndpoint) {
	udpWifibroadcastEndpoint->sendMessage(message);
  }
}

[[noreturn]] void GroundTelemetry::loopInfinite(const bool enableExtendedLogging) {
  while (true) {
	std::cout << "GroundTelemetry::loopInfinite()\n";
	// for debugging, check if any of the endpoints is not alive
	if (enableExtendedLogging && udpWifibroadcastEndpoint) {
	  std::cout<<udpWifibroadcastEndpoint->createInfo();
	}
	if (enableExtendedLogging && udpGroundClient) {
	  std::cout<<udpGroundClient->createInfo();
	}
	// send messages to the ground station in regular intervals, includes heartbeat.
	// everything else is handled by the callbacks and their threads
	/*const auto ohdTelemetryMessages = _ohd_main_component->generate_mavlink_messages();
	for (const auto &msg: ohdTelemetryMessages) {
	  sendMessageGroundStationClients(msg);
	}*/
        for(auto& component:components){
          const auto messages=component->generate_mavlink_messages();
          for(const auto& msg:messages){
            sendMessageGroundStationClients(msg);
          }
        }
	std::this_thread::sleep_for(std::chrono::seconds(3));
  }
}

std::string GroundTelemetry::createDebug() const {
  std::stringstream ss;
  //ss<<"GT:\n";
  if (udpWifibroadcastEndpoint) {
	std::cout<<udpWifibroadcastEndpoint->createInfo();
  }
  if (udpGroundClient) {
	std::cout<<udpGroundClient->createInfo();
  }
  return ss.str();
}


void GroundTelemetry::add_settings_component(
    int comp_id, std::shared_ptr<openhd::XSettingsComponent> glue) {
  auto param_server=std::make_shared<XMavlinkParamProvider>(*this,comp_id,std::move(glue));
  components.push_back(param_server);
  std::cout<<"Added parameter component\n";
}
