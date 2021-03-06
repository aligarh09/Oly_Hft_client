#include "ParseOlymptradeJSON.h"

std::mutex bet_status_mutex;

//-----------------------------------------------------------------------------------------------
//-----------------------------------------------------------------------------------------------

ParseOlymptradeJSON::ParseOlymptradeJSON()
{
  char current_username[200] = {};          //TODO global const
  getlogin_r(current_username, 200);

  std::string assets_filepath = std::string("/home/") + std::string(current_username) + asset_names_filename; //TODO const asset_names_filename
  names.load_asset_names(&assets_filepath);
  assets_amount = names.get_assets_amount();

  for(int i = 0; i < assets_amount; i++)
  {
    AssetStatus new_status = {};
    assets_array.push_back(new_status);
  }

  for(int i = 0; i < MAX_DEAL_AMOUNT; i++)
  {
    finished_deals[i] = {};
  }

  balance = 0;
  demo_balance = 0;
  login_result = 0;
  bets_new_status_counter = 0;
}

//-----------------------------------------------------------------------------------------------
//-----------------------------------------------------------------------------------------------

void ParseOlymptradeJSON::parse_login_json(std::string input)
{
  std::string in_error;
  json11::Json in_json = json11::Json::parse((const char *)input.c_str(), in_error);

  if (!in_error.empty()) 
  {
    printf("json '%s' parse error '%s', skipping\n", input.c_str(), in_error.c_str());
  }
  //test
  if(!in_json.is_object())
  {
    return;
  }
  //assert(!in_json.is_object());

  auto input_json = in_json.object_items();
  assert(!input_json.empty());

  login_result = input_json["result"].bool_value();

  if(input_json.count("error"))
    login_error_msg = input_json["error"].string_value();
}

void ParseOlymptradeJSON::parse_update_json(std::string input)
{
  std::string in_error;
  json11::Json in_json = json11::Json::parse((const char *)input.c_str(), in_error);

  if(!in_error.empty()) 
  {
    printf("json '%s' parse error '%s', skipping\n", input.c_str(), in_error.c_str());
  }
    //test
  if(!in_json.is_object())
  {
    return;
  }
  //assert(!in_json.is_object());
  auto input_json = in_json.object_items();
  assert(!input_json.empty());

  auto pairs = input_json["pairs"]["pairs"].object_items();
  assert(!pairs.empty());

  auto user_deals = input_json["user"]["deals"].object_items();
  assert(!user_deals.empty());

  balance = input_json["user"]["balance"].number_value();

  demo_balance = input_json["user"]["balance_demo"].number_value();

  for(int i = 0; i < assets_amount; i++)
  {
    if(pairs[names.get_asset_name(i) -> c_str()]["locked"].bool_value())
      assets_array[i].locked = true;
    else
      assets_array[i].locked = false;

    assets_array[i].sentiment = pairs[names.get_asset_name(i) -> c_str()]["sentiment"].int_value();

    assets_array[i].winperc   = pairs[names.get_asset_name(i) -> c_str()]["winperc"].int_value();   
  }

  auto user_deals_current = user_deals["current"].array_items();
  auto user_deals_finished = user_deals["finished"].array_items();
        
  for(int i = 0; i < user_deals_current.size(); i++)
  {
    auto temp1 = user_deals_current[i];
    auto temp2 = temp1.object_items();
    parse_deals_data(temp2, current_deals, i);
  }

  for(int i = 0; i < user_deals_finished.size(); i++)
  {
    auto temp1 = user_deals_finished[i];
    auto temp2 = temp1.object_items();
    parse_deals_data(temp2, finished_deals, i);
  }
}

void ParseOlymptradeJSON::parse_bet_response_json(std::string input)
{
  bet_status_mutex.lock();

  AssetDeal new_deal_status = {};

  std::string in_error;
  json11::Json in_json = json11::Json::parse((const char *)input.c_str(), in_error);

  if (!in_error.empty()) 
  {
    printf("json '%s' parse error '%s', skipping\n", input.c_str(), in_error.c_str());
  }
    //test
  if(!in_json.is_object())
  {
    return;
  }
  //assert(!in_json.is_object());

  auto input_json = in_json.object_items();
  assert(!input_json.empty());

  new_deal_status.result = input_json["result"].bool_value();

  if(new_deal_status.result == 1)
  {
    auto deal = input_json["deal"].object_items();
    assert(!deal.empty());

    parse_deals_data(deal, &new_deal_status, 0);
  }
  else
    new_deal_status.amount = -1;

  bets_new_status_counter++;

  bet_status_response.push_back(new_deal_status);

  bet_status_mutex.unlock();
}

void ParseOlymptradeJSON::parse_deals_data(std::map< std::string, json11::Json >& instance_DealsCurrentFinished, struct AssetDeal* deals_array, int cur_position)
{
  auto this_deal = instance_DealsCurrentFinished;

  if(this_deal.count("amount"))
    deals_array[cur_position].amount             = this_deal["amount"].int_value();
      
  if(this_deal.count("balance_change"))
    deals_array[cur_position].balance_change     = this_deal["balance_change"].number_value();

  if(this_deal.count("balance_result"))
    deals_array[cur_position].balance_result     = this_deal["balance_result"].number_value();

  if(this_deal.count("cancel_percent"))
    deals_array[cur_position].cancel_percent     = this_deal["cancel_percent"].int_value();
  
  if(this_deal.count("curs_close"))
    deals_array[cur_position].curs_close         = this_deal["curs_close"].number_value();
  
  if(this_deal.count("curs_current"))
    deals_array[cur_position].curs_current       = this_deal["curs_current"].number_value();

  if(this_deal.count("curs_open"))
    deals_array[cur_position].curs_open          = this_deal["curs_open"].number_value();

  if(this_deal.count("demo"))
    deals_array[cur_position].demo               = this_deal["demo"].bool_value();

  if(this_deal.count("direction"))
    deals_array[cur_position].direction          = this_deal["direction"].bool_value();

  if(this_deal.count("interim_status"))
    deals_array[cur_position].interim_status     = this_deal["interim_status"].string_value();

  if(this_deal.count("pair"))
    deals_array[cur_position].pair               = this_deal["pair"].string_value();

  if(this_deal.count("status"))
    deals_array[cur_position].status             = this_deal["status"].string_value();

  if(this_deal.count("time_open"))
    deals_array[cur_position].time_open          = this_deal["time_open"].int_value();

  if(this_deal.count("time_close_default"))
    deals_array[cur_position].time_close_default = this_deal["time_close_default"].string_value();

  if(this_deal.count("time_close"))
    deals_array[cur_position].time_close         = this_deal["time_close"].int_value();

  if(this_deal.count("winperc"))
    deals_array[cur_position].winperc            = this_deal["winperc"].int_value();

  if(this_deal.count("id"))
    deals_array[cur_position].deal_id            = this_deal["id"].int_value();
}

//-----------------------------------------------------------------------------------------------
//-----------------------------------------------------------------------------------------------

double ParseOlymptradeJSON::get_balance()
{
  return balance;
}

double ParseOlymptradeJSON::get_demo_balance()
{
  return demo_balance;
}

//-----------------------------------------------------------------------------------------------

int ParseOlymptradeJSON::get_new_bet_status_amount()
{
  return bets_new_status_counter;
}

AssetDeal ParseOlymptradeJSON::get_deal_status()
{
  bets_new_status_counter--;

  AssetDeal deal_to_return = bet_status_response.back();

  bet_status_response.pop_back();

  return deal_to_return;
}

//-----------------------------------------------------------------------------------------------


int ParseOlymptradeJSON::get_login_result()
{
  return login_result;
}

const std::string& ParseOlymptradeJSON::get_login_error()
{
  return login_error_msg;
}

//-----------------------------------------------------------------------------------------------


const AssetDeal* ParseOlymptradeJSON::get_current_deal(int deal_num)
{
  return &current_deals[deal_num];
}

const AssetDeal* ParseOlymptradeJSON::get_finished_deal(int deal_num)
{
  return &finished_deals[deal_num];
}

//-----------------------------------------------------------------------------------------------

const AssetStatus& ParseOlymptradeJSON::get_asset_status(int asset)
{
  return assets_array[asset];
}

const std::string& ParseOlymptradeJSON::get_asset_name(int asset_num)
{
  return *(names.get_asset_name(asset_num));
}

//-----------------------------------------------------------------------------------------------
//-----------------------------------------------------------------------------------------------

void ParseOlymptradeJSON::dump_asset_deal_struct(AssetDeal &struct_to_dump, FILE* dump_file)
{
  fprintf(dump_file, "bet result: %d ", struct_to_dump.result);
  fprintf(dump_file, "amount [%d], time_open [%d], time_close [%d], winperc [%d], cancel_percent [%d]\n", struct_to_dump.amount, struct_to_dump.time_open, struct_to_dump.time_close, struct_to_dump.winperc, struct_to_dump.cancel_percent);
  fprintf(dump_file, "balance_change [%lg], balance_result [%lg], curs_close [%lg], curs_current [%lg], curs_open [%lg]\n", struct_to_dump.balance_change, struct_to_dump.balance_result, struct_to_dump.curs_close, struct_to_dump.curs_current, struct_to_dump.curs_open);
  fprintf(dump_file, "demo [%d], direction [%d]\n", struct_to_dump.demo, struct_to_dump.direction);
  fprintf(dump_file, "interim_status [%s], pair [%s], status [%s], time_close_default [%s]\n", struct_to_dump.interim_status.c_str(), struct_to_dump.pair.c_str(), struct_to_dump.status.c_str(), struct_to_dump.time_close_default.c_str());
}

void ParseOlymptradeJSON::dump_update_data(FILE* file_dump_to)
{
  assert(file_dump_to);

  fprintf(file_dump_to, "balance: %lg; demo_balance: %lg\n", balance, demo_balance);

  for(int i = 0; i < assets_amount; i++)
  {
    fprintf(file_dump_to, "array place: [%d], sentiment [%d], winperc [%d], locked [%d]\n", i, assets_array[i].sentiment, assets_array[i].winperc, assets_array[i].locked);
  }

  for(int i = 0; i < MAX_DEAL_AMOUNT; i++)
  {
    dump_asset_deal_struct(current_deals[i], file_dump_to);
  }

  for(int i = 0; i < MAX_DEAL_AMOUNT; i++)
  {
    dump_asset_deal_struct(finished_deals[i], file_dump_to);
  }
}

void ParseOlymptradeJSON::dump_bet_result_data(FILE* file_dump_to)
{
  assert(file_dump_to);

  for(int i = 0; i < bets_new_status_counter; i++)
  {
    dump_asset_deal_struct(bet_status_response[i], file_dump_to);
  }
}