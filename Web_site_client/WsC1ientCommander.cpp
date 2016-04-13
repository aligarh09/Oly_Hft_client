#include "WsC1ientCommander.h"

void WsC1ientCommander::queue_get_access()
{ 
  key_t key; 

  FILE* temp = fopen(queue_file_pathname.c_str(), "a+");

  fclose(temp);

  if ((key = ftok(queue_file_pathname.c_str(), 0)) < 0)
  {
    printf("Can\'t generate key\n");
  }

  if((ws_queue_fd = msgget(key, 0666 | IPC_CREAT)) < 0)
  {
    printf("Can\'t get msgid\n");
  }
  else
  {
    printf("Queue access granted\n");
  }
}

void WsC1ientCommander::send_message()
{
  if(msgsnd(ws_queue_fd, (ws_namespace::ws_msg_buf_t*) &connection_actions, sizeof(ws_namespace::ws_msg_buf_t) - sizeof(long), 0) < 0)
  {
    printf("Can\'t send message to queue\n");
    perror("msgsnd");
  }
  else
    printf("Msg successfully sent\n");
}

void WsC1ientCommander::delete_queue()       //TODO::delete file or not
{
   if(msgctl(ws_queue_fd, IPC_RMID, (struct msqid_ds*)NULL) < 0)
   {
      printf("Can't delete queue\n");
      perror("msgctl");
   }
}

void WsC1ientCommander::recieve_from_queue()
{
  ws_namespace::PingResult ping_result;

  int rcv_result = msgrcv(ws_queue_fd, (ws_namespace::PingResult*) &ping_result, sizeof(ping_result) - sizeof(long), ws_namespace::recive_type, IPC_NOWAIT);

  if (rcv_result < 0)
  {
    printf("WsC1ientCommander::recieve_ping_response::error recieving msg\n");
    perror("msgrcv");
  }
  else
  {
    if (ping_result.ready_to_recieve == false)
     {
       connection_opened = 0;
       first_time_called = 1;
     }

    if(ping_result.ready_to_recieve == true)
      connection_opened = 1;
  }
}

void WsC1ientCommander::send_ping_msg()
{
  ws_namespace::PingConnection ping = {1, ws_namespace::recive_type};

  if(msgsnd(ws_queue_fd, (ws_namespace::PingConnection*) &ping, sizeof(ping) - sizeof(long), 0) < 0)
  {
    printf("WsC1ientCommander::send_ping_msg:: Can\'t send message to queue\n");
    perror("msgsnd");
  }
  else
    printf("send_ping_msg:: Msg successfully sent\n");
}

WsC1ientCommander::WsC1ientCommander()
{
  first_time_called = 1;
  prev = {};
  current = {};
  prev_status = {};
  current_status = {};
  connection_actions = {};
  connection_actions.msgtyp = 1;
  ws_queue_fd = 0;
  connection_opened = 0;

  char current_username[ws_namespace::MAX_USERNAME_LENGTH] = {};
  getlogin_r(current_username, ws_namespace::MAX_USERNAME_LENGTH);

  queue_file_pathname = std::string("/home/") + std::string(current_username) + ws_namespace::queue_filename;

  queue_get_access();
}

void WsC1ientCommander::process_current_status(ParseOlymptradeJSON& current_parsed)
{
  prev = current;
  current = current_parsed;
  
  if(first_time_called)
  {
    first_time_called = 0;

    send_ping_msg();

    for(int i = 0; i < ws_namespace::ASSETS_AMOUNT; i++)
    {
      current_status = current.get_asset_status(i);

      if(!current_status.locked)
      {
        connection_actions.action = !current_status.locked;
        connection_actions.asset = i;

        printf("connection_actions.asset_name::%s\n", ws_namespace::assets_names[i].c_str());
        printf("current_status.locked::%d\n", current_status.locked);

        send_message();
      }
    }
  }
  else
  {
    recieve_from_queue();

    if(connection_opened)
    {
      for(int i = 0; i < ws_namespace::ASSETS_AMOUNT; i++)
      {
        prev_status = prev.get_asset_status(i);
        current_status = current.get_asset_status(i);

        if(prev_status.locked != current_status.locked)
        {
          connection_actions.action = !current_status.locked;
          connection_actions.asset = i;
          
          printf("connection_actions.asset_name::%s\n", ws_namespace::assets_names[i].c_str());
          printf("current_status.locked::%d\n", current_status.locked);

          send_message();
        }
      }
    }
    else
    {
      printf("WsC1ientCommander:: Connection is not currently opened; Can't update\n");
    }
  }
}