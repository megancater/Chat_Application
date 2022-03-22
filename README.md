Chat_Application is message queue that uses POSIX threads and network sockets to interacts with a pub/sub system.

To make:
$ make all
$ make application

To run, first start the message queue server at port xxxx (ex: 9620):
$ ./bin/mq_server.py --port=9620

To start the chat application:
$ ./application [username] [host] [port]

For help:
$ ./application --help
