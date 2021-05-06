/*
 * Copyright (C) 2012-2014 Open Source Robotics Foundation
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
 */
#include <sdf/sdf.hh>

#include <boost/bind.hpp>

#include "gazebo/gazebo.hh"
#include "gazebo/common/Plugin.hh"
#include "gazebo/msgs/msgs.hh"
#include "gazebo/physics/physics.hh"
#include "gazebo/transport/transport.hh"
#include "gazebo/physics/World.hh"

#include "packet.h"
#define ROBOTICS_COSIM_BUFSIZE 1024
#define GAZEBO_COSIM_PORT 8001

/// \example examples/plugins/world_edit.cc
/// This example creates a WorldPlugin, initializes the Transport system by
/// creating a new Node, and publishes messages to alter gravity.
namespace gazebo
{
    class WorldEdit : public WorldPlugin
    {
        public: void Load(physics::WorldPtr _parent, sdf::ElementPtr _sdf)
                {
                    // Store the WorldPtr
                    this->world = _parent;

                    // Create a new transport node
                    this->node.reset(new transport::Node());

                    // Initialize the node with the world name
                    this->node->Init(_parent->GetName());

                    std::cout << _parent->GetName() << std::endl;

                    // Create a publisher
                    this->pub = this->node->Advertise<msgs::WorldControl>("~/world_control");

                    // Listen to the update event.  Event is broadcast every simulation 
                    // iteration.
                    this->updateConnection = event::Events::ConnectWorldUpdateEnd(
                            boost::bind(&WorldEdit::OnUpdate, this));


                    // Configure the initial message to the system
                    msgs::WorldControl worldControlMsg;

                    // Set the world to paused
                    worldControlMsg.set_pause(0);

                    // Set the step flag to true
                    worldControlMsg.set_step(1);

                    // Publish the initial message. 
                    this->pub->Publish(worldControlMsg);
                    std::cout << "Publishing Load." << std::endl;

                    // COSIM-CODE
                    // Adapted from: https://www.cs.cmu.edu/afs/cs/academic/class/15213-f99/www/class26/tcpclient.c
                    this->hostname = "localhost";
                    this->portno = GAZEBO_COSIM_PORT;

                    /* socket: create the socket */
                    this->sockfd = socket(AF_INET, SOCK_STREAM | SOCK_NONBLOCK, 0);
                    if (this->sockfd < 0) {
                        perror("ERROR opening socket");
                        exit(0);
                    }
                    printf("Created socket!\n");

                    /* gethostbyname: get the server's DNS entry */
                    this->server = gethostbyname(hostname);
                    if (this->server == NULL) {
                        fprintf(stderr,"ERROR, no such host as %s\n", hostname);
                        exit(0);
                    }
                    printf("Got server's DNS entry!\n");

                    /* build the server's Internet address */
                    bzero((char *) &this->serveraddr, sizeof(this->serveraddr));
                    this->serveraddr.sin_family = AF_INET;
                    bcopy((char *)this->server->h_addr, 
                            (char *)&this->serveraddr.sin_addr.s_addr, this->server->h_length);
                    this->serveraddr.sin_port = htons(this->portno);
                    printf("Got server's Internet address!\n");

                    /* connect: create a connection with the server */
                    while(connect(this->sockfd, (const sockaddr *) &this->serveraddr, sizeof(this->serveraddr)) < 0);
                }

                // Called by the world update start event.
        public: void OnUpdate() 
                {
                    // Create WorldControl message to increment simulation tick
                    msgs::WorldControl msg;
                    msg.set_step(1);

                    //this->n = net_read(this->sockfd, this->buf, ROBOTICS_COSIM_BUFSIZE);
                    for(int i=0; i < 2; i++) 
                    {
                        bzero(this->buf, ROBOTICS_COSIM_BUFSIZE);
                        do 
                        {
                            this->n = read(this->sockfd, this->buf, 1); 
                        } while (this->n < 1);
                        printf("Recieved Packet, length %d. Byte 0: %x\n", this->n, buf[0] & 0xFF);
                        switch(buf[0] & 0xFF)
                        {
                            // Cycle the simulation by a tick
                            case CS_GRANT_TOKEN: 
                                printf("Granting token!\n");
                                break;
                            case CS_REQ_CYCLES:
                                // Sent message to server with current tick
                                printf("Recieved request for tick count!\n");
                                bzero(this->buf, ROBOTICS_COSIM_BUFSIZE);
                                sprintf(buf, "%d", world->GetIterations());
                                write(this->sockfd, this->buf, strlen(this->buf));
                                break;
                        }
                    }
                    this->pub->Publish(msg);
                    std::cout << "Publishing OnUpdate." << std::endl;

                }

                // Pointer to the world_controller
        private: transport::NodePtr node;
        private: transport::PublisherPtr pub;

                 // Pointer to the update event connection
        private: event::ConnectionPtr updateConnection;

        private:
                 physics::WorldPtr world;

                 // TCP server stuff
        private:
                 int sockfd, portno, n;
                 struct sockaddr_in serveraddr;
                 struct hostent *server;
                 char *hostname;
                 char buf[ROBOTICS_COSIM_BUFSIZE];
    };

    // Register this plugin with the simulator
    GZ_REGISTER_WORLD_PLUGIN(WorldEdit)
}
