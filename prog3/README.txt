To create a chat room:

1) Start prog3_server. Args: participant_port observer_port

2) Start prog3_participant. Args: server_address server's_participant_port.

3) Start prog3_observer in a different terminal. Args: server_address server's_participant_port.

Up to 255 participants and 255 observers may connect to the chat room at any one time. Private messages begin with "@" followed by the username of another participant. Private messages only appear on the screen of the recipiant.
