/*
  Sample unit tests for BasicServer
 */

#include <algorithm>
#include <exception>
#include <iostream>
#include <string>
#include <utility>
#include <vector>

#include <cpprest/http_client.h>
#include <cpprest/json.h>

#include <pplx/pplxtasks.h>

#include <UnitTest++/UnitTest++.h>

using std::cerr;
using std::cout;
using std::endl;
using std::make_pair;
using std::pair;
using std::string;
using std::vector;

using web::http::http_headers;
using web::http::http_request;
using web::http::http_response;
using web::http::method;
using web::http::methods;
using web::http::status_code;
using web::http::status_codes;
using web::http::uri_builder;

using web::http::client::http_client;

using web::json::object;
using web::json::value;

const string create_table_op {"CreateTableAdmin"};
const string delete_table_op {"DeleteTableAdmin"};

const string read_entity_admin {"ReadEntityAdmin"};
const string update_entity_admin {"UpdateEntityAdmin"};
const string delete_entity_admin {"DeleteEntityAdmin"};

const string read_entity_auth {"ReadEntityAuth"};
const string update_entity_auth {"UpdateEntityAuth"};

const string get_read_token_op  {"GetReadToken"};
const string get_update_token_op {"GetUpdateToken"};

// The two optional operations from Assignment 1
const string add_property_admin {"AddPropertyAdmin"};
const string update_property_admin {"UpdatePropertyAdmin"};

const string sign_on_op {"SignOn"};
const string sign_off_op {"SignOff"};
const string read_friend_list_op {"ReadFriendList"};
const string update_status_op {"UpdateStatus"};
const string push_status_op {"PushStatus"};

/*
  Make an HTTP request, returning the status code and any JSON value in the body

  method: member of web::http::methods
  uri_string: uri of the request
  req_body: [optional] a json::value to be passed as the message body

  If the response has a body with Content-Type: application/json,
  the second part of the result is the json::value of the body.
  If the response does not have that Content-Type, the second part
  of the result is simply json::value {}.

  You're welcome to read this code but bear in mind: It's the single
  trickiest part of the sample code. You can just call it without
  attending to its internals, if you prefer.
 */

// Version with explicit third argument
pair<status_code,value> do_request (const method& http_method, const string& uri_string, const value& req_body) {
  http_request request {http_method};
  if (req_body != value {}) {
    http_headers& headers (request.headers());
    headers.add("Content-Type", "application/json");
    request.set_body(req_body);
  }

  status_code code;
  value resp_body;
  http_client client {uri_string};
  client.request (request)
    .then([&code](http_response response)
          {
            code = response.status_code();
            const http_headers& headers {response.headers()};
            auto content_type (headers.find("Content-Type"));
            if (content_type == headers.end() ||
                content_type->second != "application/json")
              return pplx::task<value> ([] { return value {};});
            else
              return response.extract_json();
          })
    .then([&resp_body](value v) -> void
          {
            resp_body = v;
            return;
          })
    .wait();
  return make_pair(code, resp_body);
}

// Version that defaults third argument
pair<status_code,value> do_request (const method& http_method, const string& uri_string) {
  return do_request (http_method, uri_string, value {});
}

/*
  Utility to create a table

  addr: Prefix of the URI (protocol, address, and port)
  table: Table in which to insert the entity
 */
int create_table (const string& addr, const string& table) {
  pair<status_code,value> result {do_request (methods::POST, addr + create_table_op + "/" + table)};
  return result.first;
}

/*
  Utility to compare two JSON objects

  This is an internal routine---you probably want to call compare_json_values().
 */
bool compare_json_objects (const object& expected_o, const object& actual_o) {
  CHECK_EQUAL (expected_o.size (), actual_o.size());
  if (expected_o.size() != actual_o.size())
    return false;

  bool result {true};
  for (auto& exp_prop : expected_o) {
    object::const_iterator act_prop {actual_o.find (exp_prop.first)};
    CHECK (actual_o.end () != act_prop);
    if (actual_o.end () == act_prop)
      result = false;
    else {
      CHECK_EQUAL (exp_prop.second, act_prop->second);
      if (exp_prop.second != act_prop->second)
        result = false;
    }
  }
  return result;
}

/*
  Utility to compare two JSON objects represented as values

  expected: json::value that was expected---must be an object
  actual: json::value that was actually returned---must be an object
*/
bool compare_json_values (const value& expected, const value& actual) {
  assert (expected.is_object());
  assert (actual.is_object());

  object expected_o {expected.as_object()};
  object actual_o {actual.as_object()};
  return compare_json_objects (expected_o, actual_o);
}

/*
  Utility to compre expected JSON array with actual

  exp: vector of objects, sorted by Partition/Row property 
    The routine will throw if exp is not sorted.
  actual: JSON array value of JSON objects
    The routine will throw if actual is not an array or if
    one or more values is not an object.

  Note the deliberate asymmetry of the how the two arguments are handled:

  exp is set up by the test, so we *require* it to be of the correct
  type (vector<object>) and to be sorted and throw if it is not.

  actual is returned by the database and may not be an array, may not
  be values, and may not be sorted by partition/row, so we have
  to check whether it has those characteristics and convert it 
  to a type comparable to exp.
*/
bool compare_json_arrays(const vector<object>& exp, const value& actual) {
  /*
    Check that expected argument really is sorted and
    that every value has Partion and Row properties.
    This is a precondition of this routine, so we throw
    if it is not met.
  */
  auto comp = [] (const object& a, const object& b) -> bool {
    return a.at("Partition").as_string()  <  b.at("Partition").as_string()
       ||
       (a.at("Partition").as_string() == b.at("Partition").as_string() &&
        a.at("Row").as_string()       <  b.at("Row").as_string()); 
  };
  if ( ! std::is_sorted(exp.begin(),
                         exp.end(),
                         comp))
    throw std::exception();

  // Check that actual is an array
  CHECK(actual.is_array());
  if ( ! actual.is_array())
    return false;
  web::json::array act_arr {actual.as_array()};

  // Check that the two arrays have same size
  CHECK_EQUAL(exp.size(), act_arr.size());
  if (exp.size() != act_arr.size())
    return false;

  // Check that all values in actual are objects
  bool all_objs {std::all_of(act_arr.begin(),
                             act_arr.end(),
                             [] (const value& v) { return v.is_object(); })};
  CHECK(all_objs);
  if ( ! all_objs)
    return false;

  // Convert all values in actual to objects
  vector<object> act_o {};
  auto make_object = [] (const value& v) -> object {
    return v.as_object();
  };
  std::transform (act_arr.begin(), act_arr.end(), std::back_inserter(act_o), make_object);

  /* 
     Ensure that the actual argument is sorted.
     Unlike exp, we cannot assume this argument is sorted,
     so we sort it.
   */
  std::sort(act_o.begin(), act_o.end(), comp);

  // Compare the sorted arrays
  bool eq {std::equal(exp.begin(), exp.end(), act_o.begin(), &compare_json_objects)};
  CHECK (eq);
  return eq;
}

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
  Utility to delete a table

  addr: Prefix of the URI (protocol, address, and port)
  table: Table in which to insert the entity
 */
int delete_table (const string& addr, const string& table) {
  // SIGH--Note that REST SDK uses "methods::DEL", not "methods::DELETE"
  pair<status_code,value> result {
    do_request (methods::DEL,
                addr + delete_table_op + "/" + table)};
  return result.first;
}

/*
  Utility to put an entity with a single property

  addr: Prefix of the URI (protocol, address, and port)
  table: Table in which to insert the entity
  partition: Partition of the entity 
  row: Row of the entity
  prop: Name of the property
  pstring: Value of the property, as a string
 */
int put_entity(const string& addr, const string& table, const string& partition, const string& row, const string& prop, const string& pstring) {
  pair<status_code,value> result {
    do_request (methods::PUT,
                addr + update_entity_admin + "/" + table + "/" + partition + "/" + row,
                value::object (vector<pair<string,value>>
                               {make_pair(prop, value::string(pstring))}))};
  return result.first;
}

/*
  Utility to put an entity with multiple properties

  addr: Prefix of the URI (protocol, address, and port)
  table: Table in which to insert the entity
  partition: Partition of the entity 
  row: Row of the entity
  props: vector of string/value pairs representing the properties
 */
int put_entity(const string& addr, const string& table, const string& partition, const string& row,
              const vector<pair<string,value>>& props) {
  pair<status_code,value> result {
    do_request (methods::PUT,
               addr + update_property_admin  + "/" + table + "/" + partition + "/" + row,
               value::object (props))};
  return result.first;
}

/*
  Utility to delete an entity

  addr: Prefix of the URI (protocol, address, and port)
  table: Table in which to insert the entity
  partition: Partition of the entity 
  row: Row of the entity
 */
int delete_entity (const string& addr, const string& table, const string& partition, const string& row)  {
  // SIGH--Note that REST SDK uses "methods::DEL", not "methods::DELETE"
  pair<status_code,value> result {
    do_request (methods::DEL,
                addr + delete_entity_admin + "/" + table + "/" + partition + "/" + row)};
  return result.first;
}

/*
  Utility to get a token good for updating a specific entry
  from a specific table for one day.
 */
pair<status_code,string> get_update_token(const string& addr,  const string& userid, const string& password) {
  value pwd {build_json_object (vector<pair<string,string>> {make_pair("Password", password)})};
  pair<status_code,value> result {do_request (methods::GET,
                                              addr +
                                              get_update_token_op + "/" +
                                              userid,
                                              pwd
                                              )};
  cerr << "token " << result.second << endl;
  if (result.first != status_codes::OK)
    return make_pair (result.first, "");
  else {
    string token {result.second["token"].as_string()};
    return make_pair (result.first, token);
  }
}

/*
  Utility to get a token good for reading a specific entry
  from a specific table for one day.
 */
pair<status_code,string> get_read_token(const string& addr,  const string& userid, const string& password) {
  value pwd {build_json_object (vector<pair<string,string>> {make_pair("Password", password)})};
  pair<status_code,value> result {do_request (methods::GET,
                                              addr +
                                              get_read_token_op + "/" +
                                              userid,
                                              pwd
                                              )};
  cerr << "token " << result.second << endl;
  if (result.first != status_codes::OK)
    return make_pair (result.first, "");
  else {
    string token {result.second["token"].as_string()};
    return make_pair (result.first, token);
  }
}

/*
  A sample fixture that ensures TestTable exists, and
  at least has the entity Franklin,Aretha/USA
  with the property "Song": "RESPECT".

  The entity is deleted when the fixture shuts down
  but the table is left. See the comments in the code
  for the reason for this design.
 */
class GetFixture {
public:
  static constexpr const char* addr {"http://localhost:34568/"};
  static constexpr const char* table {"TestTable"};
  static constexpr const char* partition {"USA"};
  static constexpr const char* row {"Franklin,Aretha"};
  static constexpr const char* property {"Song"};
  static constexpr const char* prop_val {"RESPECT"};

public:
  GetFixture() {
    int make_result {create_table(addr, table)};
    cerr << "create result " << make_result << endl;
    if (make_result != status_codes::Created && make_result != status_codes::Accepted) {
      throw std::exception();
    }
    int put_result {put_entity (addr, table, partition, row, property, prop_val)};
    cerr << "put result " << put_result << endl;
    if (put_result != status_codes::OK) {
      throw std::exception();
    }
  }

  ~GetFixture() {
    int del_ent_result {delete_entity (addr, table, partition, row)};
    if (del_ent_result != status_codes::OK) {
      throw std::exception();
    }

    /*
      In traditional unit testing, we might delete the table after every test.

      However, in cloud NoSQL environments (Azure Tables, Amazon DynamoDB)
      creating and deleting tables are rate-limited operations. So we
      leave the table after each test but delete all its entities.
    */
    cout << "Skipping table delete" << endl;
    /*
      int del_result {delete_table(addr, table)};
      cerr << "delete result " << del_result << endl;
      if (del_result != status_codes::OK) {
        throw std::exception();
      }
    */
  }
};

SUITE(GET) {
  /*
    A test of GET all table entries

    Demonstrates use of new compare_json_arrays() function.
   */
  TEST_FIXTURE(GetFixture, GetAll) {
    string partition {"Canada"};
    string row {"Katherines,The"};
    string property {"Home"};
    string prop_val {"Vancouver"};
    int put_result {put_entity (GetFixture::addr, GetFixture::table, partition, row, property, prop_val)};
    cerr << "put result " << put_result << endl;
    assert (put_result == status_codes::OK);

    pair<status_code,value> result {
      do_request (methods::GET,
                  string(GetFixture::addr)
                  + read_entity_admin + "/"
                  + string(GetFixture::table))};
    CHECK_EQUAL(status_codes::OK, result.first);
    value obj1 {
      value::object(vector<pair<string,value>> {
          make_pair(string("Partition"), value::string(partition)),
          make_pair(string("Row"), value::string(row)),
          make_pair(property, value::string(prop_val))
      })
    };
    value obj2 {
      value::object(vector<pair<string,value>> {
          make_pair(string("Partition"), value::string(GetFixture::partition)),
          make_pair(string("Row"), value::string(GetFixture::row)),
          make_pair(string(GetFixture::property), value::string(GetFixture::prop_val))
      })
    };
    vector<object> exp {
      obj1.as_object(),
      obj2.as_object()
    };
    compare_json_arrays(exp, result.second);
    CHECK_EQUAL(status_codes::OK, delete_entity (GetFixture::addr, GetFixture::table, partition, row));
  }

  /********Starting Tests for required operation 1 ********/
  /*  
    A simple test of GET by partition
   */
  TEST_FIXTURE(GetFixture, GetByPartition) {
    pair<status_code,value> result {
      do_request (methods::GET,
      string(GetFixture::addr)
      + read_entity_admin + "/"
      + GetFixture::table + "/"
      + GetFixture::partition + "/"
      + "*")};

    CHECK(result.second.is_array());
    CHECK_EQUAL(1,result.second.as_array().size());
    CHECK_EQUAL(status_codes::OK, result.first);
  }

  /*
    Another simple test of GET by partition
   */
  TEST_FIXTURE(GetFixture, GetByPartition2) {
    string partition {"Bennett,Chancelor"};
    string row {"USA"};
    string property {"Home"};
    string prop_val {"Chicago"};
    int put_result {put_entity (GetFixture::addr, GetFixture::table, partition, row, property, prop_val)};
    cerr << "put result " << put_result << endl;
    assert (put_result == status_codes::OK);

    partition = "Katherines,The";
    row = "Canada";
    property = "Home";
    prop_val = "Vancouver";
    put_result = put_entity (GetFixture::addr, GetFixture::table, partition, row, property, prop_val);
    cerr << "put result " << put_result << endl;
    assert (put_result == status_codes::OK);

    put_result = put_entity (GetFixture::addr, GetFixture::table, partition, "different_row", "property", "value");
    cerr << "put result " << put_result << endl;
    assert (put_result == status_codes::OK);

    pair<status_code,value> result {
      do_request (methods::GET,
      string(GetFixture::addr)
      + read_entity_admin + "/"
      + GetFixture::table + "/"
      + partition + "/"
      + "*")};

    CHECK(result.second.is_array());
    CHECK_EQUAL(2,result.second.as_array().size());
    CHECK_EQUAL(status_codes::OK, result.first);
    CHECK_EQUAL(status_codes::OK, delete_entity (GetFixture::addr, GetFixture::table, partition, row));
    CHECK_EQUAL(status_codes::OK, delete_entity (GetFixture::addr, GetFixture::table, partition, "different_row"));
    CHECK_EQUAL(status_codes::OK, delete_entity (GetFixture::addr, GetFixture::table, "Bennett,Chancelor", "USA"));
  }

  /*
    A test of GET by partition when table name is missing 
   */
  TEST_FIXTURE(GetFixture, GetByPartition_MissingTableName) {  
    pair<status_code,value> result {
        do_request (methods::GET,
        string(GetFixture::addr)
        + read_entity_admin + "/"
        + GetFixture::partition + "/"
        + "*")};

      CHECK_EQUAL(status_codes::BadRequest, result.first);
  }

  /*
  A test of GET by partition when partition name is missing
   */

TEST_FIXTURE(GetFixture, GetByPartition_MissingPartition) {
    pair<status_code,value> result {
        do_request (methods::GET,
        string(GetFixture::addr)
        + read_entity_admin + "/"
        + GetFixture::table + "/"
        + "*")};
        
      CHECK_EQUAL(status_codes::BadRequest, result.first);
  }

  /* 
    A test of GET by partition when "*" is missing
   */
  TEST_FIXTURE(GetFixture, GetByPartition_MissingRow) {
    pair<status_code,value> result {
        do_request (methods::GET,
        string(GetFixture::addr)
        + read_entity_admin + "/"
        + GetFixture::table + "/"
        + GetFixture::partition)};

      CHECK_EQUAL(status_codes::BadRequest, result.first);
  }

  /*
    A test of GET by partition, Table does not exist 
   */
  TEST_FIXTURE(GetFixture, GetByPartition_NonExistingTable) {      
    string partition {"Katherines,The"};
    string row {"Canada"};
    string property {"Home"};
    string prop_val {"Vancouver"};
    int put_result {put_entity (GetFixture::addr, GetFixture::table, partition, row, property, prop_val)};
    cerr << "put result " << put_result << endl;
    assert (put_result == status_codes::OK);
    
    pair<status_code,value> result {
      do_request (methods::GET,
        string(GetFixture::addr)
        + read_entity_admin + "/"
        + "Table_Doesnt_Exist", 
        value::object (vector<pair<string,value>>
        {make_pair("Property", value::string("*"))}))};
    
    CHECK_EQUAL(status_codes::NotFound, result.first);
    CHECK_EQUAL(status_codes::OK, delete_entity (GetFixture::addr, GetFixture::table, partition, row));
  }

  /*
    A test of GET by partition, no entities with specified partition
   */
  TEST_FIXTURE(GetFixture, GetByPartition_NonExistingPartition) {      
    string partition {"Katherines,The"};
    string row {"Canada"};
    string property {"Home"};
    string prop_val {"Vancouver"};
    int put_result {put_entity (GetFixture::addr, GetFixture::table, partition, row, property, prop_val)};
    cerr << "put result " << put_result << endl;
    assert (put_result == status_codes::OK);

    put_result = put_entity (GetFixture::addr, GetFixture::table, partition, "different_row", "property", "value");
    cerr << "put result " << put_result << endl;
    assert (put_result == status_codes::OK);
    
    pair<status_code,value> result {
      do_request (methods::GET,
      string(GetFixture::addr)
      + read_entity_admin + "/"
      + GetFixture::table + "/"
      + "Property_Doesnt_Exist" + "/"
      + "*")};
    
    CHECK(result.second.is_array());
    CHECK_EQUAL(0,result.second.as_array().size());
    CHECK_EQUAL(status_codes::OK, result.first);
    CHECK_EQUAL(status_codes::OK, delete_entity (GetFixture::addr, GetFixture::table, partition, row));
    CHECK_EQUAL(status_codes::OK, delete_entity (GetFixture::addr, GetFixture::table, partition, "different_row"));
  }


  /********Starting Tests for required operation 2 ********/
  /*
    A simple test of GET by properties
   */
  TEST_FIXTURE(GetFixture, GetByProp) {
    string partition {"Katherines,The"};
    string row {"Canada"};
    string property {"Home"};
    string prop_val {"Vancouver"};
    int put_result {put_entity (GetFixture::addr, GetFixture::table, partition, row, property, prop_val)};
    cerr << "put result " << put_result << endl;
    assert (put_result == status_codes::OK);

    put_result = put_entity (GetFixture::addr, GetFixture::table, partition, row, "prop", "prop_val");
    cerr << "put result " << put_result << endl;
    assert (put_result == status_codes::OK);

    pair<status_code,value> result {
      do_request (methods::GET,
      string(GetFixture::addr)
      + read_entity_admin + "/"
      + string(GetFixture::table),
      value::object (vector<pair<string,value>>
        {make_pair(property, value::string("*")),
         make_pair("prop", value::string("*"))}))};
    
    CHECK(result.second.is_array());
    CHECK_EQUAL(1, result.second.as_array().size());
    CHECK_EQUAL(status_codes::OK, result.first);
    CHECK_EQUAL(status_codes::OK, delete_entity (GetFixture::addr, GetFixture::table, partition, row));
  }

  /*
    Another simple test of GET by properties
   */
  TEST_FIXTURE(GetFixture, GetByProp2) {
    string partition {"Bennett,Chancelor"};
    string row {"USA"};
    string property {"Home"};
    string prop_val {"Chicago"};
    int put_result {put_entity (GetFixture::addr, GetFixture::table, partition, row, property, prop_val)};
    cerr << "put result " << put_result << endl;
    assert (put_result == status_codes::OK);

    partition = "Katherines,The";
    row = "Canada";
    property = "Home";
    prop_val = "Vancouver";
    put_result = put_entity (GetFixture::addr, GetFixture::table, partition, row, property, prop_val);
    cerr << "put result " << put_result << endl;
    assert (put_result == status_codes::OK);

    put_result = put_entity (GetFixture::addr, GetFixture::table, partition, row, "Song", "Song_name");
    cerr << "put result " << put_result << endl;
    assert (put_result == status_codes::OK);

    put_result = put_entity (GetFixture::addr, GetFixture::table, GetFixture::partition, GetFixture::row, "Home", "Home_name");
    cerr << "put result " << put_result << endl;
    assert (put_result == status_codes::OK);

    pair<status_code,value> result {
      do_request (methods::GET,
      string(GetFixture::addr)
      + read_entity_admin + "/"
      + string(GetFixture::table),
      value::object (vector<pair<string,value>>
        {make_pair("Song", value::string("*")),
        make_pair("Home", value::string("*"))}))};
    
    CHECK(result.second.is_array());
    CHECK_EQUAL(2, result.second.as_array().size());
    CHECK_EQUAL(status_codes::OK, result.first);
    CHECK_EQUAL(status_codes::OK, delete_entity (GetFixture::addr, GetFixture::table, partition, row));
    CHECK_EQUAL(status_codes::OK, delete_entity (GetFixture::addr, GetFixture::table, "Bennett,Chancelor", "USA"));
  }

  /*
    A test of GET by properties when no entities contain the specified property
   */
  TEST_FIXTURE(GetFixture, GetByProp_PropNotFound){
    string partition {"Katherines,The"};
    string row {"Canada"};
    string property {"Home"};
    string prop_val {"Vancouver"};
    int put_result {put_entity (GetFixture::addr, GetFixture::table, partition, row, property, prop_val)};
    cerr << "put result " << put_result << endl;
    assert (put_result == status_codes::OK);

    put_result = put_entity (GetFixture::addr, GetFixture::table, partition, row, "prop", "prop_val");
    cerr << "put result " << put_result << endl;
    assert (put_result == status_codes::OK);

    pair<status_code,value> result {
      do_request (methods::GET,
      string(GetFixture::addr)
      + read_entity_admin + "/"
      + string(GetFixture::table),
      value::object (vector<pair<string,value>>
        {make_pair("Non_existing_property", value::string("*"))}))};

    CHECK(result.second.is_array());
    CHECK_EQUAL(0,result.second.as_array().size());
    CHECK_EQUAL(status_codes::OK, result.first);
    CHECK_EQUAL(status_codes::OK, delete_entity (GetFixture::addr, GetFixture::table, partition, row));
  }

  /*
    A test of GET by properties when the request specifies a table that does not exist
   */
  TEST_FIXTURE(GetFixture, GetByProp_TableNotFound){
    string partition {"Katherines,The"};
    string row {"Canada"};
    string property {"Home"};
    string prop_val {"Vancouver"};
    int put_result {put_entity (GetFixture::addr, GetFixture::table, partition, row, property, prop_val)};
    cerr << "put result " << put_result << endl;
    assert (put_result == status_codes::OK);

    pair<status_code,value> result {
      do_request (methods::GET,
      string(GetFixture::addr)
      + read_entity_admin + "/"
      + "Non-existing-Table",
      value::object (vector<pair<string,value>>
        {make_pair("Random_property", value::string("*"))}))};

    CHECK_EQUAL(status_codes::NotFound, result.first);
    CHECK_EQUAL(status_codes::OK, delete_entity (GetFixture::addr, GetFixture::table, partition, row));
  }

  /*
    A test of GET by properties when multiple entities contain multiple specified properties
   */
  TEST_FIXTURE(GetFixture, GetByProp_SameProps) {
    string partition {"Katherines,The"};
    string row {"Canada"};
    string property {"Home"};
    string prop_val {"Vancouver"};
    int put_result {put_entity (GetFixture::addr, GetFixture::table, partition, row, property, prop_val)};
    cerr << "put result " << put_result << endl;
    assert (put_result == status_codes::OK);

    put_result = put_entity (GetFixture::addr, GetFixture::table, partition, row, "Song", "Song_name");
    cerr << "put result " << put_result << endl;
    assert (put_result == status_codes::OK);

    put_result = put_entity (GetFixture::addr, GetFixture::table, GetFixture::partition, GetFixture::row, "Home", "Home_name");
    cerr << "put result " << put_result << endl;
    assert (put_result == status_codes::OK);

    pair<status_code,value> result {
      do_request (methods::GET,
      string(GetFixture::addr)
      + read_entity_admin + "/"
      + string(GetFixture::table),
      value::object (vector<pair<string,value>>
        {make_pair("Song", value::string("*")),
       make_pair("Home", value::string("*"))}))};
    
    CHECK(result.second.is_array());
    CHECK_EQUAL(2, result.second.as_array().size());
    CHECK_EQUAL(status_codes::OK, result.first);
    CHECK_EQUAL(status_codes::OK, delete_entity (GetFixture::addr, GetFixture::table, partition, row));
  }

  /*
    A test of GET by properties when request does not specify a table name
   */
  TEST_FIXTURE(GetFixture, GetByProp_NoTableName) {
    pair<status_code,value> result {
      do_request (methods::GET,
      string(GetFixture::addr)
      + read_entity_admin + "/",
      value::object (vector<pair<string,value>>
        {}))};
    
    CHECK_EQUAL(status_codes::BadRequest, result.first);
  }

  /*
    A test of GET by properties when request does not specify a JSON object where values are the string "*"
   */
  TEST_FIXTURE(GetFixture, GetByProp_BadJsonParam) {
    pair<status_code,value> result {
      do_request (methods::GET,
      string(GetFixture::addr)
      + read_entity_admin + "/"
      + string(GetFixture::table),
      value::object (vector<pair<string,value>>
        {make_pair("Property", value::string("Bad String"))}))};
    
    CHECK_EQUAL(status_codes::BadRequest, result.first);
  }
}

/*
  Test Suite for optional PUT operations
 */
SUITE(PUT) {
  class PutFixture {
  public:
    static constexpr const char* addr {"http://127.0.0.1:34568/"};
    static constexpr const char* table {"PutTestTable"};
    static constexpr const char* partition {"Franklin,Aretha"};
    static constexpr const char* row {"USA"};
    static constexpr const char* property {"Song"};
    static constexpr const char* prop_val {"RESPECT"};

  public:
    PutFixture() {
      int make_result {create_table(addr, table)};
      cerr << "create result " << make_result << endl;
      if (make_result != status_codes::Created && make_result != status_codes::Accepted) {
  throw std::exception();
      }
      int put_result {put_entity (addr, table, partition, row, property, prop_val)};
      cerr << "put result " << put_result << endl;
      if (put_result != status_codes::OK) {
  throw std::exception();
      }
    }
    ~PutFixture() {
      int del_ent_result {delete_entity (addr, table, partition, row)};
      if (del_ent_result != status_codes::OK) {
  throw std::exception();
      }
      cout << "Skipping table delete" << endl;
    }
  };

  /********Starting Tests for optional operation 1 ********/
  /*
  	A test of PUT property into all entities
   */
  TEST_FIXTURE(PutFixture, PutAll) {
    string partition {"Bennett,Chancelor"};
    string row {"USA"};
    string property {"Home"};
    string prop_val {"Chicago"};
    int put_result {put_entity (PutFixture::addr, PutFixture::table, partition, row, property, prop_val)};
    cerr << "put result " << put_result << endl;
    assert (put_result == status_codes::OK);

    partition = "Katherines,The";
    row = "Canada";
    property = "Home";
    prop_val = "Vancouver";
    put_result = put_entity (PutFixture::addr, PutFixture::table, partition, row, property, prop_val);
    cerr << "put result " << put_result << endl;
    assert (put_result == status_codes::OK);

    pair<status_code,value> result {
      do_request (methods::PUT,
      string(PutFixture::addr)
      + add_property_admin + "/"
      + string(PutFixture::table),
      value::object (vector<pair<string,value>>
        {make_pair("Song", value::string("New_Song"))}))};
    
    CHECK_EQUAL(status_codes::OK, result.first);
    CHECK_EQUAL(status_codes::OK, delete_entity (PutFixture::addr, PutFixture::table, partition, row));
    CHECK_EQUAL(status_codes::OK, delete_entity (PutFixture::addr, PutFixture::table, "Bennett,Chancelor", "USA"));
  }

  /*
    A test  of PUT property into all entities, Table does not exist
   */
  TEST_FIXTURE(PutFixture, PutAll_NonExistingTable) {
    pair<status_code,value> result {
      do_request (methods::PUT,
      string(PutFixture::addr)
      + add_property_admin + "/"
      + "Table_Doesnt_Exist",
      value::object (vector<pair<string,value>>
        {make_pair("Song", value::string("New_Song"))}))};
    
    CHECK_EQUAL(status_codes::NotFound, result.first);
  }

  /*
    A test of PUT property into all entities, missing Table name
   */
  TEST_FIXTURE(PutFixture, PutAll_NoTableName) {
    pair<status_code,value> result {
      do_request (methods::PUT,
      string(PutFixture::addr)
      + add_property_admin + "/",
      value::object (vector<pair<string,value>>
        {make_pair("Song", value::string("New_Song"))}))};
    
    CHECK_EQUAL(status_codes::BadRequest, result.first);
  }

  /*
    A test of PUT property into all entities, missing JSON body
   */
  TEST_FIXTURE(PutFixture, PutAll_NoJSON) {
    pair<status_code,value> result {
      do_request (methods::PUT,
      string(PutFixture::addr)
      + add_property_admin + "/"
      + string(PutFixture::table))};
    
    CHECK_EQUAL(status_codes::BadRequest, result.first);
  }


  /********Starting Tests for optional operation 2 ********/
  /*
  	A test of PUT, updates entities with specified property in request
   */
  TEST_FIXTURE(PutFixture, PutUpdate) {
    string partition {"Katherines,The"};
    string row {"Canada"};
    string property {"Home"};
    string prop_val {"Vancouver"};
    int put_result {put_entity (PutFixture::addr, PutFixture::table, partition, row, property, prop_val)};
    cerr << "put result " << put_result << endl;
    assert (put_result == status_codes::OK);

    put_result = put_entity (PutFixture::addr, PutFixture::table, partition, row, "Song", "Song_name");
    cerr << "put result " << put_result << endl;
    assert (put_result == status_codes::OK);

    pair<status_code,value> result {
      do_request (methods::PUT,
      string(PutFixture::addr)
      + update_property_admin + "/"
      + string(PutFixture::table),
      value::object (vector<pair<string,value>>
        {make_pair("Song", value::string("New_Song"))}))};
    
    CHECK_EQUAL(status_codes::OK, result.first);
    CHECK_EQUAL(status_codes::OK, delete_entity (PutFixture::addr, PutFixture::table, partition, row));
  }

  /*
    A test of PUT update, Table does not exist
   */
  TEST_FIXTURE(PutFixture, PutUpdate_NonExistingTable) {
    pair<status_code,value> result {
      do_request (methods::PUT,
      string(PutFixture::addr)
      + update_property_admin + "/"
      + "Table_Doesnt_Exist",
      value::object (vector<pair<string,value>>
        {make_pair("Song", value::string("New_Song"))}))};
    
    CHECK_EQUAL(status_codes::NotFound, result.first);
  }

  /*
    A test of PUT update, missing Table name
   */
  TEST_FIXTURE(PutFixture, PutUpdate_NoTableName) {
    pair<status_code,value> result {
      do_request (methods::PUT,
      string(PutFixture::addr)
      + update_property_admin + "/",
      value::object (vector<pair<string,value>>
        {make_pair("Song", value::string("New_Song"))}))};
    
    CHECK_EQUAL(status_codes::BadRequest, result.first);
  }

  /*
    A test of PUT update, missing JSON body
   */
  TEST_FIXTURE(PutFixture, PutUpdate_NoJSON) {
    pair<status_code,value> result {
      do_request (methods::PUT,
      string(PutFixture::addr)
      + update_property_admin + "/"
      + string(PutFixture::table))};
    
    CHECK_EQUAL(status_codes::BadRequest, result.first);
  }
}

class AuthFixture {
public:
  static constexpr const char* addr {"http://localhost:34568/"};
  static constexpr const char* auth_addr {"http://localhost:34570/"};
  static constexpr const char* userid {"user"};
  static constexpr const char* user_pwd {"user"};
  static constexpr const char* auth_table {"AuthTable"};
  static constexpr const char* auth_table_partition {"Userid"};
  static constexpr const char* auth_pwd_prop {"Password"};
  static constexpr const char* table {"DataTable"};
  static constexpr const char* partition {"USA"};
  static constexpr const char* row {"Franklin,Aretha"};
  static constexpr const char* property {"Song"};
  static constexpr const char* prop_val {"RESPECT"};

public:
  AuthFixture() {
    int make_result {create_table(addr, table)};
    cerr << "create result " << make_result << endl;
    if (make_result != status_codes::Created && make_result != status_codes::Accepted) {
      throw std::exception();
    }
    int put_result {put_entity (addr, table, partition, row, property, prop_val)};
    cerr << "put result " << put_result << endl;
    if (put_result != status_codes::OK) {
      throw std::exception();
    }

    /********************************************************
      NOTE
      Assumes AuthTable previously created with curl
      Assumes AuthTable entity added with adduser.sh
      {"DataPartition":"USA","DataRow":"Franklin,Aretha","Partition":"Userid","Password":"user","Row":"user"}
    ********************************************************/
    
    //Ensure userid and password in system
    int user_result {put_entity (addr,
                                 auth_table,
                                 auth_table_partition,
                                 userid,
                                 auth_pwd_prop,
                                 user_pwd)};
    cerr << "user auth table insertion result " << user_result << endl;
    if (user_result != status_codes::OK) {
      throw std::exception();
    }
  }

  ~AuthFixture() {
    int del_ent_result {delete_entity (addr, table, partition, row)};
    if (del_ent_result != status_codes::OK) {
      throw std::exception();
    }
  }
};



/* Test Suite for read with authorization */

SUITE(GET_AUTH) {

/* Test of Read Entity with Authorization (GET) */
	
	TEST_FIXTURE(AuthFixture,  GetAuth) {
	cout << "Requesting token" << endl;
	pair<status_code,string> token_res {
	  get_read_token(AuthFixture::auth_addr,
					   AuthFixture::userid,
					   AuthFixture::user_pwd)};
	cout << "Token response " << token_res.first << endl;
	CHECK_EQUAL (token_res.first, status_codes::OK);

	pair<status_code,value> result {
	  do_request (methods::GET,
				  string(AuthFixture::addr)
				  + read_entity_auth + "/"
				  + AuthFixture::table + "/"
				  + token_res.second + "/"
				  + AuthFixture::partition + "/"
				  + AuthFixture::row)};
	CHECK_EQUAL(status_codes::OK, result.first);

	value expect {
	  build_json_object (
						 vector<pair<string,string>> {
						   make_pair(string(AuthFixture::property), 
									 string(AuthFixture::prop_val))}
						 )};
	compare_json_values(expect, result.second);

	}
	
	
/* Test User not found */

	TEST_FIXTURE(AuthFixture,  GetAuth_UserNotFound) {

	cout << "Requesting token" << endl;
	pair<status_code,string> token_res {
	  get_read_token(AuthFixture::auth_addr,
					   "NonExistingUser",
					   AuthFixture::user_pwd)};
	cout << "Token response " << token_res.first << endl;

	CHECK_EQUAL (token_res.first, status_codes::NotFound);
	}
	
	
/* Test Wrong Password */

	TEST_FIXTURE(AuthFixture,  GetAuth_WrongPassword) {

	cout << "Requesting token" << endl;
	pair<status_code,string> token_res {
	  get_read_token(AuthFixture::auth_addr,
					   AuthFixture::userid,
					   "WrongPassword")};
	cout << "Token response " << token_res.first << endl;

	CHECK_EQUAL (token_res.first, status_codes::NotFound);
	}

	
/* Test No Password */

	TEST_FIXTURE(AuthFixture,  GetAuth_EmptyPassword) {

	cout << "Requesting token" << endl;
	pair<status_code,string> token_res {
	  get_read_token(AuthFixture::auth_addr,
					   AuthFixture::userid,
					   "")};
	cout << "Token response " << token_res.first << endl;

	CHECK_EQUAL (token_res.first, status_codes::BadRequest);
	}
	
	
/* Test Table Not Found */

	TEST_FIXTURE(AuthFixture,  GetAuth_TableNotFound) {
	cout << "Requesting token" << endl;
	pair<status_code,string> token_res {
	  get_read_token(AuthFixture::auth_addr,
					   AuthFixture::userid,
					   AuthFixture::user_pwd)};
	cout << "Token response " << token_res.first << endl;
	CHECK_EQUAL (token_res.first, status_codes::OK);

	pair<status_code,value> result {
	  do_request (methods::GET,
				  string(AuthFixture::addr)
				  + read_entity_auth + "/"
				  + "NonExistingTable" + "/"
				  + token_res.second + "/"
				  + AuthFixture::partition + "/"
				  + AuthFixture::row)};
				  
	CHECK_EQUAL(status_codes::NotFound, result.first);
	}

	
/* Test Partition Not Found */

	TEST_FIXTURE(AuthFixture,  GetAuth_PartitionNotFound) {
	cout << "Requesting token" << endl;
	pair<status_code,string> token_res {
	  get_read_token(AuthFixture::auth_addr,
					   AuthFixture::userid,
					   AuthFixture::user_pwd)};
	cout << "Token response " << token_res.first << endl;
	CHECK_EQUAL (token_res.first, status_codes::OK);

	pair<status_code,value> result {
	  do_request (methods::GET,
				  string(AuthFixture::addr)
				  + read_entity_auth + "/"
				  + AuthFixture::table + "/"
				  + token_res.second + "/"
				  + "NonExistingPartition" + "/"
				  + AuthFixture::row)};
				  
	CHECK_EQUAL(status_codes::NotFound, result.first);
	}
	
	
/* Test Row Not Found */

	TEST_FIXTURE(AuthFixture,  GetAuth_RowNotFound) {
	cout << "Requesting token" << endl;
	pair<status_code,string> token_res {
	  get_read_token(AuthFixture::auth_addr,
					   AuthFixture::userid,
					   AuthFixture::user_pwd)};
	cout << "Token response " << token_res.first << endl;
	CHECK_EQUAL (token_res.first, status_codes::OK);

	pair<status_code,value> result {
	  do_request (methods::GET,
				  string(AuthFixture::addr)
				  + read_entity_auth + "/"
				  + AuthFixture::table + "/"
				  + token_res.second + "/"
				  + AuthFixture::partition + "/"
				  + "NonExistingRow")};
				  
	CHECK_EQUAL(status_codes::NotFound, result.first);
	}

	
/* Test - Token did not authorize access to specified entity */

	TEST_FIXTURE(AuthFixture,  GetAuth_WrongEntity) {
	cout << "Requesting token" << endl;
	pair<status_code,string> token_res {
	  get_read_token(AuthFixture::auth_addr,
					   AuthFixture::userid,
					   AuthFixture::user_pwd)};
	cout << "Token response " << token_res.first << endl;
	CHECK_EQUAL (token_res.first, status_codes::OK);

	string partition {"Canada"};
	string row {"Katherines,The"};
	string property {"Home"};
	string prop_val {"Vancouver"};
	int put_result {put_entity (AuthFixture::addr, AuthFixture::table, partition, row, property, prop_val)};
	cerr << "put result " << put_result << endl;
	assert (put_result == status_codes::OK);

	pair<status_code,value> result {
	  do_request (methods::GET,
				  string(AuthFixture::addr)
				  + read_entity_auth + "/"
				  + AuthFixture::table + "/"
				  + token_res.second + "/"
				  + partition + "/"
				  + row)};

	CHECK_EQUAL(status_codes::NotFound, result.first);
	CHECK_EQUAL(status_codes::OK, delete_entity (AuthFixture::addr, AuthFixture::table, partition, row));
	}


	
/* Test less than four parameters */

	TEST_FIXTURE(AuthFixture,  GetAuth_LessThanFourParameters) {
	cout << "Requesting token" << endl;
	pair<status_code,string> token_res {
	  get_read_token(AuthFixture::auth_addr,
					   AuthFixture::userid,
					   AuthFixture::user_pwd)};
	cout << "Token response " << token_res.first << endl;
	CHECK_EQUAL (token_res.first, status_codes::OK);	

	pair<status_code,value> result {
	  do_request (methods::GET,
				  string(AuthFixture::addr)
				  + read_entity_auth + "/"
				  + AuthFixture::table + "/"
				  + token_res.second + "/"
				  + AuthFixture::partition)};

	CHECK_EQUAL(status_codes::BadRequest, result.first);
  }
}



/*
  Test Suite for updating with authorization operations
 */
SUITE(UPDATE_AUTH) {
  /*
    A test of PUT property given update token
   */
  TEST_FIXTURE(AuthFixture,  PutAuth) {
    pair<string,string> added_prop {make_pair(string("born"),string("1942"))};
	//testing Auth Server
    cout << "Requesting token" << endl;
    pair<status_code,string> token_res {
      get_update_token(AuthFixture::auth_addr,
                       AuthFixture::userid,
                       AuthFixture::user_pwd)};
    cout << "Token response " << token_res.first << endl;
    CHECK_EQUAL (token_res.first, status_codes::OK);
    
	
    pair<status_code,value> result {
      do_request (methods::PUT,
                  string(AuthFixture::addr)
                  + update_entity_auth + "/"
                  + AuthFixture::table + "/"
                  + token_res.second + "/"
                  + AuthFixture::partition + "/"
                  + AuthFixture::row,
                  value::object (vector<pair<string,value>>
                                   {make_pair(added_prop.first,
                                              value::string(added_prop.second))})
                  )};
    CHECK_EQUAL(status_codes::OK, result.first);
    
    pair<status_code,value> ret_res {
      do_request (methods::GET,
                  string(AuthFixture::addr)
                  + read_entity_admin + "/"
                  + AuthFixture::table + "/"
                  + AuthFixture::partition + "/"
                  + AuthFixture::row)};
    CHECK_EQUAL (status_codes::OK, ret_res.first);
    value expect {
      build_json_object (
                         vector<pair<string,string>> {
                           added_prop,
                           make_pair(string(AuthFixture::property), 
                                     string(AuthFixture::prop_val))}
                         )};
                             
    compare_json_values (expect, ret_res.second);
  }
  
  /*
	Another simple test for Auth update
  */
  
  TEST_FIXTURE(AuthFixture,  PutAuth2) {
    pair<string,string> added_prop {make_pair(string("born"),string("1942"))};
	
	//creating another entity and putting onto data table
	string partition {"Bennett,Chancelor"};
    string row {"USA"};
    string property {"Home"};
    string prop_val {"Chicago"};
    int put_result {put_entity (AuthFixture::addr, AuthFixture::table, partition, row, property, prop_val)};
    cerr << "put result " << put_result << endl;
    assert (put_result == status_codes::OK);
	
	property = "gender";
    prop_val = "male";
    put_result = put_entity (AuthFixture::addr, AuthFixture::table, partition, row, property, prop_val);
    cerr << "put result " << put_result << endl;
    assert (put_result == status_codes::OK);
	
    row = "EZPZ";
    property = "Password";
    prop_val = "foo";
    put_result = put_entity (AuthFixture::addr, AuthFixture::auth_table, AuthFixture::auth_table_partition, row, property, prop_val);
    cerr << "put result " << put_result << endl;
    assert (put_result == status_codes::OK);

	
    property = "DataPartition";
    prop_val = "Bennett,Chancelor";
    put_result = put_entity (AuthFixture::addr, AuthFixture::auth_table, AuthFixture::auth_table_partition, row, property, prop_val);
    cerr << "put result " << put_result << endl;
    assert (put_result == status_codes::OK);
	
    property = "DataRow";
    prop_val = "USA";
    put_result = put_entity (AuthFixture::addr, AuthFixture::auth_table, AuthFixture::auth_table_partition, row, property, prop_val);
    cerr << "put result " << put_result << endl;
    assert (put_result == status_codes::OK);
	
	//testing Auth Server
    cout << "Requesting token" << endl;
    pair<status_code,string> token_res {
      get_update_token(AuthFixture::auth_addr,
                       row,
                       "foo")};
    cout << "Token response " << token_res.first << endl;
    CHECK_EQUAL (token_res.first, status_codes::OK);
    
	
    pair<status_code,value> result {
      do_request (methods::PUT,
                  string(AuthFixture::addr)
                  + update_entity_auth + "/"
                  + AuthFixture::table + "/"
                  + token_res.second + "/"
                  + partition + "/"
                  + "USA",
                  value::object (vector<pair<string,value>>
                                   {make_pair(added_prop.first,
                                              value::string(added_prop.second))})
                  )};
    CHECK_EQUAL(status_codes::OK, result.first);
    
    pair<status_code,value> ret_res {
      do_request (methods::GET,
                  string(AuthFixture::addr)
                  + read_entity_admin + "/"
                  + AuthFixture::table + "/"
                  + partition + "/"
                  + "USA")};
    CHECK_EQUAL (status_codes::OK, ret_res.first);
    value expect {
      build_json_object (
                         vector<pair<string,string>> {
                           added_prop,
                           make_pair("gender", 
                                     "male"),
              							make_pair("Home",
              									     "Chicago")}
                                      )};
                             
    compare_json_values (expect, ret_res.second);
    CHECK_EQUAL(status_codes::OK, delete_entity (AuthFixture::addr, AuthFixture::table, partition, "USA"));
    CHECK_EQUAL(status_codes::OK, delete_entity (AuthFixture::addr, AuthFixture::auth_table, AuthFixture::auth_table_partition, row));
  }
  
  //Testing wrong password
  TEST_FIXTURE(AuthFixture, PutAuth_WrongPassword){
	  
	  cout << "Requesting token" << endl;
    pair<status_code,string> token_res {
      get_update_token(AuthFixture::auth_addr,
                       AuthFixture::userid,
                       "WrongPassword")};
    cout << "Token response " << token_res.second << endl;
    CHECK_EQUAL (token_res.first, status_codes::NotFound);
  }

  /*
    Testing user not found
   */
  TEST_FIXTURE(AuthFixture,  PutAuth_UserNotFound) {
    cout << "Requesting token" << endl;
    pair<status_code,string> token_res {
      get_update_token(AuthFixture::auth_addr,
                       "NonExistingUser",
                       AuthFixture::user_pwd)};
    cout << "Token response " << token_res.first << endl;
    CHECK_EQUAL (token_res.first, status_codes::NotFound);
  }

  /*
    Testing empty password
   */
  TEST_FIXTURE(AuthFixture,  PutAuth_EmptyPasword) {
    cout << "Requesting token" << endl;
    pair<status_code,string> token_res {
      get_update_token(AuthFixture::auth_addr,
                       AuthFixture::userid,
                       "")};
    cout << "Token response " << token_res.first << endl;
    CHECK_EQUAL (token_res.first, status_codes::BadRequest);
  }

  /*
    Testing when userid is missing from the URI
   */
  TEST_FIXTURE(AuthFixture,  PutAuth_MissingUserid) {
    cout << "Requesting token" << endl;
    string password = AuthFixture::user_pwd;
    value pwd {build_json_object (vector<pair<string,string>> {make_pair("Password", password)})};
    
    pair<status_code,value> token_res {do_request (methods::GET,
                                                   AuthFixture::auth_addr +
                                                   get_update_token_op + "/",
                                                   pwd)};
    cerr << "token " << token_res.second << endl;
    cout << "Token response " << token_res.first << endl;
    
    CHECK_EQUAL (token_res.first, status_codes::BadRequest);
  }

  /*
    Testing message body did not have a property named ‘Password’
   */
  TEST_FIXTURE(AuthFixture,  PutAuth_BadPropName) {
    cout << "Requesting token" << endl;
    string password = AuthFixture::user_pwd;
    value pwd {build_json_object (vector<pair<string,string>> {make_pair("NotPassword", password)})};
    
    pair<status_code,value> token_res {do_request (methods::GET,
                                                   AuthFixture::auth_addr +
                                                   get_update_token_op + "/" +
                                                   AuthFixture::userid,
                                                   pwd)};
    cerr << "token " << token_res.second << endl;
    cout << "Token response " << token_res.first << endl;
    
    CHECK_EQUAL (token_res.first, status_codes::BadRequest);
  }

  /*
    Testing message body included one or more properties other than 'Password'
   */
  TEST_FIXTURE(AuthFixture,  PutAuth_TooMuchProps) {
    cout << "Requesting token" << endl;
    string password = AuthFixture::user_pwd;
    value pwd {build_json_object (vector<pair<string,string>> {make_pair("Password", password),
                                                               make_pair("NotPassword", "AnotherProperty")})};
    
    pair<status_code,value> token_res {do_request (methods::GET,
                                                   AuthFixture::auth_addr +
                                                   get_update_token_op + "/" +
                                                   AuthFixture::userid,
                                                   pwd)};
    cerr << "token " << token_res.second << endl;
    
    cout << "Token response " << token_res.first << endl;
    CHECK_EQUAL (token_res.first, status_codes::BadRequest);
  }

  /*
    Testing when message body has less than four parameters 
   */
  TEST_FIXTURE(AuthFixture,  PutAuth_TooFewParam) {
    pair<string,string> added_prop {make_pair(string("born"),string("1942"))};

    cout << "Requesting token" << endl;
    pair<status_code,string> token_res {
      get_update_token(AuthFixture::auth_addr,
                       AuthFixture::userid,
                       AuthFixture::user_pwd)};
    cout << "Token response " << token_res.first << endl;
    CHECK_EQUAL (token_res.first, status_codes::OK);
    
    pair<status_code,value> result {
      do_request (methods::PUT,
                  string(AuthFixture::addr)
                  + update_entity_auth + "/"
                  + AuthFixture::table + "/"
                  + token_res.second + "/"
                  + AuthFixture::partition + "/",
                  value::object (vector<pair<string,value>>
                                   {make_pair(added_prop.first,
                                              value::string(added_prop.second))})
                  )};
    CHECK_EQUAL(status_codes::BadRequest, result.first);
  }

  /*
    Testing when table not found
   */
  TEST_FIXTURE(AuthFixture,  PutAuth_TableNotFound) {
    pair<string,string> added_prop {make_pair(string("born"),string("1942"))};

    cout << "Requesting token" << endl;
    pair<status_code,string> token_res {
      get_update_token(AuthFixture::auth_addr,
                       AuthFixture::userid,
                       AuthFixture::user_pwd)};
    cout << "Token response " << token_res.first << endl;
    CHECK_EQUAL (token_res.first, status_codes::OK);
    
    pair<status_code,value> result {
      do_request (methods::PUT,
                  string(AuthFixture::addr)
                  + update_entity_auth + "/"
                  + "NonExistingTable"+ "/"
                  + token_res.second + "/"
                  + AuthFixture::partition + "/"
                  + AuthFixture::row,
                  value::object (vector<pair<string,value>>
                                   {make_pair(added_prop.first,
                                              value::string(added_prop.second))})
                  )};
    CHECK_EQUAL(status_codes::NotFound, result.first);
  }

  /*
    Testing when partition not found
   */
  TEST_FIXTURE(AuthFixture,  PutAuth_PartitionNotFound) {
    pair<string,string> added_prop {make_pair(string("born"),string("1942"))};

    cout << "Requesting token" << endl;
    pair<status_code,string> token_res {
      get_update_token(AuthFixture::auth_addr,
                       AuthFixture::userid,
                       AuthFixture::user_pwd)};
    cout << "Token response " << token_res.first << endl;
    CHECK_EQUAL (token_res.first, status_codes::OK);
    
    pair<status_code,value> result {
      do_request (methods::PUT,
                  string(AuthFixture::addr)
                  + update_entity_auth + "/"
                  + AuthFixture::table + "/"
                  + token_res.second + "/"
                  + "NonExistingPartition" + "/"
                  + AuthFixture::row,
                  value::object (vector<pair<string,value>>
                                   {make_pair(added_prop.first,
                                              value::string(added_prop.second))})
                  )};
    CHECK_EQUAL(status_codes::NotFound, result.first);
  }

  /*
    Testing when row not found
   */
  TEST_FIXTURE(AuthFixture,  PutAuth_RowNotFound) {
    pair<string,string> added_prop {make_pair(string("born"),string("1942"))};

    cout << "Requesting token" << endl;
    pair<status_code,string> token_res {
      get_update_token(AuthFixture::auth_addr,
                       AuthFixture::userid,
                       AuthFixture::user_pwd)};
    cout << "Token response " << token_res.first << endl;
    CHECK_EQUAL (token_res.first, status_codes::OK);
    
    pair<status_code,value> result {
      do_request (methods::PUT,
                  string(AuthFixture::addr)
                  + update_entity_auth + "/"
                  + AuthFixture::table + "/"
                  + token_res.second + "/"
                  + AuthFixture::partition + "/"
                  + "NonExistingRow",
                  value::object (vector<pair<string,value>>
                                   {make_pair(added_prop.first,
                                              value::string(added_prop.second))})
                  )};
    CHECK_EQUAL(status_codes::NotFound, result.first);
  }

  /*
    Testing when token did not authorize access to specified entity
   */
  TEST_FIXTURE(AuthFixture,  PutAuth_TokenWrongEntity) {
    pair<string,string> added_prop {make_pair(string("born"),string("1942"))};

    cout << "Requesting token" << endl;
    pair<status_code,string> token_res {
      get_update_token(AuthFixture::auth_addr,
                       AuthFixture::userid,
                       AuthFixture::user_pwd)};
    cout << "Token response " << token_res.first << endl;
    CHECK_EQUAL (token_res.first, status_codes::OK);

    string partition {"Bennett,Chancelor"};
    string row {"USA"};
    string property {"Home"};
    string prop_val {"Chicago"};
    int put_result {put_entity (AuthFixture::addr, AuthFixture::table, partition, row, property, prop_val)};
    cerr << "put result " << put_result << endl;
    assert (put_result == status_codes::OK);
    
    pair<status_code,value> result {
      do_request (methods::PUT,
                  string(AuthFixture::addr)
                  + update_entity_auth + "/"
                  + AuthFixture::table + "/"
                  + token_res.second + "/"
                  + partition + "/"
                  + row,
                  value::object (vector<pair<string,value>>
                                   {make_pair(added_prop.first,
                                              value::string(added_prop.second))})
                  )};
    CHECK_EQUAL(status_codes::NotFound, result.first);
    CHECK_EQUAL(status_codes::OK, delete_entity (AuthFixture::addr, AuthFixture::table, partition, row));
  }

  /*
    Testing when the specified entity exists but the token is only valid for reading, not updating
   */
  TEST_FIXTURE(AuthFixture,  PutAuth_WrongToken) {
    pair<string,string> added_prop {make_pair(string("born"),string("1942"))};

    cout << "Requesting token" << endl;
    pair<status_code,string> bad_token_res {
      get_read_token(AuthFixture::auth_addr,
                       AuthFixture::userid,
                       AuthFixture::user_pwd)};
    cout << "Token response " << bad_token_res.first << endl;
    CHECK_EQUAL (bad_token_res.first, status_codes::OK);
    
    pair<status_code,value> result {
      do_request (methods::PUT,
                  string(AuthFixture::addr)
                  + update_entity_auth + "/"
                  + AuthFixture::table + "/"
                  + bad_token_res.second + "/"
                  + AuthFixture::partition + "/"
                  + AuthFixture::row,
                  value::object (vector<pair<string,value>>
                                   {make_pair(added_prop.first,
                                              value::string(added_prop.second))})
                  )};
    CHECK_EQUAL(status_codes::Forbidden, result.first);
  }
}


class UserFixture {
public:
  static constexpr const char* addr {"http://localhost:34568/"};
  static constexpr const char* user_addr {"http://localhost:34572/"};
  static constexpr const char* auth_addr {"http://localhost:34570/"};

  static constexpr const char* userid {"user"};
  static constexpr const char* user_pwd {"user"};
  static constexpr const char* auth_table {"AuthTable"};
  static constexpr const char* auth_table_partition {"Userid"};
  static constexpr const char* auth_pwd_prop {"Password"};
  static constexpr const char* auth_data_partition_prop {"DataPartition"};
  static constexpr const char* auth_data_row_prop {"DataRow"};

  static constexpr const char* table {"DataTable"};
  static constexpr const char* partition {"USA"};
  static constexpr const char* row {"Franklin,Aretha"};
  static constexpr const char* friends_property {"Friends"};
  static constexpr const char* status_property {"Status"};
  static constexpr const char* updates_property {"Updates"};

public:
  UserFixture() {
    int make_result {create_table(addr, table)};
    cerr << "create result " << make_result << endl;
    if (make_result != status_codes::Created && make_result != status_codes::Accepted) {
      throw std::exception();
    }

    //Initialize user with empty friends, status and updates property
    int put_result {put_entity (addr, table, partition, row, friends_property, "")};
    cerr << "put result " << put_result << endl;
    if (put_result != status_codes::OK) {
      throw std::exception();
    }

    put_result = put_entity (addr, table, partition, row, status_property, "");
    cerr << "put result " << put_result << endl;
    if (put_result != status_codes::OK) {
      throw std::exception();
    }

    put_result = put_entity (addr, table, partition, row, updates_property, "");
    cerr << "put result " << put_result << endl;
    if (put_result != status_codes::OK) {
      throw std::exception();
    }


    /********************************************************
      NOTE
      Assumes AuthTable previously created with curl
      Assumes AuthTable entity added with adduser.sh
      {"DataPartition":"USA","DataRow":"Franklin,Aretha","Partition":"Userid","Password":"user","Row":"user"}
    ********************************************************/
    
    //Ensure userid and password in system
    int user_result {put_entity (addr,
                                 auth_table,
                                 auth_table_partition,
                                 userid,
                                 auth_pwd_prop,
                                 user_pwd)};
    cerr << "user auth table insertion result " << user_result << endl;
    if (user_result != status_codes::OK) {
      throw std::exception();
    }
  }

  ~UserFixture() {
    int del_ent_result {delete_entity (addr, table, partition, row)};
    if (del_ent_result != status_codes::OK) {
      throw std::exception();
    }
  }
};

SUITE(USER_OP) {
  /*
    Simple Test of SignIn and SignOff operation
   */
  TEST_FIXTURE(UserFixture, SignOn_SignOff) {
    //Signing In
    pair<status_code,value> sign_on_result {
      do_request (methods::POST,
            string(UserFixture::user_addr) +
            sign_on_op + "/" +
            UserFixture::userid,
            value::object (vector<pair<string,value>>
                                   {make_pair(string(UserFixture::auth_pwd_prop),
                                              value::string(UserFixture::user_pwd))}))};
    CHECK_EQUAL(status_codes::OK, sign_on_result.first);

    //Signing Off
    pair<status_code,value> sign_off_result {do_request (methods::POST,
                                             string(UserFixture::user_addr) +
                                             sign_off_op + "/" +
                                             UserFixture::userid)};
    CHECK_EQUAL(status_codes::OK, sign_off_result.first);
  }

  /*
    Test of SignOff operation where the specified userid does not have a active session
   */
  TEST_FIXTURE(UserFixture, SignOff_NoSession) {
    pair<status_code,value> sign_off_result {do_request (methods::POST,
                                             string(UserFixture::user_addr) +
                                             sign_off_op + "/" +
                                             "NonActive_userid")};
    CHECK_EQUAL(status_codes::NotFound, sign_off_result.first);
  }

  /*
    Test of SignOn operation where token is recieved but the token refers to a user with no record in DataTable
   */
  TEST_FIXTURE(UserFixture, SignOn_NoRecord) {
    //Add entity to AuthTable where DataPartion and DataRow do not exist in DataTable
    string new_userid {"UserNoRecord"};
    string pwd {"foo"};
    int put_result {put_entity (UserFixture::addr, UserFixture::auth_table, UserFixture::auth_table_partition, new_userid, UserFixture::auth_pwd_prop, pwd)};
    cerr << "put result " << put_result << endl;
    if (put_result != status_codes::OK) {
      throw std::exception();
    }

    put_result = put_entity (UserFixture::addr, UserFixture::auth_table, UserFixture::auth_table_partition, new_userid, UserFixture::auth_data_partition_prop, "NonExistingPartition");
    cerr << "put result " << put_result << endl;
    if (put_result != status_codes::OK) {
      throw std::exception();
    }

    put_result = put_entity (UserFixture::addr, UserFixture::auth_table, UserFixture::auth_table_partition, new_userid, UserFixture::auth_data_row_prop, "NonExistingRow");
    cerr << "put result " << put_result << endl;
    if (put_result != status_codes::OK) {
      throw std::exception();
    }

    pair<status_code,value> sign_on_result {
      do_request (methods::POST,
            string(UserFixture::user_addr) +
            sign_on_op + "/" +
            new_userid,
            value::object (vector<pair<string,value>>
                                   {make_pair(string(UserFixture::auth_pwd_prop),
                                              value::string(pwd))}))};
    CHECK_EQUAL(status_codes::NotFound, sign_on_result.first);

    CHECK_EQUAL(status_codes::OK, delete_entity (UserFixture::addr, UserFixture::auth_table, UserFixture::auth_table_partition, new_userid));
  }

  /*
    Test of SignOn operation where the specified userid does not exist in AuthTable
   */
  TEST_FIXTURE(UserFixture, SignOn_UserNotFound) {
    pair<status_code,value> sign_on_result {
      do_request (methods::POST,
            string(UserFixture::user_addr) +
            sign_on_op + "/" +
            "NonExistingUser",
            value::object (vector<pair<string,value>>
                                   {make_pair(string(UserFixture::auth_pwd_prop),
                                              value::string(UserFixture::user_pwd))}))};
    CHECK_EQUAL(status_codes::NotFound, sign_on_result.first);
  }

  /*
    Test of SignOn operation where the specified password is incorrect
   */
  TEST_FIXTURE(UserFixture, SignOn_WrongPassword) {
    pair<status_code,value> sign_on_result {
      do_request (methods::POST,
            string(UserFixture::user_addr) +
            sign_on_op + "/" +
            UserFixture::userid,
            value::object (vector<pair<string,value>>
                                   {make_pair(string(UserFixture::auth_pwd_prop),
                                              value::string("WrongPassword"))}))};
    CHECK_EQUAL(status_codes::NotFound, sign_on_result.first);
  }

  /*
    Test of SignOn operation where the user is already signed in and attempts to sign in again with the same userid and password
   */
  TEST_FIXTURE(UserFixture, SignOn_CorrectTwice) {
    pair<status_code,value> sign_on_result {
      do_request (methods::POST,
            string(UserFixture::user_addr) +
            sign_on_op + "/" +
            UserFixture::userid,
            value::object (vector<pair<string,value>>
                                   {make_pair(string(UserFixture::auth_pwd_prop),
                                              value::string(UserFixture::user_pwd))}))};
    CHECK_EQUAL(status_codes::OK, sign_on_result.first);

    pair<status_code,value> sign_on_again {
      do_request (methods::POST,
            string(UserFixture::user_addr) +
            sign_on_op + "/" +
            UserFixture::userid,
            value::object (vector<pair<string,value>>
                                   {make_pair(string(UserFixture::auth_pwd_prop),
                                              value::string(UserFixture::user_pwd))}))};
    CHECK_EQUAL(status_codes::OK, sign_on_again.first);

    pair<status_code,value> sign_off_result {do_request (methods::POST,
                                             string(UserFixture::user_addr) +
                                             sign_off_op + "/" +
                                             UserFixture::userid)};
    CHECK_EQUAL(status_codes::OK, sign_off_result.first);
  }

  /*
    Test of SignOn operation where the user is already signed in and makes an unsuccessful attempt to sign in 
   */
  TEST_FIXTURE(UserFixture, SignOn_InCorrectTwice) {
    pair<status_code,value> sign_on_result {
      do_request (methods::POST,
            string(UserFixture::user_addr) +
            sign_on_op + "/" +
            UserFixture::userid,
            value::object (vector<pair<string,value>>
                                   {make_pair(string(UserFixture::auth_pwd_prop),
                                              value::string(UserFixture::user_pwd))}))};
    CHECK_EQUAL(status_codes::OK, sign_on_result.first);

    pair<status_code,value> sign_on_again {
      do_request (methods::POST,
            string(UserFixture::user_addr) +
            sign_on_op + "/" +
            UserFixture::userid,
            value::object (vector<pair<string,value>>
                                   {make_pair(string(UserFixture::auth_pwd_prop),
                                              value::string("WrongPassword"))}))};
    CHECK_EQUAL(status_codes::NotFound, sign_on_again.first);

    pair<status_code,value> sign_off_result {do_request (methods::POST,
                                             string(UserFixture::user_addr) +
                                             sign_off_op + "/" +
                                             UserFixture::userid)};
    CHECK_EQUAL(status_codes::OK, sign_off_result.first);
  }

  /*
    Test of ReadFriendList operation
   */
  TEST_FIXTURE(UserFixture, ReadFriendList) {
    //Add friends to USA/Franklin,Aretha entity in DataTable, Note that this doesn't use AddFriend operation for simplicity
    string new_friends {"USA;Shinoda,Mike|Canada;Edwards,Kathleen|Korea;Bae,Doona"};
    int put_result {put_entity (UserFixture::addr, UserFixture::table, UserFixture::partition, UserFixture::row, UserFixture::friends_property, new_friends)};
    cerr << "put result " << put_result << endl;
    if (put_result != status_codes::OK) {
      throw std::exception();
    }

    //Sign on
    pair<status_code,value> sign_on_result {
            do_request (methods::POST,
                  string(UserFixture::user_addr) +
                  sign_on_op + "/" +
                  UserFixture::userid,
                  value::object (vector<pair<string,value>>
                                         {make_pair(string(UserFixture::auth_pwd_prop),
                                                    value::string(UserFixture::user_pwd))}))};
    CHECK_EQUAL(status_codes::OK, sign_on_result.first);

    //Get user's friend list
    pair<status_code,value> read_result {do_request (methods::GET,
                                                     string(UserFixture::user_addr) +
                                                     read_friend_list_op + "/" +
                                                     UserFixture::userid)};
    CHECK_EQUAL(status_codes::OK, read_result.first);

    value expect {
      build_json_object (vector<pair<string,string>> {
                             make_pair(string(UserFixture::friends_property), 
                                       new_friends)})};      
    compare_json_values (expect, read_result.second);

    //Sign off
    pair<status_code,value> sign_off_result {do_request (methods::POST,
                                             string(UserFixture::user_addr) +
                                             sign_off_op + "/" +
                                             UserFixture::userid)};
    CHECK_EQUAL(status_codes::OK, sign_off_result.first);
  }

  /*
    Test of ReadFriendList operation when userid does not have an active session (is not signed in)
   */
  TEST_FIXTURE(UserFixture, ReadFriendList_Unactive){
    pair<status_code,value> read_result {do_request (methods::GET,
                                                     string(UserFixture::user_addr) +
                                                     read_friend_list_op + "/" +
                                                     UserFixture::userid)};
    CHECK_EQUAL(status_codes::Forbidden, read_result.first);
  }

  /*
    Extensive test of UpdateStatus and PushStatus operation
   */
  TEST_FIXTURE(UserFixture, UpdateStatus) {
    //Add entity to DataTable where Partion="Canada", Row="Reynolds,Ryan", Friends="", Status="", Updates=""
    //Note that friends are added without the AddFriend operation for simplicity
    string new_partition {"Canada"};
    string new_row {"Reynolds,Ryan"};
    int put_result {put_entity (UserFixture::addr, UserFixture::table, new_partition, new_row, UserFixture::friends_property, "")};
    cerr << "put result " << put_result << endl;
    if (put_result != status_codes::OK) {
      throw std::exception();
    }
    put_result = put_entity (UserFixture::addr, UserFixture::table, new_partition, new_row, UserFixture::status_property, "");
    cerr << "put result " << put_result << endl;
    if (put_result != status_codes::OK) {
      throw std::exception();
    }
    put_result = put_entity (UserFixture::addr, UserFixture::table, new_partition, new_row, UserFixture::updates_property, "");
    cerr << "put result " << put_result << endl;
    if (put_result != status_codes::OK) {
      throw std::exception();
    }

    //Add entity to DataTable where Partion="USA", Row="Curry,Stephen", Friends="", Status="", Updates=""
    //Note that friends are added without the AddFriend operation for simplicity
    new_partition = "USA";
    new_row = "Curry,Stephen";
    put_result = put_entity (UserFixture::addr, UserFixture::table, new_partition, new_row, UserFixture::friends_property, "");
    cerr << "put result " << put_result << endl;
    if (put_result != status_codes::OK) {
      throw std::exception();
    }
    put_result = put_entity (UserFixture::addr, UserFixture::table, new_partition, new_row, UserFixture::status_property, "");
    cerr << "put result " << put_result << endl;
    if (put_result != status_codes::OK) {
      throw std::exception();
    }
    put_result = put_entity (UserFixture::addr, UserFixture::table, new_partition, new_row, UserFixture::updates_property, "");
    cerr << "put result " << put_result << endl;
    if (put_result != status_codes::OK) {
      throw std::exception();
    }

    //Adding Friends to fixture entity in DataTable, Note that friends are added without the AddFriend operation for simplicity
    string friend_list {"Canada;Reynolds,Ryan|USA;Curry,Stephen"};
    put_result = put_entity (UserFixture::addr, UserFixture::table, UserFixture::partition, UserFixture::row, UserFixture::friends_property, friend_list);
    cerr << "put result " << put_result << endl;
    if (put_result != status_codes::OK) {
      throw std::exception();
    }

    //Sign on
    pair<status_code,value> sign_on_result {
            do_request (methods::POST,
                  string(UserFixture::user_addr) +
                  sign_on_op + "/" +
                  UserFixture::userid,
                  value::object (vector<pair<string,value>>
                                         {make_pair(string(UserFixture::auth_pwd_prop),
                                                    value::string(UserFixture::user_pwd))}))};
    CHECK_EQUAL(status_codes::OK, sign_on_result.first);

    //Update status of fixture entity
    string new_status {"Happy"};
    pair<status_code,value> update_result {do_request (methods::PUT,
                                                       string(UserFixture::user_addr) +
                                                       update_status_op + "/" +
                                                       UserFixture::userid + "/" + 
                                                       new_status)};
    CHECK_EQUAL(status_codes::OK, update_result.first);

    pair<status_code,value> get_result {do_request (methods::GET,
                                                    string(UserFixture::addr) +
                                                    read_entity_admin + "/" +
                                                    UserFixture::table + "/" + 
                                                    UserFixture::partition + "/" + 
                                                    UserFixture::row)};
    CHECK_EQUAL (status_codes::OK, get_result.first);

    value expect {
      build_json_object (vector<pair<string,string>> {
                               make_pair(string(UserFixture::friends_property), friend_list),
                               make_pair(string(UserFixture::status_property), new_status),
                               make_pair(string(UserFixture::updates_property), "")}
                         )};
    compare_json_values (expect, get_result.second);

    //Update status of fixture entity again
    new_status = "Sad";
    update_result = do_request (methods::PUT,
                                string(UserFixture::user_addr) +
                                update_status_op + "/" +
                                UserFixture::userid + "/" + 
                                new_status);
    CHECK_EQUAL(status_codes::OK, update_result.first);

    get_result = do_request (methods::GET,
                             string(UserFixture::addr) +
                             read_entity_admin + "/" +
                             UserFixture::table + "/" + 
                             UserFixture::partition + "/" + 
                             UserFixture::row);

    expect = build_json_object (vector<pair<string,string>> {
                                       make_pair(string(UserFixture::friends_property), friend_list),
                                       make_pair(string(UserFixture::status_property), new_status),
                                       make_pair(string(UserFixture::updates_property), "")});
    compare_json_values (expect, get_result.second);

    //Check if updated status is pushed to friends
    get_result = do_request (methods::GET,
                             string(UserFixture::addr) +
                             read_entity_admin + "/" +
                             UserFixture::table + "/" + 
                             "Canada" + "/" + 
                             "Reynolds,Ryan");

    expect = build_json_object (vector<pair<string,string>> {
                                       make_pair(string(UserFixture::friends_property), ""),
                                       make_pair(string(UserFixture::status_property), ""),
                                       make_pair(string(UserFixture::updates_property), "Happy\nSad\n")});
    compare_json_values (expect, get_result.second);

    get_result = do_request (methods::GET,
                             string(UserFixture::addr) +
                             read_entity_admin + "/" +
                             UserFixture::table + "/" + 
                             "USA" + "/" + 
                             "Curry,Stephen");

    expect = build_json_object (vector<pair<string,string>> {
                                       make_pair(string(UserFixture::friends_property), ""),
                                       make_pair(string(UserFixture::status_property), ""),
                                       make_pair(string(UserFixture::updates_property), "Happy\nSad\n")});
    compare_json_values (expect, get_result.second);
    
    //Sign off
    pair<status_code,value> sign_off_result {do_request (methods::POST,
                                             string(UserFixture::user_addr) +
                                             sign_off_op + "/" +
                                             UserFixture::userid)};
    CHECK_EQUAL(status_codes::OK, sign_off_result.first);

    CHECK_EQUAL(status_codes::OK, delete_entity (UserFixture::addr, UserFixture::table, "Canada", "Reynolds,Ryan"));
    CHECK_EQUAL(status_codes::OK, delete_entity (UserFixture::addr, UserFixture::table, "USA", "Curry,Stephen"));
  }

  /*
    Test of UpdateStatus operation when userid does not have an active session (is not signed in)
   */
  TEST_FIXTURE(UserFixture, Update_Unactive){
    pair<status_code,value> update_result {do_request (methods::PUT,
                                                       string(UserFixture::user_addr) +
                                                       update_status_op + "/" +
                                                       UserFixture::userid + "/" + 
                                                       "status")};
    CHECK_EQUAL(status_codes::Forbidden, update_result.first);
  }
}
