/*
 User Server code for CMPT 276, Spring 2016.
 */

#include <iostream>
#include <string>
#include <unordered_map>
#include <vector>
#include <tuple>

#include <cpprest/http_listener.h>
#include <cpprest/json.h>

#include <was/common.h>
#include <was/table.h>

#include "make_unique.h"
#include "ClientUtils.h"

using azure::storage::storage_exception;
using azure::storage::cloud_table;
using azure::storage::cloud_table_client;
using azure::storage::edm_type;
using azure::storage::entity_property;
using azure::storage::table_entity;
using azure::storage::table_operation;
using azure::storage::table_request_options;
using azure::storage::table_result;
using azure::storage::table_shared_access_policy;

using std::cin;
using std::cout;
using std::endl;
using std::getline;
using std::make_pair;
using std::pair;
using std::string;
using std::unordered_map;
using std::vector;
using std::get;
using std::make_tuple;
using std::tuple;

using web::http::http_headers;
using web::http::http_request;
using web::http::methods;
using web::http::status_code;
using web::http::status_codes;
using web::http::uri;

using web::json::value;

using web::http::experimental::listener::http_listener;

using prop_str_vals_t = vector<pair<string,string>>;

constexpr const char* def_url = "http://localhost:34572";
constexpr const char* addr {"http://localhost:34568/"};
constexpr const char* auth_addr {"http://localhost:34570/"};
constexpr const char* push_addr {"http://localhost:34574/"};

const string read_entity_auth {"ReadEntityAuth"};
const string update_entity_auth {"UpdateEntityAuth"};

const string get_read_token_op {"GetReadToken"};
const string get_update_token_op {"GetUpdateToken"};
const string get_update_data_op {"GetUpdateData"};

const string auth_table_name {"AuthTable"};
const string auth_table_userid_partition {"Userid"};
const string auth_table_password_prop {"Password"};
const string auth_table_partition_prop {"DataPartition"};
const string auth_table_row_prop {"DataRow"};
const string data_table_name {"DataTable"};
const string data_table_friends_prop {"Friends"};
const string data_table_status_prop {"Status"};

const string sign_on_op {"SignOn"};
const string sign_off_op {"SignOff"};
const string add_friend_op {"AddFriend"};
const string remove_friend_op {"UnFriend"};
const string read_friend_list_op {"ReadFriendList"};
const string update_status_op {"UpdateStatus"};
const string push_status_op {"PushStatus"};

/*
  A map that maps each userid  to a tuple comprising a token, a DataPartition, and a DataRow. 
  When the user signs off, the entry is erased from the map
  get<0>(user_map[userid]) = token
  get<1>(user_map[userid]) = DataPartition
  get<2>(user_map[userid]) = DataRow
*/
typedef tuple<string, string, string> three_tuple_string;
unordered_map<string, three_tuple_string> user_map {};

/*
  Utility to create JSON object value from vector of properties
*/
value build_json_object (const vector<pair<string,string>>& properties) {
    value result {value::object ()};
    for (auto& prop : properties) {
      result[prop.first] = value::string(prop.second);
    }
    return result;
}

/*
  Given an HTTP message with a JSON body, return the JSON
  body as an unordered map of strings to strings.

  Note that all types of JSO
  N values are returned as strings.
  Use C++ conversion utilities to convert to numbers or dates
  as necessary.
 */
unordered_map<string,string> get_json_body(http_request message) {  
  unordered_map<string,string> results {};
  const http_headers& headers {message.headers()};
  auto content_type (headers.find("Content-Type"));
  if (content_type == headers.end() ||
      content_type->second != "application/json")
    return results;

  value json{};
  message.extract_json(true)
    .then([&json](value v) -> bool
          {
            json = v;
            return true;
          })
    .wait();

  if (json.is_object()) {
    for (const auto& v : json.as_object()) {
      if (v.second.is_string()) {
        results[v.first] = v.second.as_string();
      }
      else {
        results[v.first] = v.second.serialize();
      }
    }
  }
  return results;
}

/*
  Top-level routine for processing all HTTP GET requests.
 */
void handle_get(http_request message) { 
  string path {uri::decode(message.relative_uri().path())};
  cout << endl << "**** UserServer GET " << path << endl;
  auto paths = uri::split_path(path);
  //Needs at least an operation and userid
  if (paths.size() < 2) {
    message.reply(status_codes::BadRequest);
    return;
  }

  string userid {paths[1]};
  if (user_map.find(userid) == user_map.end()) {
      message.reply(status_codes::Forbidden);
      return;
  }

  if (paths[0] == read_friend_list_op) {
    pair<status_code,value> result {do_request (methods::GET,
                                                addr +
                                                read_entity_auth + "/" +
                                                data_table_name + "/" + 
                                                get<0>(user_map[userid]) + "/" +
                                                get<1>(user_map[userid]) + "/" +
                                                get<2>(user_map[userid]))}; 
    
    if (result.first == status_codes::OK) {
      unordered_map<string, string> data_props {unpack_json_object (result.second)};
      vector<pair<string,value>> json_friends {
                make_pair(data_table_friends_prop, 
                          value::string(data_props[data_table_friends_prop]))};
      
      message.reply(result.first, value::object(json_friends));
    }
    else {
      message.reply(result.first);
    }
  }

  else {
    message.reply(status_codes::BadRequest);
  }
}

/*
  Top-level routine for processing all HTTP POST requests.
 */
void handle_post(http_request message) {
  string path {uri::decode(message.relative_uri().path())};
  cout << endl << "**** UserServer POST " << path << endl;
  auto paths = uri::split_path(path);
  //Needs at least an operation and userid
  if (paths.size() < 2) {
    message.reply(status_codes::BadRequest);
    return;
  }

  string userid {paths[1]};
  unordered_map<string, string> message_properties {get_json_body(message)};

  if (paths[0] == sign_on_op) {
    if (message_properties.size() != 1) {
      message.reply(status_codes::BadRequest);
      return;
    }

    value pwd {
    build_json_object (
        vector<pair<string,string>> 
          {make_pair(message_properties.begin()->first, 
                     message_properties.begin()->second)})};

    pair<status_code,value> result {do_request (methods::GET,
                                                auth_addr +
                                                get_update_data_op + "/" +
                                                userid,
                                                pwd)}; 

    if (result.first == status_codes::OK) {
      if (user_map.find(userid) == user_map.end()){
        unordered_map<string, string> auth_props {unpack_json_object (result.second)};
        string token {result.second["token"].as_string()};
        
        cout << "token: " << token << endl;
        cout << auth_table_partition_prop << ": " << auth_props[auth_table_partition_prop] << endl;
        cout << auth_table_row_prop << ": " << auth_props[auth_table_row_prop] << endl;

        pair<status_code,value> exist_chk {do_request (methods::GET,
                                                       addr +
                                                       read_entity_auth + "/" +
                                                       data_table_name + "/" +
                                                       token + "/" +
                                                       auth_props[auth_table_partition_prop] + "/" +
                                                       auth_props[auth_table_row_prop])};

        if (exist_chk.first == status_codes::OK) {
          three_tuple_string user_map_vals {make_tuple(token, 
                                                       auth_props[auth_table_partition_prop], 
                                                       auth_props[auth_table_row_prop])};
          user_map[userid] = user_map_vals;
          //added Does this need to return token as second param?
          message.reply(result.first);
        }

        else {
          message.reply(status_codes::NotFound);
        }
      }

      else {
        message.reply(status_codes::OK);
      }
    }

    else {
      message.reply(result.first);
    }
  }

  else if (paths[0] == sign_off_op) {
    if (message_properties.size() != 0) {
      message.reply(status_codes::BadRequest);
      return;
    }

    if (user_map.find(userid) != user_map.end()) {
      user_map.erase(userid);
      message.reply(status_codes::OK);
    }

    else {
      message.reply(status_codes::NotFound);
    }
  }

  else {
    message.reply(status_codes::BadRequest);
  }
}

/*
  Top-level routine for processing all HTTP PUT requests.
 */
void handle_put(http_request message) {
  string path {uri::decode(message.relative_uri().path())};
  cout << endl << "**** UserServer PUT " << path << endl;
  auto paths = uri::split_path(path);
  //Needs at least an operation and userid
  if (paths.size() < 2) {
    message.reply(status_codes::BadRequest);
    return;
  }

  string userid {paths[1]};
  if (user_map.find(userid) == user_map.end()) {
      message.reply(status_codes::Forbidden);
      return;
  }

  string token {get<0>(user_map[userid])};
  string data_partition {get<1>(user_map[userid])};
  string data_row {get<2>(user_map[userid])};

  if (paths[0] == add_friend_op) {
	  // Needs four parameters
    if (paths.size() != 4) {
      message.reply(status_codes::BadRequest);  
      return;
    }
	
  	// Create string country and name
  	// Create string new_friend with country and name separated by ';' character
  	string country {paths[2]}; 
  	string name {paths[3]};
  	string new_friend {country + ";" + name};
  	
  	// Retrieve friends list and assign it to string friend_list
  	pair<status_code,value> get_friends {do_request (methods::GET,
                                                     string(def_url) + "/" + 
                                                     read_friend_list_op + "/" + 
                                                     userid)};		
  	
    if (get_friends.first != status_codes::OK) {
      message.reply(get_friends.first);
      return;
    }

    unordered_map<string, string> friend_prop {unpack_json_object (get_friends.second)};
    string friend_list_string {friend_prop[data_table_friends_prop]};

  	// Search for new_friend in friend_list to see if the friend already exists in the user's friendlist
  	std::size_t found = friend_list_string.find(new_friend);
  	
  	// if it does, then return status code OK
  	if (found != string::npos) {
    	message.reply(status_codes::OK);
    	return;
  	}
  	else {
  	  // Otherwise, add the new friend to the user's friends list
      friends_list_t friend_list_vector {parse_friends_list(friend_list_string)};
      friend_list_vector.push_back(make_pair(country,name));
      friend_list_string = friends_list_to_string(friend_list_vector);

      pair<status_code,value> update_result {do_request (methods::PUT,
                                                         addr +
                                                         update_entity_auth + "/" +
                                                         data_table_name + "/" +
                                                         token + "/" +
                                                         data_partition + "/" +
                                                         data_row,
                                                         value::object (vector<pair<string,value>>
                                                                        {make_pair(data_table_friends_prop,
                                                                                   value::string(friend_list_string))}))}; 
  	  message.reply(update_result.first);
    }
  }

  else if (paths[0] == remove_friend_op) {
    // Needs four parameters
  	if (paths.size() != 4) {
      message.reply(status_codes::BadRequest);  
      return;
    }
  	  
  	// Create string country and name
  	// Create string new_friend with country and name separated by ';' character
  	string country {paths[2]}; 
  	string name {paths[3]};
  	string new_friend {country + ";" + name};
  	  
  	// Retrieve friends list and assign it to string friend_list
  	pair<status_code,value> get_friends {do_request (methods::GET,
                                                     string(def_url) + "/" + 
                                                     read_friend_list_op + "/" + 
                                                     userid)};		

    if (get_friends.first != status_codes::OK) {
      message.reply(get_friends.first);
      return;
    }

    unordered_map<string, string> friend_prop {unpack_json_object (get_friends.second)};
    string friend_list_string {friend_prop[data_table_friends_prop]};

  	
  	// Search for new_friend in friend_list to see if the friend exists in the user's friendlist
  	std::size_t found = friend_list_string.find(new_friend);  
  	  
  	// if it doesn't, then return status code OK
  	if (found == string::npos) {
      message.reply(status_codes::OK);
  	  return;
  	}

  	// Otherwise, delete friend
    else {
      friends_list_t friend_list_vector {parse_friends_list(friend_list_string)};
      friends_list_t new_friend_list_vector {};
      pair<string,string> new_friend_pair {make_pair(country,name)};
      for (auto user_friend = friend_list_vector.begin(); user_friend != friend_list_vector.end(); ++user_friend) {
        if (make_pair(user_friend->first, user_friend->second) != new_friend_pair) {
          new_friend_list_vector.push_back(make_pair(user_friend->first, user_friend->second));
        }
      }
      friend_list_string = friends_list_to_string(new_friend_list_vector);

      pair<status_code,value> update_result {do_request (methods::PUT,
                                                         addr +
                                                         update_entity_auth + "/" +
                                                         data_table_name + "/" +
                                                         token + "/" +
                                                         data_partition + "/" +
                                                         data_row,
                                                         value::object (vector<pair<string,value>>
                                                                        {make_pair(data_table_friends_prop,
                                                                                   value::string(friend_list_string))}))}; 
      message.reply(update_result.first);
    }
  }

  else if (paths[0] == update_status_op) {
    //Needs three parameters
    if (paths.size() != 3) {
      message.reply(status_codes::BadRequest);  
      return;
    }

    string status {paths[2]};
    pair<status_code,value> update_result {do_request (methods::PUT,
                                                addr +
                                                update_entity_auth + "/" +
                                                data_table_name + "/" +
                                                token + "/" +
                                                data_partition + "/" +
                                                data_row,
                                                value::object (vector<pair<string,value>>
                                                          {make_pair(data_table_status_prop,
                                                                     value::string(status))}))};  
    if (update_result.first == status_codes::OK) {
      pair<status_code,value> get_friends {do_request (methods::GET,
                                                       string(def_url) + "/" + 
                                                       read_friend_list_op + "/" + 
                                                       userid)};

      if (get_friends.first != status_codes::OK) {
        message.reply(get_friends.first);
        return;
      }

      try {
        pair<status_code,value> push_result {do_request (methods::POST,
                                                         push_addr + 
                                                         push_status_op + "/" + 
                                                         data_partition + "/" +
                                                         data_row + "/" + 
                                                         status,
                                                         get_friends.second)};
        message.reply(push_result.first);
      }

      catch (const std::exception& e) {
        cout << "Error: " << e.what() << endl;
        message.reply(status_codes::ServiceUnavailable);
      }
    }

    else {
      message.reply(update_result.first);
    }
  }

  else {
    message.reply(status_codes::BadRequest);
  }

}

/*
  Top-level routine for processing all HTTP DELETE requests.
 */
void handle_delete(http_request message) {
  string path {uri::decode(message.relative_uri().path())};
  cout << endl << "**** DELETE " << path << endl;
}

/*
  User server routine

  Install handlers for the HTTP requests and open the listener,
  which processes each request asynchronously.

  Note that, UserServer only installs the listeners for GET, 
  POST and PUT. Any other HTTP method will produce a Method 
  Not Allowed (405) response.

  If you want to support other methods, uncomment
  the call below that hooks in a the appropriate 
  listener.
  
  Wait for a carriage return, then shut the server down.
 */
int main (int argc, char const * argv[]) {
  cout << "UserServer: Opening listener" << endl;
  http_listener listener {def_url};
  listener.support(methods::GET, &handle_get);
  listener.support(methods::POST, &handle_post);
  listener.support(methods::PUT, &handle_put);
  //listener.support(methods::DEL, &handle_delete);
  listener.open().wait(); // Wait for listener to complete starting

  cout << "Enter carriage return to stop UserServer." << endl;
  string line;
  getline(std::cin, line);

  // Shut it down
  listener.close().wait();
  cout << "UserServer closed" << endl;
}
