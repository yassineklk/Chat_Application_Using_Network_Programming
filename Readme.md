# Distributed Chat Application

This repository is dedicated to a  project focused on building a distributed chat application using C Sockets. The application closely resembles the functionality of Discord, allowing users to participate in conversations within a **general** channel, exchange private messages, and create new **channels**.

## Getting Started

To run the application, follow these steps:

### Server
1. Navigate to the `/Serveur` directory.
2. Execute `make` to compile the server code.
3. Run the server using `make run`. The server is now up and running!

### Client
1. Move to the `/Client` directory.
2. Compile the client code by executing `make`.

Now, you can run as many clients as you require by using the following command:

```shell
./client <server_address> <user_name>
```
For example:

```shell
./client 127.0.0.1 Alex
With the clients set up, users can communicate effortlessly.
```

###Features

#General Channel: Upon connecting with a new user, they are automatically joined to the general channel. Users can view all messages in this channel and send their own messages, visible to everyone.

#Private Messaging: Users can send private messages to each other using the command /m "user_name" [message].

#Join Private Channels: To join a private channel, use the command /join [number]. Please note that currently, only numerical values are supported for channel names.

Message History: The chat application saves and displays the message history for every channel. This historical data is available whenever a user joins a channel. Be aware that all message histories are deleted when the server is shut down.

We hope you enjoy using our distributed chat application! If you have any questions or encounter issues, please feel free to reach out to us.
