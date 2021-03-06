#include "OlyClientDealService.h"

//-----------------------------------------------------------------------------------------------
//-----------------------------------------------------------------------------------------------

void OlyClientDealService::get_ping_msg()
{
  ping_structs::PingConnection ping = {};

  int rcv_result = msgrcv(asset_deals_queue_fd, (ping_structs::PingConnection*) &ping, sizeof(ping) - sizeof(long), DEAL_SERVICE_PING_MTYPE, IPC_NOWAIT);

  if (rcv_result < 0)
  {
    printf("OlyClientDealService::get_ping_answer::error recieving ping msg\n");
    perror("msgrcv");
  }
  else
  {
    ping_success = 1;
    printf("OlyClientDealService::get_ping_answer::ping msg successfully recieved\n");
    response_ping_msg();
  }
} 

void OlyClientDealService::response_ping_msg()
{
  ping_structs::PingResult ping_result = {DEAL_SERVICE_RESPONCE_PING_MTYPE, 1};

  if(msgsnd(asset_deals_queue_fd, (ping_structs::PingResult*) &ping_result, sizeof(ping_structs::PingResult) - sizeof(long), 0) < 0)
  {
    printf("OlyClientDealService::Can\'t send ping_result message to queue\n");
    perror("msgsnd");
  }
  else
    printf("OlyClientDealService::Ping response msg successfully sent\n");
}

//-----------------------------------------------------------------------------------------------
//-----------------------------------------------------------------------------------------------


void OlyClientDealService::open_deals_queue()
{ 
  key_t key;

  FILE* temp = fopen(deals_queue_file_pathname.c_str(), "a+");

  fclose(temp);

  if ((key = ftok(deals_queue_file_pathname.c_str(), 0)) < 0)
  {
    printf("OlyClientDealService::open_deals_queue::Can\'t generate key\n");
    return;
  }

  if((asset_deals_queue_fd = msgget(key, 0666 | IPC_CREAT)) < 0)
  {
    printf("OlyClientDealService::open_deals_queue::Can\'t get msgid\n");
    return;
  }
}

void OlyClientDealService::close_deals_queue()
{
  asset_deals_queue_fd = 0;
}

//-----------------------------------------------------------------------------------------------
//-----------------------------------------------------------------------------------------------


std::vector<deal_structs::NewDeal> OlyClientDealService::get_new_bets()
{
  std::vector<deal_structs::NewDeal> bets_to_return;

  if(!free_bet_array_positions_amount)
  {
    printf("OlyClientDealService::get_new_bets::No free_bet_array_positions\n");
    return bets_to_return;
  }

  if(!ping_success)
    get_ping_msg();

  if(!ping_success)
  {
    printf("OlyClientDealService::get_new_bets::No ping_success\n");
    return bets_to_return;
  }

  int current_free_positions_amount = free_bet_array_positions_amount;
  std::vector<int> free_positions;
  int places_used = 0;

  for(int i = 0; i < MAX_DEAL_AMOUNT; i++)
  {
    if(!bets_array[i].bet_id)
      free_positions.push_back(i);
  }

  for(int i = 0; i < current_free_positions_amount; i++)
  {
    int rcv_result = msgrcv(asset_deals_queue_fd, (deal_structs::NewDeal*) &bets_array[free_positions[i]], sizeof(deal_structs::NewDeal) - sizeof(long), NEW_DEAL_MTYPE, IPC_NOWAIT);

    if (rcv_result < 0)
    {
      printf("OlyClientDealService::get_new_bets::error recieving msg\n");
      perror("msgrcv");
      bets_array[free_positions[i]] = {};
      break;
    }
    else
    {
      printf("OlyClientDealService::get_new_bets::recieved new msg\n");
      places_used++;
      free_bet_array_positions_amount--;
    }
  }

  if(!places_used)
    return bets_to_return;

  for(int i = 0; i < places_used; i++)
  {
    bets_to_return.push_back(bets_array[free_positions[i]]);
  }

  return bets_to_return;
}

void OlyClientDealService::set_deals_status_json_handler(ParseOlymptradeJSON* deals_status_handler)
{
  assert(deals_status_handler);
  parsed_deals_status = deals_status_handler;
}

//-----------------------------------------------------------------------------------------------
//-----------------------------------------------------------------------------------------------

void OlyClientDealService::send_deal_status(deal_structs::DealStatus* bet_status)
{
  assert(bet_status);

  bet_status -> mtype = DEAL_STATUS_MTYPE;

  if(msgsnd(asset_deals_queue_fd, (deal_structs::DealStatus*) bet_status, sizeof(deal_structs::DealStatus) - sizeof(long), 0) < 0)
  {
    printf("OlyClientDealService::Can\'t send %d deal result message to queue\n", bet_status -> bet_id);
    perror("msgsnd");
  }
  else
    printf("OlyClientDealService::Deal %d result msg successfully sent\n", bet_status -> bet_id);
}

void OlyClientDealService::send_deal_result(deal_structs::DealResult* deal_result)
{
  assert(deal_result);

  deal_result -> mtype = DEAL_RESULTS_MTYPE;

  if(msgsnd(asset_deals_queue_fd, (deal_structs::DealResult*) deal_result, sizeof(deal_structs::DealResult) - sizeof(long), 0) < 0)
  {
    printf("OlyClientDealService::Can\'t send %d deal result message to queue\n", deal_result -> bet_id);
    perror("msgsnd");
  }
  else
    printf("OlyClientDealService::Deal %d result msg successfully sent\n", deal_result -> bet_id);
}

//-----------------------------------------------------------------------------------------------
//-----------------------------------------------------------------------------------------------

OlyClientDealService::OlyClientDealService()
{
  parsed_deals_status = NULL;
  ping_success = 0;
  asset_deals_queue_fd = 0;
  free_bet_array_positions_amount = MAX_DEAL_AMOUNT;
  
  char current_username[MAX_USERNAME_LENGTH] = {};
  getlogin_r(current_username, MAX_USERNAME_LENGTH);

  deals_queue_file_pathname =  std::string("/home/") + std::string(current_username) + std::string("/NEURO_DEAL_ACTIONS");

  open_deals_queue();

  std::string assets_filepath = std::string("/home/") + std::string(current_username) + asset_names_filename;
  names.load_asset_names(&assets_filepath);
  assets_amount = names.get_assets_amount();

  for(int i = 0; i < MAX_DEAL_AMOUNT; i++)
  {
    bets_array[i] = {};
    bet_status_recieved[i] = 0;
  }
}

void OlyClientDealService::service_deal_status()
{
  int deals_status_served = 0;
  int new_status_amount = parsed_deals_status -> get_new_bet_status_amount();

  for(int i = 0; i < new_status_amount; i++)
  {
    AssetDeal deal_to_serve = parsed_deals_status -> get_deal_status();
    
    if(deal_to_serve.result)
    { 
      int bet_num = define_bet_id(&deal_to_serve);
      
      if(bet_num == -1)
      {
        printf("OlyClientDealService::service_deal_status:: bet status handle skipped:: Cannot find bet_id or asset, returned bet %s\n", deal_to_serve.pair.c_str());
      }
      else
      {
        bet_status_recieved[bet_num] = true;
        deal_structs::DealStatus current_bet_status = {DEAL_STATUS_MTYPE, bets_array[bet_num].bet_id, deal_to_serve.result};
        send_deal_status(&current_bet_status);
        deals_status_served++;
      }
    }
  }

  for(int i = 0; i < new_status_amount - deals_status_served; i++)
  {
    for(int i = 0; i < MAX_DEAL_AMOUNT; i++)
    {
      if(bets_array[i].bet_id && !bet_status_recieved[i])
      {
        deal_structs::DealStatus current_bet_status = {DEAL_STATUS_MTYPE, bets_array[i].bet_id, false};
        send_deal_status(&current_bet_status);
        bets_array[i] = {};
        free_bet_array_positions_amount++;
      }
    }
  }
}

int OlyClientDealService::define_bet_id(const AssetDeal* deal_to_serve)
{
  int asset = find_asset_num(&(deal_to_serve -> pair));

  if(asset == -1)
  {
    printf("OlyClientDealService::define_bet_id::Can't find the asset name:: %s \n", deal_to_serve -> pair.c_str());
    return -1;
  }

  for(int i = 0; i < MAX_DEAL_AMOUNT; i++)
  {
    if(bets_array[i].asset == asset && bets_array[i].deal_amount == deal_to_serve -> amount && bets_array[i].direction == deal_to_serve -> direction)
    {
      return i;
    }
  }

  return -1;
}

int OlyClientDealService::find_asset_num(const std::string* asset)
{
  for(int i = 0; i < assets_amount; i++)
  {
    if(*(names.get_asset_name(i)) == *asset)
      return i;
  }

  return -1;
}

void OlyClientDealService::update_deals(ParseOlymptradeJSON* parsed_data)
{
  assert(parsed_data);

  static int first_time_called = 1;
  static int prev_top_deal_id = 0;
  static int current_deal_id = 0;
  deal_structs::DealResult deal_result_to_send = {};
  const AssetDeal* serviced_deal = NULL;
  int while_cycle_counter = 0;
  int bet_array_position = 0;
  bool deal_result = 0;

  if(first_time_called)
  {
    first_time_called = 0;
    prev_top_deal_id = (parsed_data -> get_finished_deal(0)) -> deal_id;
    current_deal_id = prev_top_deal_id;
    return;
  }

  current_deal_id = (parsed_data -> get_finished_deal(0)) -> deal_id;

  if(current_deal_id != prev_top_deal_id)
  {
    while(current_deal_id != prev_top_deal_id || while_cycle_counter != 10)
    {
      serviced_deal = parsed_data -> get_finished_deal(while_cycle_counter);
      bet_array_position = define_bet_id(serviced_deal);

      if(bet_array_position == -1)
      {
        printf("Cannot define bet id for bet with olymptrade id %d\n", serviced_deal -> deal_id);
        return;
      }

      for(int i = 0; i < 2; i++)
      {
        if(serviced_deal -> status == bets_result_status[i])
        {
          deal_result = i;
          break;
        }
      }

      deal_result_to_send = {DEAL_RESULTS_MTYPE, bets_array[bet_array_position].bet_id, 
                             deal_result, serviced_deal -> balance_change, serviced_deal -> balance_result};
      send_deal_result(&deal_result_to_send);
      bets_array[bet_array_position] = {};
      free_bet_array_positions_amount++;
      while_cycle_counter++;
    }
  }
}