# IP_Project

#Time Spent

Brandon Chao (bc105) - 35 hours

Peter Yom (pky3) - 35 hours

#Implementation

--Data Structures--
We create structs for IP packet, RIP info, interface entry, and route table entry. Other
variables we maintain are an array of interface entries and an array of route table
entries on each node.

Because of our data structures, printing the interfaces and routes was just a matter of
iterating through the two arrays.

Our IP Packet contains all of the IP header info as well as a payload. This payload is
either a RIP struct, or a message, depending on the protocol number. The interface array
and the route table array are used to assist in forwarding and routing.

--Link Layer--
We adapted our code from assignment one to complete the link layer. This is implemented
by our two functions SERVER and CLIENT. We changed the TCP to UDP and by changing some
constants and taking out the parts only relevant to TCP.

We create the packets using our CREATEIPPACKET and CREATERIPPACKET functions to send
over UDP.

--Serialization and Deserialization--
We serialize by converting our structs into one long char array. We then are able to send
this through the socket to be received on the server. Serialization was achieved by 
using memcpy to copy each value in the struct onto the char array and maintaining an offset
as we go. Deserialization was achieved by doing the opposite - using memcpy to copy each
value from the char array back into a struct that we were able to recreate.

This is handled in our SERIALIZE and DESERIALIZE functions.

--Routing--
Routing is handled in multiple functions. When the node first begins, it populates its 
routing table with the input data and also sends requests out on every interface to continue
populating the table with the data from its neighboring nodes using the SEND_INITIAL_REQUESTS
function. Then, triggered updates are run every 5 seconds on a separate thread which send out
a reponse to each interface that isn't currently down in the EVICT_ENTRIES function. Along 
with this, we have route timeouts after 12 seconds of no updates. This is achieved by 
keeping a last_updated variable in the route entries and constantly comparing those with 
the current time.

Because our nodes constantly send updates out using our SEND_UPDATES function, the routes
are able to deal with links going up and down since the network would eventually update
around the changes. 

We deal with the count to infinity problem by implementing split horizon when we send out
update packets. If the node is receiving a path that includes itself, then the cost of that
path is set to the max 16 thus making it such a node will not continually update on itself.

--Forwarding--
Our forwarding works based off of our link layer and also using our routing and interface
tables. When we receive a packet on the server, we check whether its a message or a RIP
packet based on the protocol id in the IP header. If it is a message and its destination
IP is one of this node's interfaces, then print the message. If it is a message and its
destination is not on this node, then consult the routing table to forward it to its
next hop. If it is a RIP response, then we will loop through and update our routing
tables as needed to find the minimum cost next hop for each route.

#Thoughts

We learned a lot about C programming and network architecture on this project. However,
we felt it was way too tedious with a lot of required boilerplate coding and code
that was not relevant to the class. For example, we spent many hours working out the 
serialization and deserialization methods to send our packets over the socket. We felt
like methods like these should have been provided. Also, the provided assignment
document was very vague, as were the answers posted on Piazza a lot of the time. This 
made it difficult to figure out what was expected of us in this project.

#Resources

We consulted TA Boyang Xu on this project on some portions. For the majority of the
project we worked by ourselves though.