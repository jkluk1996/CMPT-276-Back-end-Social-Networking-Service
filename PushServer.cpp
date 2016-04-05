/*
 Push Server code for CMPT 276, Spring 2016.
 */

#include <iostream>
#include <string>
#include <unordered_map>
#include <vector>

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

using web::http::http_headers;
using web::http::http_request;
using web::http::methods;
using web::http::status_code;
using web::http::status_codes;
using web::http::uri;

using web::json::value;

using web::http::experimental::listener::http_listener;

using prop_str_vals_t = vector<pair<string,string>>;

constexpr const char* def_url = "http://localhost:34574";
constexpr const char* addr {"http://localhost:34568/"};
constexpr const char* auth_addr {"http://localhost:34570/"};

const string read_entity_admin {"ReadEntityAdmin"};
const string update_entity_admin {"UpdateEntityAdmin"};

const string data_table_name {"DataTable"};
const string data_table_friends_prop {"Friends"};
const string data_table_update_prop {"Updates"};

/*
  Given an HTTP message with a JSON body, return the JSON
  body as an unordered map of strings to strings.

  Note that all types of JSON values are returned as strings.
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
  cout << endl << "**** GET " << path << endl;
}

/*
  Top-level routine for processing all HTTP POST requests.
 */
void handle_post(http_request message) {
  string path {uri::decode(message.relative_uri().path())};
  cout << endl << "**** PushServer POST " << path << endl;
  auto paths = uri::split_path(path);
  if (paths.size() != 4 || paths[0] != "PushStatus") {
    message.reply(status_codes::BadRequest);
  }
  assert(paths[0] == "PushStatus");
  string user_country {paths[1]};
  string user_name {paths[2]};
  string status {paths[3]};
  //Assuming status stored in updates are of the form user_country;user_name;status\n
  string new_status {user_country + ";" + user_name + ";" + status +"\n"};

  unordered_map<string, string> friend_map {get_json_body(message)};
  if (friend_map.size() == 1 
      && friend_map.begin()->first == data_table_friends_prop) {
    friends_list_t friend_list {parse_friends_list(friend_map.begin()->second)};
    
    for (auto user_friend = friend_list.begin(); user_friend != friend_list.end(); ++user_friend) {
      pair<status_code,value> get_old_updates {do_request (methods::GET,
                                                           addr + 
                                                           read_entity_admin + "/" + 
                                                           data_table_name + "/" + 
                                                           user_friend->first + "/" +
                                                           user_friend->second)};
      if (get_old_updates.first != status_codes::OK) {
        continue;
      }

      unordered_map<string,string> user_friend_properties {unpack_json_object(get_old_updates.second)};
      string updates {user_friend_properties[data_table_friends_prop]};

      updates += new_status;

      pair<status_code,value> update_friend {do_request (methods::PUT,
                                                         addr + 
                                                         update_entity_admin + "/" + 
                                                         data_table_name + "/" + 
                                                         user_friend->first + "/" +
                                                         user_friend->second,
                                                         value::object(vector<pair<string,value>>
                                                                       {make_pair(data_table_update_prop,
                                                                                  value::string(updates))}))};
    }
    message.reply(status_codes::OK);
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
  cout << endl << "**** PUT " << path << endl;
}

/*
  Top-level routine for processing all HTTP DELETE requests.
 */
void handle_delete(http_request message) {
  string path {uri::decode(message.relative_uri().path())};
  cout << endl << "**** DELETE " << path << endl;
}

/*
  Push server routine

  Install handlers for the HTTP requests and open the listener,
  which processes each request asynchronously.

  Note that, PushServer only installs the listeners for POST. 
  Any other HTTP method will produce a Method Not Allowed (405) response.

  If you want to support other methods, uncomment
  the call below that hooks in a the appropriate 
  listener.
  
  Wait for a carriage return, then shut the server down.
 */
int main (int argc, char const * argv[]) {
  cout << "PushServer: Opening listener" << endl;
  http_listener listener {def_url};
  //listener.support(methods::GET, &handle_get);
  listener.support(methods::POST, &handle_post);
  //listener.support(methods::PUT, &handle_put);
  //listener.support(methods::DEL, &handle_delete);
  listener.open().wait(); // Wait for listener to complete starting

  cout << "Enter carriage return to stop PushServer." << endl;
  string line;
  getline(std::cin, line);

  // Shut it down
  listener.close().wait();
  cout << "PushServer closed" << endl;
}
