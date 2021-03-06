#include "AssetsStatusRecieve.h"

AssetsStatusRecieve::AssetsStatusRecieve()
{
  asset = -1;
  ping_recieved = 0;
  asset_availability = 0;
  asset_status_queue_fd = 0;

  char current_username[MAX_USERNAME_LENGTH] = {};
  getlogin_r(current_username, MAX_USERNAME_LENGTH);

  status_queue_file_pathname =  std::string("/home/") + std::string(current_username) + std::string("/") + status_queue_filename;

  std::string assets_filepath = std::string("/home/") + std::string(current_username) + asset_names_filename;
  names.load_asset_names(&assets_filepath);
  assets_amount = names.get_assets_amount();
}

void AssetsStatusRecieve::open_status_queue()
{ 
  key_t key;

  FILE* temp = fopen(status_queue_file_pathname.c_str(), "a+");

  fclose(temp);

  if ((key = ftok(status_queue_file_pathname.c_str(), 0)) < 0)
  {
    printf("Can\'t generate key\n");
    return;
  }

  if((asset_status_queue_fd = msgget(key, 0666 | IPC_CREAT)) < 0)
  {
    printf("Can\'t get msgid\n");
    return;
  }
}

void AssetsStatusRecieve::close_status_queue()
{
  asset_status_queue_fd = 0;
}

void AssetsStatusRecieve::ping_connection()
{
  ping_structs::PingConnection recieved_ping = {};

  int rcv_result = msgrcv(asset_status_queue_fd, (ping_structs::PingConnection*) &recieved_ping, sizeof(recieved_ping) - sizeof(long), WINPERC_COMMANDER_STATUS_CHANGED_MTYPE + asset, IPC_NOWAIT);

  if (rcv_result < 0)
  {
    printf("AssetsStatusRecieve::ping connection::error recieving ping msg\n");
    perror("msgrcv");
  }
  else
  {
    ping_recieved = 1;
    printf("AssetsStatusRecieve::ping connection::ping msg successfully recieved\n");    

    ping_structs::PingResult ping = {};
    ping.mtype = recieved_ping.ping_data;
    ping.ready_to_recieve  = 1;

    if(msgsnd(asset_status_queue_fd, (ping_structs::PingResult*) &ping, sizeof(ping) - sizeof(long), 0) < 0)
    {
      printf("AssetsStatusRecieve::Can\'t respond to ping message\n");
    }  
  }
} 

void AssetsStatusRecieve::update_availability()
{
  status_structs::AssetAvailable availability;

  if(asset == -1)
  {
    printf("Set please asset to serve\n");
    return;
  }

  if(!ping_recieved)
    ping_connection();

  if(asset_status_queue_fd && ping_recieved)
  {
    availability = {};

    int rcv_result = msgrcv(asset_status_queue_fd, (status_structs::AssetAvailable*) &availability, sizeof(availability) - sizeof(long), WINPERC_COMMANDER_STATUS_CHANGED_MTYPE + asset, IPC_NOWAIT);

    if (rcv_result < 0)
    {
      printf("AssetsStatusRecieve:: Asset %s:: update_availability::error recieving msg\n", names.get_asset_name(asset) -> c_str());
      perror("msgrcv");
    }
    else
    {
      printf("AssetsStatusRecieve:: Asset %s:: update_availability::update msg successfully recieved\n", names.get_asset_name(asset) -> c_str());
      asset_availability = availability.available;
    }
  }
  else  
  {
    if(!asset_status_queue_fd)
      printf("AssetsStatusRecieve:: queue connection closed; Can't update availability\n", names.get_asset_name(asset) -> c_str());
  }
}

void AssetsStatusRecieve::set_serviced_asset(int asset_set_num)
{
  if(asset == -1 && asset_set_num < assets_amount)
  {
    asset = asset_set_num;
    open_status_queue();
    ping_connection();
  }
  else
  {
    printf("AssetsStatusRecieve:: Current asset is already set or error asset num; Create new variable to serve new asset or correct your input\n");
  }
}

int AssetsStatusRecieve::get_availability()
{
  if(asset == -1)
  {
    printf("Set please asset to serve\n");
    return -1;
  }

  if(ping_recieved)
    return asset_availability;
  else
  {
    printf("Ping msg does not recieved! No data to return\n");
    return -1;
  }
}