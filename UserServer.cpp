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

const string read_entity_admin {"ReadEntityAdmin"};
const string update_entity_admin {"UpdateEntityAdmin"};

const string read_entity_auth {"ReadEntityAuth"};
const string update_entity_auth {"UpdateEntityAuth"};

const string get_read_token_op {"GetReadToken"};
const string get_update_token_op {"GetUpdateToken"};

const string auth_table_name {"AuthTable"};
const string auth_table_userid_partition {"Userid"};
const string auth_table_password_prop {"Password"};
const string auth_table_partition_prop {"DataPartition"};
const string auth_table_row_prop {"DataRow"};
const string data_table_name {"DataTable"};

/*
  A map that maps each userid  to a tuple comprising a token, a DataPartition, and a DataRow. 
  When the user signs off, the entry is erased from the map
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
  message.reply(status_codes::NotImplemented);
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
  value pwd {
    build_json_object (
        vector<pair<string,string>> 
          {make_pair(message_properties.begin()->first, 
                     message_properties.begin()->second)})};
  if (paths[0] == "SignOn") {

    pair<status_code,value> result {do_request (methods::GET,
                                                auth_addr +
                                                get_update_token_op + "/" +
                                                userid,
                                                pwd)}; 
    cout << "token " << result.second << endl;

    if (result.first == status_codes::OK) {
      if (user_map.size() == 0){
        string token {result.second["token"].as_string()};
        pair<status_code,value> get_auth_props {do_request (methods::GET,
                                                        addr +
                                                        read_entity_admin + "/" +
                                                        auth_table_name + "/" +
                                                        auth_table_userid_partition + "/" +
                                                        userid)};

        unordered_map<string, string> userid_props {unpack_json_object (get_auth_props.second)};
        pair<status_code,value> exist_chk {do_request (methods::GET,
                                                       addr +
                                                       read_entity_auth + "/" +
                                                       data_table_name + "/" +
                                                       token + "/" +
                                                       userid_props[auth_table_partition_prop] + "/" +
                                                       userid_props[auth_table_row_prop])};

        if (exist_chk.first == status_codes::OK) {
          three_tuple_string user_map_vals {make_tuple(token, 
                                                       userid_props[auth_table_partition_prop], 
                                                       userid_props[auth_table_row_prop])};
          user_map[userid] = user_map_vals;
          //cout << get<1>(user_map[userid]) << endl;
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

  else if (paths[0] == "SignOff") {
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
  message.reply(status_codes::NotImplemented);
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
